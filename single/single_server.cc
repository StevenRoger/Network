#include "../comm/comm.h"

int main(void) 
{
	int listen_sock;
	pthread_t print_thread;
	int err;
	
#ifdef TEST_PERF
	init_gperftools("block_server.prof");
#endif

	err = init_print_thread();
	if (err < 0) {
		perror("create print thread error");
		exit(EXIT_FAILURE);
	}

	listen_sock = make_socket(0);
	if (listen(listen_sock, SOMAXCONN) < 0) {
		perror("listen error");
		exit(EXIT_FAILURE);
	}

	while (1) {
		int new_sock;
		new_sock = accept(listen_sock, NULL, NULL);
		if (new_sock < 0) {
			perror("accept error");
			exit(EXIT_FAILURE);
		}
		request_add(1);
		set_block_filedes_timeout(new_sock);
		if (0 == server_read(new_sock)) {
			server_write(new_sock);
		}
		close(new_sock);
	}
	return 0;
}