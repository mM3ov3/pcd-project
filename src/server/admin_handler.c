#include "admin_handler.h"
#include "server.h"
#include "job_handler.h"
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
#include <stdint.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <time.h>


#define ADMIN_SOCKET_PATH "/tmp/admin.sock"
#define INACTIVITY_TIMEOUT_MS 60000  // 60 seconds inactivity timeout

/* Extern data structures */
extern LogQueue global_log_queue;
extern ClientInfo* clients;
extern PendingJob *pending_jobs;   // Pointer to your dynamic array of pending jobs
extern size_t client_count;
extern size_t job_count;      // Number of jobs in the queue
extern pthread_mutex_t clients_mutex;
extern pthread_mutex_t max_limits_mutex;
extern pthread_mutex_t jobs_mutex;

/* Helpers */

void log_append(const char *category, const char *fmt, ...) 
{
	char buf[LOG_ENTRY_MAX];
	va_list args;

	// Get current time string
	time_t now = time(NULL);
	struct tm tm_now;
	localtime_r(&now, &tm_now);
	char time_str[32];
	strftime(time_str, sizeof(time_str), "[%Y-%m-%d %H:%M:%S]", &tm_now);

	// Compose the full prefix: [CATEGORY] [TIME]
	int prefix_len = snprintf(buf, sizeof(buf), "%s %s ", category, time_str);

	// Append the formatted log message
	va_start(args, fmt);
	vsnprintf(buf + prefix_len, sizeof(buf) - prefix_len, fmt, args);
	va_end(args);

	// Push to log queue
	log_queue_push(&global_log_queue, buf);
}

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

static void format_client_id_hex(const uint8_t id[16], char *out, size_t out_size) 
{
	for (int i = 0; i < 16; ++i) {
		snprintf(out + i * 2, out_size - i * 2, "%02x", id[i]);
	}
}

static int hexchar_to_int(char c) 
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
	if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
	return -1;
}

static int parse_client_id(const char *hex_str, uint8_t *out_id) 
{
	if (strlen(hex_str) != 32) return -1;
	for (int i = 0; i < 16; ++i) {
		int hi = hexchar_to_int(hex_str[i*2]);
		int lo = hexchar_to_int(hex_str[i*2 + 1]);
		if (hi < 0 || lo < 0) return -1;
		out_id[i] = (hi << 4) | lo;
	}
	return 0;
}


/* Commands */

static void list_clients(int fd) 
{
	pthread_mutex_lock(&clients_mutex);

	if (client_count == 0) {
		dprintf(fd, "[No connected clients]\n");
		pthread_mutex_unlock(&clients_mutex);
		return;
	}

	dprintf(fd, "Connected clients:\n");

	for (size_t i = 0; i < client_count; ++i) {
		char ip_str[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(clients[i].addr.sin_addr), ip_str, sizeof(ip_str));
		int port = ntohs(clients[i].addr.sin_port);

		char id_str[33];
		format_client_id_hex(clients[i].client_id, id_str, sizeof(id_str));

		char time_buf[64];
		struct tm *tm_info = localtime(&clients[i].last_heartbeat);
		strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

		dprintf(fd, "  ID: %s\t  IP: %s:%d\t  Last heartbeat: %s\n\n",
				id_str, ip_str, port, time_buf);
	}
	pthread_mutex_unlock(&clients_mutex);
}

void set_max_uploads(int n) 
{
	if (n < 0 || n > MAX_UPLOADS) return;  // ignore invalid values
	pthread_mutex_lock(&max_limits_mutex);
	max_uploads = n;
	pthread_mutex_unlock(&max_limits_mutex);
}

