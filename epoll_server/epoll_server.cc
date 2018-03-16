#include "../comm/comm.h"
#include <sys/epoll.h>

#define SOCKET_LIST_COUNT 40960


int deal_socket_count() 
{
	return 2;
}

int total_loop_count() 
{
	return 1;
}

int main(void) 
{
	struct epoll_event ev, events[SOCKET_LIST_COUNT];
	int epfd, nfds;
	int listen_sock;
	int index;
	int in_events, out_events;

	int err;

	err = init_print_thread();
	if (err < 0) {
		perror("create print thread error");
		exit(EXIT_FAILURE);
	}

	epfd = epoll_create1(0);//创建epoll描述符
	in_events = EPOLLIN | EPOLLET;//读事件  边缘触发
	out_events = EPOLLOUT | EPOLLET;//写事件  边缘触发
	
	/* Create the socket and set it up to accept connections. */
	listen_sock = make_socket(1);
	ev.data.fd = listen_sock;
	ev.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, listen_sock, &ev);//

	if (listen(listen_sock, SOMAXCONN) < 0) {
		perror("listen error");
		exit(EXIT_FAILURE);
	}

	while (1) {
		nfds = epoll_wait(epfd, events, sizeof(events), 500);
		for (index = 0; index < nfds; ++index) {
			total_loop_count();
			int fd = events[index].data.fd; 
			if (fd == listen_sock) {
				int new_sock;
				deal_socket_count();
				new_sock = accept(fd, NULL, NULL);
				if (new_sock < 0) {
					if (errno == EMFILE) {
					}
					else {
						perror("accept error");
						//if (errno != EWOULDBLOCK) {
						//	continue;
						//}
						exit(EXIT_FAILURE);
					}
				}
				else {
					request_add(1);
					//set_block_filedes_timeout(new_sock);
					ev.data.fd = new_sock;
					ev.events = in_events;
					epoll_ctl(epfd, EPOLL_CTL_ADD, new_sock, &ev);
				}
			}
			else {
				if (events[index].events & EPOLLIN) {
					deal_socket_count();
					if (0 != server_read(fd)) {
						close(fd);
						epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
					}
					else {
						events[index].events = out_events;
						epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &events[index]);
					}
				}
				else if (events[index].events & EPOLLOUT) {
					deal_socket_count();
					server_write(fd);
					events[index].events = in_events;
					epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &events[index]);
					//close(fd);
					//epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
				}
			}
		}
	}
	close(epfd);
	return 0;
}
