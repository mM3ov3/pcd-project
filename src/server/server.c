#include "server.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <errno.h>

int sockfd;
ClientHashMap client_map;
MinHeap upload_queue;
pthread_t upload_thread;

int server_init() {
    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        return -1;
    }

    // Bind socket
    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(SERVER_PORT);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        return -1;
    }

    // Initialize data structures
    hashmap_init(&client_map);
    minheap_init(&upload_queue, MAX_UPLOADS * 2);

    // Create upload thread
    if (pthread_create(&upload_thread, NULL, upload_thread_func, NULL) != 0) {
        perror("Failed to create upload thread");
        return -1;
    }

    return 0;
}

void server_run() {
    struct pollfd fds[1];
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    printf("Server running on port %d\n", SERVER_PORT);

    while (1) {
        int ret = poll(fds, 1, 1000);
        if (ret < 0) {
            perror("poll");
            break;
        }

        if (ret == 0) {
            // Timeout - cleanup dead clients
            cleanup_dead_clients();
            continue;
        }

        if (fds[0].revents & POLLIN) {
            struct sockaddr_in cli_addr;
            socklen_t cli_len = sizeof(cli_addr);
            char buffer[BUFFER_SIZE];

            ssize_t n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                                (struct sockaddr *)&cli_addr, &cli_len);
            if (n < 0) {
                perror("recvfrom");
                continue;
            }

            // Process message based on type
            uint8_t msg_type = buffer[0];
            switch (msg_type) {
                case CLIENT_ID_REQ:
                    handle_new_client(&cli_addr);
                    break;
                case HEARTBEAT: {
                    Heartbeat *hb = (Heartbeat *)buffer;
                    handle_heartbeat(hb->client_id, &cli_addr);
                    break;
                }
                case JOB_REQ: {
                    JobReq *jr = (JobReq *)buffer;
                    handle_job_request(jr, &cli_addr);
                    break;
                }
                case UPLOAD_REQ: {
                    UploadReq *ur = (UploadReq *)buffer;
                    handle_upload_request(ur, &cli_addr);
                    break;
                }
                default:
                    printf("Unknown message type: %d\n", msg_type);
            }
        }

        cleanup_dead_clients();
    }
}

void server_cleanup() {
    close(sockfd);
    hashmap_free(&client_map);
    minheap_free(&upload_queue);
    pthread_cancel(upload_thread);
    pthread_join(upload_thread, NULL);
}

int main() {
    if (server_init() != 0) {
        fprintf(stderr, "Server initialization failed\n");
        return 1;
    }
    
    server_run();
    server_cleanup();
    
    return 0;
}