static int kick_client_by_id(const uint8_t client_id[16]) 
{
	pthread_mutex_lock(&clients_mutex);

	for (size_t i = 0; i < client_count; ++i) {
		if (memcmp(clients[i].client_id, client_id, 16) == 0) {
			// Optionally send a "kick" UDP message to client
			const char *kick_msg = "You have been kicked by the admin.\n";
			sendto(udp_sock, kick_msg, strlen(kick_msg), 0,
					(struct sockaddr*)&clients[i].addr, sizeof(clients[i].addr));

			// Remove client from array
			memmove(&clients[i], &clients[i+1], (client_count - i - 1) * sizeof(ClientInfo));
			client_count--;
			clients = realloc(clients, client_count * sizeof(ClientInfo));

			pthread_mutex_unlock(&clients_mutex);
			return 1; // success
		}
	}

	pthread_mutex_unlock(&clients_mutex);
	return 0; // not found
}

void show_processing_queue(int client_fd) 
{
	pthread_mutex_lock(&jobs_mutex);

	if (job_count == 0) {
		send(client_fd, "[Queue is empty]\n", 23, 0);
		pthread_mutex_unlock(&jobs_mutex);
		return;
	}

	char buffer[4096];  // Larger buffer for longer output
	size_t offset = 0;
	offset += snprintf(buffer + offset, sizeof(buffer) - offset,
			"Processing queue:\n");

	for (size_t i = 0; i < job_count; i++) {
		PendingJob *job = &pending_jobs[i];  // Adjust this to your actual variable

		// Format client ID as hex string:
		char client_id_str[33] = {0};
		for (int j = 0; j < 16; j++) {
			sprintf(client_id_str + j*2, "%02x", job->client_id[j]);
		}

		// Format last_update as string:
		char time_str[26];  // ctime_r requires buffer of size >= 26
		ctime_r(&job->last_update, time_str);
		// Remove trailing newline added by ctime_r:
		time_str[strcspn(time_str, "\n")] = '\0';

		offset += snprintf(buffer + offset, sizeof(buffer) - offset,
				"%zu. Client %s, Job ID: %u\n"
				"    Command: %s\n"
				"    Files: %d received out of %d\n"
				"    Last update: %s\n",
				i + 1,
				client_id_str,
				job->job_id,
				job->command,
				job->files_received,
				job->file_count,
				time_str);

		if (offset >= sizeof(buffer) - 256) {  // Leave some margin
						       // Send partial output if buffer almost full
			send(client_fd, buffer, offset, 0);
			offset = 0;  // Reset buffer
		}
	}

	if (offset > 0) {
		send(client_fd, buffer, offset, 0);
	}

	pthread_mutex_unlock(&jobs_mutex);
}



/* Display and thread */

