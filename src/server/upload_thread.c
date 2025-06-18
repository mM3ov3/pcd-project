#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

void *upload_thread_func(void *arg) {
    (void)arg;
    
    // Create TCP listening socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return NULL;
    }

    // Set up server address
    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(SERVER_PORT + 1);

    // Bind socket
    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return NULL;
    }

    // Listen for connections
    if (listen(listen_fd, MAX_UPLOADS) < 0) {
        perror("listen");
        close(listen_fd);
        return NULL;
    }

    printf("Upload thread listening on port %d\n", SERVER_PORT + 1);

    while (1) {
        // Get next upload job
        UploadJob *job = NULL;
        pthread_mutex_lock(&upload_queue.lock);
        if (upload_queue.size > 0) {
            job = minheap_pop(&upload_queue);
        }
        pthread_mutex_unlock(&upload_queue.lock);

        if (job) {
            // Accept connection
            struct sockaddr_in cli_addr;
            socklen_t cli_len = sizeof(cli_addr);
            int conn_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);
            if (conn_fd < 0) {
                perror("accept");
                free(job);
                continue;
            }

            // Create file path
            char file_path[256];
            snprintf(file_path, sizeof(file_path), 
                    "processing/%.16s_%u/%s", job->client_id, job->job_id, job->filename);
            
            // Create file
            int file_fd = open(file_path, O_WRONLY | O_CREAT, 0644);
            if (file_fd < 0) {
                perror("open");
                close(conn_fd);
                free(job);
                continue;
            }

            // Transfer file data
            char buffer[4096];
            ssize_t bytes_read;
            while ((bytes_read = read(conn_fd, buffer, sizeof(buffer))) > 0) {
                if (write(file_fd, buffer, bytes_read) < 0) {
                    perror("write");
                    break;
                }
            }

            // Cleanup
            close(file_fd);
            close(conn_fd);
            free(job);
        } else {
            usleep(100000); // Sleep for 100ms if no jobs
        }
    }

    close(listen_fd);
    return NULL;
}