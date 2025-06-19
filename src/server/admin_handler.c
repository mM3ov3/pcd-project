#include "admin_handler.h"
#include "log_queue.h"

#include <sys/socket.h>
#include <ctype.h>
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

static void trim_whitespace(char *str) 
{
	// Left trim
	while (isspace((unsigned char)*str)) str++;

	if (*str == '\0') return;

	// Right trim
	char *end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end)) end--;
	*(end + 1) = '\0';
}

static void send_prompt(int client_fd) 
{
	send(client_fd, "admin> ", 7, 0);
}

void handle_admin_command(int client_fd, char *input, int *show_logs) 
{
	trim_whitespace(input);

	if (strlen(input) == 0) {
		send_prompt(client_fd);
		return;
	}

	char *cmd = strtok(input, " \t\n");
	char *arg = strtok(NULL, "\n");

	if (strcasecmp(cmd, "HELP") == 0) {
		const char *help =
			"Available commands:\n"
			"  HELP\n"
			"      Show this help message.\n\n"
			"  SHOW_CLIENTS\n"
			"      List all currently connected clients.\n\n"
			"  SHOW_QUEUE\n"
			"      Display the processing queue.\n\n"
			"  KICK_CLIENT <client_id>\n"
			"      Disconnect a specific client by ID.\n\n"
			"  SET_MAX_UPLOADS <number>\n"
			"      Set the maximum number of simultaneous uploads.\n\n"
			"  SET_MAX_DOWNLOADS <number>\n"
			"      Set the maximum number of simultaneous downloads.\n\n"
			"  SHOW_LOGS\n"
			"      Stream logs from the server in real-time (tail -f style).\n\n"
			"  STOP_LOGS\n"
			"      Stop receiving real-time log updates.\n\n"
			"  EXIT\n"
			"      Close the admin session.\n\n";
		send(client_fd, help, strlen(help), 0);
		send_prompt(client_fd);
	} else if (strcasecmp(cmd, "SHOW_LOGS") == 0) {
		*show_logs = 1;
		send(client_fd, "[Streaming logs]\n", 17, 0);
		// no prompt sent here, log streaming mode disables prompt
	} else if (strcasecmp(cmd, "STOP_LOGS") == 0) {
		*show_logs = 0;
		send(client_fd, "[Log streaming stopped]\n", 24, 0);
		send_prompt(client_fd);
	} else if (strcasecmp(cmd, "KICK_CLIENT") == 0) {
		if (!arg) {
			send(client_fd, "Usage: KICK_CLIENT <id>\n", 23, 0);
			send_prompt(client_fd);
			return;
		}
		// Assume client id is UUID string, not int - fix that later
		// TODO: kick_client_by_id(arg);
		dprintf(client_fd, "Kicked client %s (placeholder).\n", arg);
		send_prompt(client_fd);
	} else if (strcasecmp(cmd, "SET_MAX_UPLOADS") == 0) {
		if (!arg) {
			send(client_fd, "Usage: SET_MAX_UPLOADS <n>\n", 26, 0);
			send_prompt(client_fd);
			return;
		}
		int n = atoi(arg);
		// TODO: set_max_uploads(n);
		dprintf(client_fd, "Set max uploads to %d (placeholder).\n", n);
		send_prompt(client_fd);
	} else if (strcasecmp(cmd, "SET_MAX_DOWNLOADS") == 0) {
		if (!arg) {
			send(client_fd, "Usage: SET_MAX_DOWNLOADS <n>\n", 28, 0);
			send_prompt(client_fd);
			return;
		}
		int n = atoi(arg);
		// TODO: set_max_downloads(n);
		dprintf(client_fd, "Set max downloads to %d (placeholder).\n", n);
		send_prompt(client_fd);
	} else if (strcasecmp(cmd, "SHOW_CLIENTS") == 0) {
		// TODO: show_connected_clients(client_fd);
		send(client_fd, "[Connected clients listing not yet implemented]\n", 48, 0);
		send_prompt(client_fd);
	} else if (strcasecmp(cmd, "SHOW_QUEUE") == 0) {
		// TODO: show_processing_queue(client_fd);
		send(client_fd, "[Queue display not yet implemented]\n", 36, 0);
		send_prompt(client_fd);
	} else if (strcasecmp(cmd, "EXIT") == 0) {
		send(client_fd, "Goodbye.\n", 9, 0);
		*show_logs = -1;  // signal disconnect
				  // no prompt needed here
	} else {
		send(client_fd, "Unknown command.\n", 17, 0);
		send_prompt(client_fd);
	}
}

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
		send(client_fd, "Welcome to Admin Console.\nType HELP to see available "
				"commands.\nadmin> ", 70, 0);

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

				handle_admin_command(client_fd, recv_buf, &show_logs);
				if (show_logs == -1) break;  // EXIT was requested
			}


		}

disconnect:
		close(client_fd);
	}

	return NULL;
}

