#include "../comm/comm.h"
#include <sys/poll.h>

#define FDS_COUNT 40960


int deal_socket_count() {
	return 2;
}

int total_loop_count() {
	return 1;
}

int 
main(void) {
	int listen_sock;
	struct pollfd fds[FDS_COUNT];
	int timeout;
	int cur_fds_count;
	int rc;
	int index;
	int expect_events;
	int error_events;

	int err;

#ifdef TEST_PERF
	init_gperftools("poll_server.prof");
#endif

	err = init_print_thread();
	if (err < 0) {
		perror("create print thread error");
		exit(EXIT_FAILURE);
	}

	memset(fds, 0, sizeof(fds));

	/* Create the socket and set it up to accept connections. */
	listen_sock = make_socket(1);
	if (listen(listen_sock, SOMAXCONN) < 0) {
		perror("listen error");
		exit(EXIT_FAILURE);
	}
	//set_nonblock(listen_sock);

	error_events = POLLERR | POLLNVAL;
	//expect_events = POLLIN | POLLOUT;
	expect_events = POLLIN;
	
	fds[0].fd = listen_sock;
	fds[0].events = POLLIN;
	cur_fds_count = 1;

	timeout = (500);	

	while (1) {
		rc = poll(fds, cur_fds_count, timeout);
		if (rc < 0) {
			perror("poll error\n");
			exit(EXIT_FAILURE);
		};
		if (rc == 0) {
			continue;
			//perror("poll timeout\n");
		};
		
		int cur_fds_count_temp = cur_fds_count;
		for (index = 0; index < cur_fds_count_temp; ++index) {
			total_loop_count();
			if (fds[index].revents == 0) {
				continue;
			}
			if (fds[index].revents & error_events) {
				perror("revents error");
				if (fds[index].fd == listen_sock) {
					perror("listen sock error");
					exit(EXIT_FAILURE);
				}
				else {
					close(fds[index].fd);
					cur_fds_count--;
					if (index < cur_fds_count) {
						memcpy(&fds[index], &fds[cur_fds_count], sizeof(fds[cur_fds_count]));
						memset(&fds[cur_fds_count], 0, sizeof(fds[cur_fds_count]));
						index--;
					}
					cur_fds_count_temp--;
					continue;
				}	
			}
			if (!(fds[index].revents & fds[index].events)) {
				continue;
			}
			deal_socket_count();
			if (fds[index].fd == listen_sock) {
				int new_sock;
				new_sock = accept(listen_sock, NULL, NULL);
				if (new_sock < 0) {
					//perror("accept error");
					if (errno != EWOULDBLOCK) {
						continue;
					}
					exit(EXIT_FAILURE);
				}
				else {
					request_add(1);
					//set_block_filedes_timeout(new_sock);
					if (cur_fds_count + 1 < sizeof(fds)) {
						fds[cur_fds_count].fd = new_sock;
						fds[cur_fds_count].events = expect_events;
						cur_fds_count++;
					}
				}
			}
			else {
				if (0 == server_read(fds[index].fd)) {
					server_write(fds[index].fd);
				}
				//close(fds[index].fd);
				//cur_fds_count--;
				//if (index < cur_fds_count) {
				//	memcpy(&fds[index], &fds[cur_fds_count], sizeof(fds[cur_fds_count]));
				//	memset(&fds[cur_fds_count], 0, sizeof(fds[cur_fds_count]));
				//	index--;
				//}
				//cur_fds_count_temp--;
			}
		}
	}
	return 0;
}
