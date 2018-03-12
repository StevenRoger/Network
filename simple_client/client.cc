#include "../comm/comm.h"

#define MAXREQUESTCOUNT 100000

static int g_total = 0;
static int g_max_total = 0;

void* send_data(void* arg) {
	int wait_time;
	int client_sock;
	wait_time = *(int*)arg;
	
	while (__sync_fetch_and_add(&g_total, 1) < g_max_total) {
		usleep(wait_time);
		client_sock = make_client_socket();
		connect_server(client_sock);
		request_add(1);
		set_block_filedes_timeout(client_sock);
		if (0 == client_write(client_sock)) {
			client_read(client_sock);
		}
		close(client_sock);
		client_sock = 0;
	}
}

int main(int argc, char* argv[]) 
{
	int thread_count;
	int index;
	int err;
	int wait_time;
	pthread_t thread_id;

	if (argc < 3) {
		fprintf(stderr, "error! example: client 10 50\n");
		return 0;
	}

	err = init_print_thread();
	if (err < 0) {
		perror("create print thread error");
		exit(EXIT_FAILURE);
	}

	thread_count = atoi(argv[1]);
	wait_time = atoi(argv[2]);
	
	g_max_total = MAXREQUESTCOUNT;
	if (argc > 3) {
		g_max_total = atoi(argv[3]);
	}	

	for (index = 0; index < thread_count; index++) {
		err = pthread_create(&thread_id, NULL, send_data, &wait_time);
		if (err != 0) {
			perror("can't create send thread");
			exit(EXIT_FAILURE);
		}
		
	}

	wait_print_thread();
	return 0;
}
