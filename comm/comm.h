#ifndef _COMM_H_
#define _COMM_H_

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <arpa/inet.h>


void request_add(int count);//记录请求次数

int init_print_thread();//打印统计信息的线程

void wait_print_thread();//等待该打印结果的线程退出，用来阻塞主线程

void set_nonblock(int fd);

int make_socket(int asyn);

void set_block_filedes_timeout(int filedes);

int client_write(int filedes);

int server_write(int filedes);

int client_read(int filedes);

int server_read(int filedes);

void* print_count(void* arg);//打印统计信息的函数

int make_client_socket();

void connect_server(int client_sock);

int read_data(int filedes, int from_server);

int write_data(int filedes, int from_server);

#endif