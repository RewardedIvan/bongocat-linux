#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "server.h"
#include "shared.h"

#define MAX_CLIENTS 3

int clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int running = 1;
char socket_path[108] = {0};
pthread_t server_tid = -1;

void add_client(int client_fd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == 0) {
            clients[i] = client_fd;
            client_count++;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int client_fd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == client_fd) {
            clients[i] = 0;
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void* handle_client(void* arg) {
    int client_fd = *(int*)arg;
    free(arg);

    char buf[256];
    while (1) {
        int n = read(client_fd, buf, sizeof(buf) - 1);
        if (n <= 0) {
            printf("Client disconnected.\n");
            break;
        }
        //buf[n] = '\0';
        //printf("[Thread %ld] Received: %s\n", pthread_self(), buf);
    }

    close(client_fd);
    remove_client(client_fd);
    return NULL;
}

void broadcast(const void* msg, size_t len) {
	pthread_mutex_lock(&clients_mutex);
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (clients[i] != 0) {
			write(clients[i], msg, len);
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

void* server_thread(void* arg) {
    int server_fd;
    struct sockaddr_un addr;

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    unlink(socket_path);

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); exit(1); }
    if (listen(server_fd, 5) < 0) { perror("listen"); exit(1); }

	int flags = fcntl(server_fd, F_GETFL, 0);
	fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    printf("Server listening on %s\n", socket_path);

    while (running) {
		fd_set fds;
		struct timeval tv = {0, 500000}; // 0.5 seconds

		FD_ZERO(&fds);
		FD_SET(server_fd, &fds);

		int ready = select(server_fd + 1, &fds, NULL, NULL, &tv);
		if (ready < 0) { perror("select"); break; }
		if (ready == 0) continue; // timeout, check running again

		if (FD_ISSET(server_fd, &fds)) {
			int* client_fd = malloc(sizeof(int));
			*client_fd = accept(server_fd, NULL, NULL);

			if (*client_fd < 0) {
				free(client_fd);
				if (running) perror("accept"); // ignore errors during shutdown
				continue;
			}

			add_client(*client_fd);
			pthread_t tid;
			pthread_create(&tid, NULL, handle_client, client_fd);
			pthread_detach(tid);
		}
    }

    close(server_fd);
    unlink(socket_path);
    return NULL;
}

void server_start() {
	get_socket_path(socket_path);
	pthread_create(&server_tid, NULL, server_thread, NULL);
}

void server_shutdown() {
	running = 0;
	pthread_join(server_tid, NULL);
}
