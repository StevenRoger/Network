#include "../comm/comm.h"


void deal_socket(int index, int listen_sock, fd_set* active_fd_set, fd_set* read_fd_set) 
{
	if (listen_sock == index) {
		/* Connection request on original socket. */
		int new_sock;
		new_sock = accept(listen_sock, NULL, NULL);
		if (new_sock < 0) {
			perror("accept error");
			exit(EXIT_FAILURE);
		}
		request_add(1);
		FD_SET(new_sock, active_fd_set);
	} else {
		if (0 == server_read(index)) {
			server_write(index);
		}
		close(index);
		FD_CLR(index, active_fd_set);
	}
}

void deal_request(int listen_sock, fd_set* active_fd_set, fd_set* read_fd_set) 
{
	int index = 0;
	for (index = 0; index < FD_SETSIZE; ++index) {
		if (FD_ISSET(index, read_fd_set)) {
			deal_socket(index, listen_sock, active_fd_set, read_fd_set);
		}
	}
}

int main(void) 
{
	int listen_sock;
	fd_set active_fd_set, read_fd_set;
	int index;
	struct timeval timeout;
	int err;

	err = init_print_thread();
	if (err < 0) {
		perror("create print thread error");
		exit(EXIT_FAILURE);
	}

#ifdef TEST_PERF
	init_gperftools("select_server.prof");
#endif


	/* Create the socket and set it up to accept connections. */
	listen_sock = make_socket(1);
	if (listen(listen_sock, SOMAXCONN) < 0) {
		perror("listen error");
		exit(EXIT_FAILURE);
	}
	FD_ZERO(&active_fd_set);
	FD_SET(listen_sock, &active_fd_set);

	/* Initialize the set of active sockets. */
	while (1) {
		timeout.tv_sec = 0;
		timeout.tv_usec = 500;

		/* Service all the sockets with input pending. */
		read_fd_set = active_fd_set;
		switch(select(FD_SETSIZE, &read_fd_set, NULL, NULL, &timeout)) {
			case -1 : {
				perror("select error\n");
				exit(EXIT_FAILURE);
			}break;
			case 0 : {
				//perror("select timeout\n");
			}break;
			default: {
				deal_request(listen_sock, &active_fd_set, &read_fd_set);
			}
		}
	}
	return 0;
}
