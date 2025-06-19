#include "admin_handler.h"
#include "log_queue.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define ADMIN_SOCKET_PATH "/tmp/admin.sock"

extern LogQueue global_log_queue;

// --- Local helpers now part of this file ---

static int make_socket_non_blocking(int fd) 
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int setup_admin_socket() 
{
	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("socket");
		exit(1);
	}

	unlink(ADMIN_SOCKET_PATH); // remove stale socket
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, ADMIN_SOCKET_PATH, sizeof(addr.sun_path) - 1);

	if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		exit(1);
	}

	if (listen(sockfd, 1) < 0) {
		perror("listen");
		exit(1);
	}

	printf("[admin] Listening on %s\n", ADMIN_SOCKET_PATH);
	return sockfd;
}

// --- Main admin thread ---

void *admin_thread(void *arg) 
{
	int sockfd = setup_admin_socket();

	while (1) {
		int client_fd = accept(sockfd, NULL, NULL);
		if (client_fd < 0) {
			perror("accept");
			continue;
		}

		make_socket_non_blocking(client_fd);
		send(client_fd, "Welcome to Admin Console.\nadmin> ", 33, 0);

		struct pollfd fds[] = {{client_fd, POLLIN, 0}};
		char recv_buf[512], logline[512];
		int show_logs = 0;

		while (1) {
			int ret = poll(fds, 1, show_logs ? 1000 : -1);
			if (ret < 0) {
				perror("poll");
				break;
			}

			if (show_logs) {
				while (log_queue_pop_timed(&global_log_queue, logline, 0) == 1) {
					strcat(logline, "\n");
					if (send(client_fd, logline, strlen(logline), 0) <= 0)
						goto disconnect;
				}
			}

			if (fds[0].revents & POLLIN) {
				ssize_t n = recv(client_fd, recv_buf, sizeof(recv_buf) - 1, 0);
				if (n <= 0) break;

				recv_buf[n] = 0;
				char *newline = strchr(recv_buf, '\n');
				if (newline) *newline = 0;

				if (strcasecmp(recv_buf, "SHOW_LOGS") == 0) {
					show_logs = 1;
					send(client_fd, "[Streaming logs]\n", 17, 0);
				} else if (strcasecmp(recv_buf, "STOP_LOGS") == 0) {
					show_logs = 0;
					send(client_fd, "[Log streaming stopped]\nadmin> ", 31, 0);
				} else if (strcasecmp(recv_buf, "HELP") == 0) {
					const char *help =
						"Commands:\n"
						"  HELP        Show this message\n"
						"  SHOW_LOGS   Start log stream\n"
						"  STOP_LOGS   Stop log stream\n"
						"  EXIT        Close admin session\nadmin> ";
					send(client_fd, help, strlen(help), 0);
				} else if (strcasecmp(recv_buf, "EXIT") == 0) {
					break;
				} else {
					send(client_fd, "Unknown command.\nadmin> ", 24, 0);
				}
			}
		}

disconnect:
		close(client_fd);
	}

	return NULL;
}

