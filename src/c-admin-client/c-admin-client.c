#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>

#define SOCKET_PATH "/tmp/admin.sock"
#define BUF_SIZE 512

volatile sig_atomic_t in_log_mode = 0;
int sockfd_global = -1;
volatile int got_line = 0;
char *line_buffer = NULL;

void handle_sigint(int sig) 
{
	if (in_log_mode) {
		// Stop logs logic...
		const char *stop = "STOP_LOGS\n";
		if (sockfd_global != -1) {
			send(sockfd_global, stop, strlen(stop), 0);
		}
		in_log_mode = 0;
		printf("\n[Log streaming stopped. You may continue entering commands.]\n");
		rl_on_new_line();
		rl_redisplay();
	} else {
		printf("\nExiting.\n");
		rl_cleanup_after_signal();    // Restore terminal modes before exit
		rl_callback_handler_remove(); // Remove readline handler cleanly
		exit(0);
	}
}

int connect_to_server() 
{
	int sockfd;
	struct sockaddr_un addr;

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

	if (connect(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) == -1) {
		perror("connect");
		close(sockfd);
		exit(EXIT_FAILURE);
	}

	return sockfd;
}

void readline_callback(char *line) 
{
	if (line == NULL) {
		printf("EOF, exiting...\n");
		exit(0);
	}
	if (*line) {
		add_history(line);
	}
	line_buffer = line; // Save the line for processing
	got_line = 1;
}

void event_loop(int sockfd) 
{
	struct pollfd fds[2];
	char buffer[BUF_SIZE];

	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;

	fds[1].fd = sockfd;
	fds[1].events = POLLIN;

	ssize_t n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
	if (n <= 0) {
		fprintf(stderr, "Failed to receive welcome message.\n");
		return;
	}
	buffer[n] = '\0';
	printf("%s", buffer);  // print the welcome message from server

	rl_callback_handler_install("admin> ", readline_callback);

	while (1) {
		int ret = poll(fds, 2, -1);
		if (ret == -1) {
			if (errno == EINTR) continue;
			perror("poll");
			break;
		}

		if (fds[0].revents & POLLIN) {
			rl_callback_read_char();
		}

		if (got_line) {
			if (!line_buffer) {
				printf("\nEOF. Exiting.\n");
				break;
			}

			if (strcasecmp(line_buffer, "SHOW_LOGS") == 0) {
				in_log_mode = 1;
			} else if (strcasecmp(line_buffer, "STOP_LOGS") == 0) {
				in_log_mode = 0;
			} else if (strcasecmp(line_buffer, "EXIT") == 0) {
				free(line_buffer);
				printf("Exiting.\n");
				break;
			}

			if (send(sockfd, line_buffer, strlen(line_buffer), 0) == -1) {
				perror("send");
				free(line_buffer);
				break;
			}

			free(line_buffer);
			line_buffer = NULL;
			got_line = 0;

			rl_on_new_line();
			rl_redisplay();
		}

		if (fds[1].revents & POLLIN) {
			ssize_t n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
			if (n <= 0) {
				if (n == 0) {
					printf("\nServer closed the connection.\n");
				} else {
					perror("recv");
				}
				break;
			}

			buffer[n] = '\0';
			// Save current line buffer and cursor position
			char *saved_line = strdup(rl_line_buffer);
			int saved_point = rl_point;

			// Move to beginning of line and clear it
			rl_save_prompt();
			rl_replace_line("", 0);
			rl_redisplay();

			// Move cursor to new line so output is clean
			rl_crlf();
			printf("%s", buffer);
			fflush(stdout);

			// Restore the line buffer and cursor
			rl_restore_prompt();
			rl_replace_line(saved_line, 1); // 1 = clear undo list
			rl_point = saved_point;
			rl_redisplay();

			free(saved_line);
			if (in_log_mode) {
				printf("\n[Type STOP_LOGS or Ctrl+C to stop log streaming]\n");
			}
			fflush(stdout);
		}

		if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			// fprintf(stderr, "\nSocket error, exiting.\n");
			break;
		}
	}

	rl_callback_handler_remove();
}


int main() 
{
	struct sigaction sa;
	sa.sa_handler = handle_sigint;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);

	int sockfd = connect_to_server();
	sockfd_global = sockfd;

	printf("Connected to admin server at %s\n", SOCKET_PATH);
	event_loop(sockfd);

	close(sockfd);
	return 0;
}