void handle_admin_command(int client_fd, char *input, int *show_logs) 
{
	trim_whitespace(input);

	if (strlen(input) == 0) {
		// send_prompt(client_fd);
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
			"  KICK_CLIENT <client_id>\n"
			"      Disconnect a specific client by ID.\n\n"
			"  SHOW_QUEUE\n"
			"      Display the processing queue.\n\n"
			"  SET_MAX_UPLOADS <number>\n"
			"      Set the maximum number of simultaneous uploads.\n\n"
			"  SHOW_LOGS\n"
			"      Stream logs from the server in real-time (tail -f style).\n\n"
			"  EXIT\n"
			"      Close the admin session.\n\n";
		send(client_fd, help, strlen(help), 0);
		// send_prompt(client_fd);
	} else if (strcasecmp(cmd, "SHOW_LOGS") == 0) {
		*show_logs = 1;
		send(client_fd, "[Streaming logs]\n", 17, 0);
		// no prompt sent here, log streaming mode disables prompt
	} else if (strcasecmp(cmd, "STOP_LOGS") == 0) {
		*show_logs = 0;
		// send(client_fd, "[Log streaming stopped]\n", 24, 0);
		// send_prompt(client_fd);
	} else if (strcasecmp(cmd, "KICK_CLIENT") == 0) {
		if (!arg) {
			send(client_fd, "Usage: KICK_CLIENT <id>\n", 23, 0);
			// send_prompt(client_fd);
			return;
		}
		// Assume client id is UUID string, not int - fix that later
		uint8_t carg[16];
		parse_client_id(arg, carg);
		kick_client_by_id(carg);
		send(client_fd, "User got kicked.\n\n", 17, 0);
		// send_prompt(client_fd);
	} else if (strcasecmp(cmd, "SET_MAX_UPLOADS") == 0) {
		if (!arg) {
			send(client_fd, "Usage: SET_MAX_UPLOADS <n>\n\n", 26, 0);
			// send_prompt(client_fd);
			return;
		}
		int n = atoi(arg);
		set_max_uploads(n);
		send(client_fd, "New upload limit set.\n\n", 22, 0);
		// send_prompt(client_fd);
	} else if (strcasecmp(cmd, "SHOW_CLIENTS") == 0) {
		list_clients(client_fd);
		// send_prompt(client_fd);
	} else if (strcasecmp(cmd, "SHOW_QUEUE") == 0) {
		show_processing_queue(client_fd);
		// send_prompt(client_fd);
	} else if (strcasecmp(cmd, "EXIT") == 0) {
		send(client_fd, "Goodbye.\n\n", 9, 0);
		*show_logs = -1;  // signal disconnect
				  // no prompt needed here
	} else {
		send(client_fd, "Unknown command.\n\n", 17, 0);
		// send_prompt(client_fd);
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
    int client_connected = 0;  // Flag: is an admin client connected now?

	while (1) {
		int client_fd = accept(sockfd, NULL, NULL);
		if (client_fd < 0) {
			perror("accept");
			continue;
		}

		if (client_connected) {
			// Reject new connection politely
			const char *msg = "Only one admin client allowed at a time. Try again later.\n";
			send(client_fd, msg, strlen(msg), 0);
			close(client_fd);
			continue;
		}

		client_connected = 1;  // Mark client as connected
		make_socket_non_blocking(client_fd);
		send(client_fd, "Welcome to Admin Console.\nType HELP to see available "
				"commands.\n", 70, 0);

		struct pollfd fds[] = {{client_fd, POLLIN, 0}};
		char recv_buf[4096], logline[4096];
		int show_logs = 0;

		// Setup inactivity timer
		struct timespec last_activity;
		clock_gettime(CLOCK_MONOTONIC, &last_activity);

		while (1) {
			int timeout_ms;

			if (show_logs) {
				// Poll every 1 second to send logs but don't disconnect due to inactivity here
				timeout_ms = 1000;
			} else {
				// Calculate how much time left before inactivity timeout
				struct timespec now;
				clock_gettime(CLOCK_MONOTONIC, &now);

				int elapsed_ms = (now.tv_sec - last_activity.tv_sec) * 1000 +
					(now.tv_nsec - last_activity.tv_nsec) / 1000000;

				timeout_ms = INACTIVITY_TIMEOUT_MS - elapsed_ms;

				if (timeout_ms <= 0) {
					// Timeout expired: disconnect client
					send(client_fd, "Disconnected due to inactivity.\n", 32, 0);
					goto disconnect;
				}
			}

			int ret = poll(fds, 1, timeout_ms);
			if (ret < 0) {
				perror("poll");
				break;
			}

			if (show_logs) {
				// Send available logs
				while (log_queue_pop_timed(&global_log_queue, logline, 0) == 1) {
					strcat(logline, "\n");
					if (send(client_fd, logline, strlen(logline), 0) <= 0)
						goto disconnect;

					// Reset inactivity timer on every log sent
					clock_gettime(CLOCK_MONOTONIC, &last_activity);
				}
			}

			if (fds[0].revents & POLLIN) {
				ssize_t n = recv(client_fd, recv_buf, sizeof(recv_buf) - 1, 0);
				if (n <= 0)
					break;

				recv_buf[n] = 0;

				handle_admin_command(client_fd, recv_buf, &show_logs);
				if (show_logs == -1)  // EXIT command received
					break;

				// Reset inactivity timer on any command received
				clock_gettime(CLOCK_MONOTONIC, &last_activity);
			}
		}

disconnect:
		close(client_fd);
		client_connected = 0;  // Mark client as disconnected
	}

	return NULL;
}

