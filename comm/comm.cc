#include "comm.h"
#include <signal.h> 
#include <unistd.h>


#define PORT 5555
#define MAXMSG 512
#define MINMSG 8

#define READ_TIMEOUT_S 3
#define READ_TIMEOUT_US 0

#define WRITE_TIMEOUT_S 3
#define WRITE_TIMEOUT_US 0

static int g_request_count = 0;//记录总请求数

static int g_server_suc = 0;//读取客户端成功且发送回包成功次数
static int g_client_suc = 0;//客户端发送包成功且读取服务器回包成功次数
static int g_read_suc = 0;//1 服务器读取客户端请求包成功； 2 客户端读取服务器回包成功
static int g_write_suc = 0;//1 客户端向服务器发送请求包成功； 2 服务器向客户端回包成功

static int g_server_fai = 0;//服务端 1 读取客户端失败；2 读取客户端成功但是发送回包失败
static int g_client_fai = 0;//客户端 1 发送包失败； 2 发送包成功但是接收服务器回包失败
static int g_read_fai = 0;//1 服务器读取客户端请求包失败； 2 客户端读取服务器回包失败
static int g_write_fai = 0;//1 客户端向服务器发送请求包失败； 2 服务器向客户端回包失败

#define WAIT_COUNT_MAX	3

const char* g_client_send = "client request";
const char* g_server_answer = "server answer";

pthread_t g_print_thread;

#ifdef TEST_PERF

void my_handler(int s)
{  
	ProfilerStop();
	exit(1);   
} 
 
void init_gperftools(const char* name) 
{
	struct sigaction sigIntHandler;  
  
	sigIntHandler.sa_handler = my_handler;  
	sigemptyset(&sigIntHandler.sa_mask);  
	sigIntHandler.sa_flags = 0;  
	sigaction(SIGINT, &sigIntHandler, NULL);  
	
	ProfilerStart(name); 
}
#endif

inline const char* get_client_send_ptr(int index) 
{
	return g_client_send + index;
}

inline int get_client_send_len(int length) 
{
	return strlen(g_client_send) - length;
}

inline int is_client_send_finish(int length) 
{
	return strlen(g_client_send) == length ? 1 : 0;
}

inline int is_client_recv_finish(int length) 
{
	return strlen(g_server_answer) == length ? 1 : 0;
}

inline const char* get_server_send_ptr(int index) 
{
	return g_server_answer + index;
}

inline int get_server_send_len(int length) 
{
	return strlen(g_server_answer) - length;
}

inline int is_server_send_finish(int length) 
{
	return strlen(g_server_answer) == length ? 1 : 0;
}

inline int is_server_recv_finish(int length) 
{
	return strlen(g_client_send) == length ? 1 : 0;
}

int init_print_thread() 
{
	return pthread_create(&g_print_thread, NULL, print_count, NULL);
}

void wait_print_thread() 
{
	pthread_join(g_print_thread, NULL);
}

void request_add(int count) 
{
	__sync_fetch_and_add(&g_request_count, count);//原子操作
}

int is_nonblock(int fd) 
{
	int flags = fcntl(fd, F_GETFL);
	if (flags == -1) {
		perror("get fd flags error");
		exit(EXIT_FAILURE);
	}
	return (flags & O_NONBLOCK) ? 1 : 0;
}

void set_nonblock(int fd) 
{
	int flags = fcntl(fd, F_GETFL);
	if (flags == -1) {
		perror("get fd flags error");
		exit(EXIT_FAILURE);
	}
	flags |= O_NONBLOCK;
	flags = fcntl(fd, F_SETFL, flags);
	if (flags == -1) {
		perror("set fd flags error");
		exit(EXIT_FAILURE);
	}
}

//asyn: 0、同步   1、异步
int make_socket(int asyn) 
{
	int listen_sock = -1;
	int rc = -1;
	int on = 1;
	struct sockaddr_in name;
	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock < 0) {
		perror("create socket error");
		exit(EXIT_FAILURE);
	}

	rc = setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));
	if (rc < 0) {
		perror("setsockopt error");
		exit(EXIT_FAILURE);
	}
	
	if (asyn) {
		rc = ioctl(listen_sock, FIONBIO, (char*)&on);//FIONBIO：非阻塞模式（异步）
		if (rc < 0) {
			perror("ioctl failed");
			exit(EXIT_FAILURE);
		}
	}

	name.sin_family = AF_INET;
	name.sin_port = htons(PORT);
	name.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(listen_sock, (struct sockaddr*)&name, sizeof(name)) < 0) {
		perror("bind error");
		exit(EXIT_FAILURE);
	}
	return listen_sock;
}

void set_block_filedes_timeout(int filedes) 
{
 	struct timeval tv_out, tv_in;

	tv_in.tv_sec = READ_TIMEOUT_S;
    	tv_in.tv_usec = READ_TIMEOUT_US;
	if (setsockopt(filedes, SOL_SOCKET, SO_RCVTIMEO, &tv_in, sizeof(tv_in)) < 0) {
		perror("set rcv timeout error");
		exit(EXIT_FAILURE);
	}
	
	tv_out.tv_sec = WRITE_TIMEOUT_S;
    	tv_out.tv_usec = WRITE_TIMEOUT_US;
	if (setsockopt(filedes, SOL_SOCKET, SO_SNDTIMEO, &tv_out, sizeof(tv_out)) < 0) {
		perror("set rcv timeout error");
		exit(EXIT_FAILURE);
	}
}

