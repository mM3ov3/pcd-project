#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>

#define SOCKET_PATH "/tmp/admin.sock"
#define BUF_SIZE 4096

volatile sig_atomic_t in_log_mode = 0;
int sockfd_global = -1;

void handle_sigint(int sig) 
{
	if (in_log_mode) {
		const char *stop = "STOP_LOGS\n";
		if (sockfd_global != -1) {
			send(sockfd_global, stop, strlen(stop), 0);
		}
		in_log_mode = 0;
		printf("\n[Log streaming stopped. You may continue entering commands.]\n");
		printf("admin> ");
		fflush(stdout);
	} else {
		printf("\nExiting.\n");
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

void event_loop(int sockfd) 
{
	struct pollfd fds[2];
	char buffer[BUF_SIZE];

	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;

	fds[1].fd = sockfd;
	fds[1].events = POLLIN;

	while (1) {
		int ret = poll(fds, 2, -1);
		if (ret == -1) {
			perror("poll");
			break;
		}

		// Handle user input
		if (fds[0].revents & POLLIN) {
			if (!fgets(buffer, sizeof(buffer), stdin)) {
				printf("EOF on stdin. Exiting.\n");
				break;
			}

			buffer[strcspn(buffer, "\n")] = 0;

			if (strcasecmp(buffer, "SHOW_LOGS") == 0) {
				in_log_mode = 1;
			} else if (strcasecmp(buffer, "STOP_LOGS") == 0) {
				in_log_mode = 0;
			}

			if (strcasecmp(buffer, "EXIT") == 0) {
				printf("Exiting.\n");
				break;
			}

			if (send(sockfd, buffer, strlen(buffer), 0) == -1) {
				perror("send");
				break;
			}
		}

		// Handle server messages
		if (fds[1].revents & POLLIN) {
			ssize_t n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
			if (n <= 0) {
				if (n == 0) {
					printf("Server closed the connection.\n");
				} else {
					perror("recv");
				}
				break;
			}

			buffer[n] = '\0';
			printf("%s", buffer);
			fflush(stdout);

			// REMOVE prompt printing here (server sends it)
			// if (!in_log_mode) {
			//     printf("admin> ");
			//     fflush(stdout);
			// }
		}

		if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			fprintf(stderr, "Socket error (revents: %d)\n", fds[1].revents);
			break;
		}
	}
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