int server_write(int filedes) 
{
	return write_data(filedes, 1);
}

int client_write(int filedes) 
{
	return write_data(filedes, 0);
}

int write_data(int filedes, int from_server) 
{
	int nbytes;
	int total_len_send;
	int wait_count = 0;
	int index;
	int send_suc = 0;

	total_len_send = 0;
	index = 0;
	while (1) {
		if (from_server) {
			nbytes = send(filedes, get_server_send_ptr(index), get_server_send_len(index), 0);
		}
		else {
			nbytes = send(filedes, get_client_send_ptr(index), get_client_send_len(index), 0);
		}

		if (nbytes < 0) {
			if (is_nonblock(filedes)) {
				if (EAGAIN == errno || EWOULDBLOCK == errno || EINTR == errno) {
					if (wait_count < WAIT_COUNT_MAX) {
						wait_count++;
						usleep(wait_count);
						continue;
					}
				}
			}
			break;
		}
		else if (nbytes == 0) {
			break;
		}
		else if (nbytes > 0) {
			total_len_send += nbytes;
		}
		
		if ((from_server && is_server_send_finish(total_len_send))
			|| (!from_server && is_client_send_finish(total_len_send))) {
			send_suc = 1;
			break;
		}
	}
	
	if (from_server) {
		if (send_suc) {
			__sync_fetch_and_add(&g_write_suc, 1);
			__sync_fetch_and_add(&g_server_suc, 1);
			return 0;
		} else {
			__sync_fetch_and_add(&g_write_fai, 1);
			__sync_fetch_and_add(&g_server_fai, 1);
			return -1;
		}
	} else {
		if (send_suc) {
			__sync_fetch_and_add(&g_write_suc, 1);
			return 0;
		} else {
			__sync_fetch_and_add(&g_write_fai, 1);
			__sync_fetch_and_add(&g_client_fai, 1);
			return -1;
		}
	}
}

int client_read(int filedes) 
{
	return read_data(filedes, 0);
}

int server_read(int filedes) 
{
	return read_data(filedes, 1);
}

//from_server 0:client   1:server
int read_data(int filedes, int from_server) 
{
	char buffer[MAXMSG];
	int nbytes;
	int total_len_recv;
	int wait_count = 0;
	int rec_suc = 0;
	total_len_recv = 0;
	
	while (1) {
		nbytes = recv(filedes, buffer, sizeof(buffer) - 1, 0);
		if (nbytes < 0) {
			if (is_nonblock(filedes)) {
				if (EAGAIN == errno || EWOULDBLOCK == errno || EINTR == errno) {
					if (wait_count < WAIT_COUNT_MAX) {
						wait_count++;
						usleep(wait_count);
						continue;
					}
				}
			}
			break;
		}
		if (nbytes == 0) {
			//fprintf(stderr, "read end\n");
			break;
		}
		else if (nbytes > 0) {
			total_len_recv += nbytes;
			//buffer[nbytes] = 0;
			//fprintf(stderr, "%s", buffer);
		}

		if ((from_server && is_server_recv_finish(total_len_recv))
			|| (!from_server && is_client_recv_finish(total_len_recv))) {
			rec_suc = 1;
			break;
		}
	}
	

	if (from_server) {
		if (rec_suc) {
			__sync_fetch_and_add(&g_read_suc, 1);
			return 0;
		} else {
			__sync_fetch_and_add(&g_read_fai, 1);
			__sync_fetch_and_add(&g_server_fai, 1);
			return -1;
		}
	} else {
		if (rec_suc) {
			__sync_fetch_and_add(&g_read_suc, 1);
			__sync_fetch_and_add(&g_client_suc, 1);
			return 0;
		} else {
			__sync_fetch_and_add(&g_read_fai, 1);
			__sync_fetch_and_add(&g_client_fai, 1);
			return -1;
		}
	}

}

void* print_count(void* arg) 
{
	struct timeval cur_time;
	int index = 0;
	fprintf(stderr, "index\tseconds_micro_seconds\tac\tst\tsr\tsw\tft\tfr\tfw\n");
	while (1) {
		sleep(1);
		gettimeofday(&cur_time, NULL);
		fprintf(stderr, "%d\t%ld\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", 
				index, 
				cur_time.tv_sec * 1000000 + cur_time.tv_usec, 
				g_request_count, 
				g_server_suc > g_client_suc ? g_server_suc : g_client_suc, 
				g_read_suc, 
				g_write_suc, 
				g_server_fai > g_client_fai ? g_server_fai : g_client_fai, 
				g_read_fai,
				g_write_fai);
		index++;
	}
}

int make_client_socket() 
{
	int client_sock = -1;
	struct sockaddr_in client_addr;
	
	client_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (client_sock < 0) {
		perror("create socket error");
		exit(EXIT_FAILURE);
	}

	bzero(&client_addr, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	client_addr.sin_addr.s_addr = htons(INADDR_ANY);
	client_addr.sin_port = htons(0);
	if (bind(client_sock, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
		perror("bind error");
		exit(EXIT_FAILURE);
	}
	return client_sock;
}

void connect_server(int client_sock) 
{
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	if (inet_aton("127.0.0.1", &server_addr.sin_addr) == 0) {
		perror("set server ip error");
		exit(EXIT_FAILURE);
	}
	server_addr.sin_port = htons(PORT);
	if (connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		perror("client connect server error");
		exit(EXIT_FAILURE);
	}
}
