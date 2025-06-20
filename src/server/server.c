#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include "protocol.h"
#include "common.h"
#include "job_handler.h"
#include "upload_handler.h"
#include "processing.h"
#include "server.h"
#include "admin_handler.h"
#include "log_queue.h"

#define MAX_EVENTS 10
#define BUFFER_SIZE 2048

ClientInfo *clients = NULL;
size_t client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t max_limits_mutex = PTHREAD_MUTEX_INITIALIZER;
DownloadQueue download_queue = {0};
LogQueue global_log_queue;

int max_uploads = MAX_UPLOADS;
int udp_sock = -1;

void *client_thread(void *arg);
void *watcher_thread(void *arg);
void *processing_thread(void *arg);
void *download_thread(void *arg);
void handle_udp_message(int sockfd, struct sockaddr_in *client_addr, uint8_t *buffer, ssize_t n);
void generate_client_id(uint8_t *client_id);
void cleanup_dead_clients(time_t timeout);

int main() {
    int tcp_sock, download_sock;
    struct sockaddr_in server_addr;
    
    // Initialize download queue
    download_queue.jobs = malloc(10 * sizeof(DownloadJob));
    download_queue.capacity = 10;
    download_queue.size = 0;
    pthread_mutex_init(&download_queue.mutex, NULL);
    pthread_cond_init(&download_queue.cond, NULL);
    
    // Create processing directory if it doesn't exist
    if (mkdir("processing", 0777) != 0 && errno != EEXIST) {
        perror("[DEBUG] Failed to create processing directory");
        exit(EXIT_FAILURE);
    }
    
    if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("UDP socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    if ((tcp_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("TCP socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    if ((download_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Download socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (bind(udp_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("UDP bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (bind(tcp_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("TCP bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(tcp_sock, max_uploads) < 0) {
        perror("TCP listen failed");
        exit(EXIT_FAILURE);
    }
    
    server_addr.sin_port = htons(SERVER_PORT + 1);
    
    if (bind(download_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Download bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(download_sock, max_uploads) < 0) {
        perror("Download listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Server started on port %d (Uploads) and %d (downloads)\n", SERVER_PORT, SERVER_PORT + 1);
    
    init_job_handler();
    init_upload_handler(tcp_sock);
    init_processing();
    log_queue_init(&global_log_queue);
    
    pthread_t client_tid, watcher_tid, processing_tid, download_tid, admin_tid;
    
    pthread_create(&client_tid, NULL, client_thread, &udp_sock);
    pthread_create(&watcher_tid, NULL, watcher_thread, NULL);
    pthread_create(&processing_tid, NULL, processing_thread, &udp_sock);
    pthread_create(&download_tid, NULL, download_thread, &download_sock);
    pthread_create(&admin_tid, NULL, admin_thread, NULL);
    
    pthread_join(client_tid, NULL);
    pthread_join(watcher_tid, NULL);
    pthread_join(processing_tid, NULL);
    pthread_join(download_tid, NULL);
    pthread_join(admin_tid, NULL);
    
    close(udp_sock);
    close(tcp_sock);
    close(download_sock);
    free(download_queue.jobs);
    pthread_mutex_destroy(&download_queue.mutex);
    pthread_cond_destroy(&download_queue.cond);
    return 0;
}

void *client_thread(void *arg) {
    int sockfd = *(int *)arg;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    uint8_t buffer[BUFFER_SIZE];
    
    while (1) {
        ssize_t n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, 
                            (struct sockaddr *)&client_addr, &addr_len);
        if (n < 0) {
            perror("[DEBUG] recvfrom failed");
            continue;
        }
        
        handle_udp_message(sockfd, &client_addr, buffer, n);
        
        if (rand() % 100 < 5) {
            cleanup_dead_clients(HEARTBEAT_TIMEOUT);
        }
    }
    return NULL;
}

void handle_udp_message(int sockfd, struct sockaddr_in *client_addr, uint8_t *buffer, ssize_t n) {
    if (n < 1) {
        fprintf(stderr, "[DEBUG] Received empty message\n");
        return;
    }
    
    uint8_t type = buffer[0];
    printf("[DEBUG] Received message type %d from %s:%d\n", 
           type, inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
    
    switch (type) {
        case CLIENT_ID_REQ: {
            ClientIdRequest *req = (ClientIdRequest *)buffer;
            ClientIdResponse resp;
            
            resp.type = CLIENT_ID_ACK;
            resp.message_id = req->message_id;
            generate_client_id(resp.client_id);
            
            pthread_mutex_lock(&clients_mutex);
            
            ClientInfo new_client;
            memcpy(new_client.client_id, resp.client_id, 16);
            new_client.addr = *client_addr;
            new_client.last_heartbeat = time(NULL);
            
            clients = realloc(clients, (client_count + 1) * sizeof(ClientInfo));
            clients[client_count++] = new_client;
            
            printf("[DEBUG] Assigned client_id=%02x%02x to %s:%d\n",
                   resp.client_id[0], resp.client_id[1],
                   inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
	    log_append("[CLIENT]", "Assigned client_id=%02x%02x to %s:%d", resp.client_id[0],
	           resp.client_id[1], inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
            
            pthread_mutex_unlock(&clients_mutex);
            
            printf("[DEBUG] Sending CLIENT_ID_ACK to %s:%d\n", 
                   inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
            sendto(sockfd, &resp, sizeof(resp), 0, 
                  (struct sockaddr *)client_addr, sizeof(*client_addr));
            break;
        }
        
        case HEARTBEAT: {
            Heartbeat *hb = (Heartbeat *)buffer;
            
            pthread_mutex_lock(&clients_mutex);
            for (size_t i = 0; i < client_count; i++) {
                if (memcmp(clients[i].client_id, hb->client_id, 16) == 0) {
                    clients[i].last_heartbeat = time(NULL);
                    clients[i].addr = *client_addr;
                    printf("[DEBUG] Updated heartbeat for client %02x%02x\n", 
                           hb->client_id[0], hb->client_id[1]);
		    log_append("[CLIENT]", "Updated hearbeat for client %02x%02x",
		           hb->client_id[0], hb->client_id[1]);
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            break;
        }
        
        case JOB_REQ: {
            JobRequest *req = (JobRequest *)buffer;
            char *job_cmd = (char *)(buffer + sizeof(JobRequest));
            if (n < (ssize_t)(sizeof(JobRequest) + req->cmd_len)) {
                fprintf(stderr, "[DEBUG] Invalid JOB_REQ size: %zd, expected %zu\n",
                        n, sizeof(JobRequest) + req->cmd_len);
                break;
            }
            job_cmd[req->cmd_len] = '\0'; // Ensure null-termination
            
            printf("[DEBUG] Handling JOB_REQ for job_id=%u, cmd=%s, file_count=%d, client_id=%02x%02x\n", 
                   req->job_id, job_cmd, req->file_count, req->client_id[0], req->client_id[1]);
            
            JobResponse resp;
            resp.type = JOB_ACK;
            resp.message_id = req->message_id;
            resp.job_id = req->job_id;
            
            int create_success = create_job(req->client_id, client_addr, req->job_id, job_cmd, req->file_count);
            resp.status = create_success ? STATUS_OK : STATUS_ERROR;
            const char *msg = create_success ? "Job created successfully" : "Failed to create job";
            resp.msg_len = strlen(msg);

            
            size_t resp_size = sizeof(resp) + resp.msg_len;
            uint8_t *send_buf = malloc(resp_size);
            if (!send_buf) {
                perror("[DEBUG] malloc failed for JOB_ACK");
                break;
            }
            memcpy(send_buf, &resp, sizeof(resp));
            memcpy(send_buf + sizeof(resp), msg, resp.msg_len);
            

	    if (resp.status == STATUS_OK)
	    	log_append("[JOB]", "Job %u for client %02x%02x created succesfully.", resp.job_id,
		           req -> client_id[0], req -> client_id[1]);
            else
	    	log_append("[JOB]", "Job %u for client %02x%02x failed.", resp.job_id,
		           req -> client_id[0], req -> client_id[1]);
	    
            printf("[DEBUG] Sending JOB_ACK to %s:%d for job_id=%u, status=%d, msg=%s\n",
                   inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port),
                   resp.job_id, resp.status, msg);
            sendto(sockfd, send_buf, resp_size, 0,
                  (struct sockaddr *)client_addr, sizeof(*client_addr));
            free(send_buf);
            break;
        }
        
        case UPLOAD_REQ: {
            UploadRequest *req = (UploadRequest *)buffer;
            char *filename = (char *)(buffer + sizeof(UploadRequest));
            if (n < (ssize_t)(sizeof(UploadRequest) + req->name_len)) {
                fprintf(stderr, "[DEBUG] Invalid UPLOAD_REQ size: %zd, expected %zu\n",
                        n, sizeof(UploadRequest) + req->name_len);
                break;
            }
            filename[req->name_len] = '\0'; // Ensure null-termination
            
            printf("[DEBUG] Received UPLOAD_REQ for job_id=%u, filename=%s\n",
                   req->job_id, filename);
            handle_upload_request(sockfd, req, filename, client_addr);
            break;
        }
        
        case DOWNLOAD_REQ: {
            DownloadRequest *req = (DownloadRequest *)buffer;
            char *filename = (char *)(buffer + sizeof(DownloadRequest));
            if (n < (ssize_t)(sizeof(DownloadRequest) + req->name_len)) {
                fprintf(stderr, "[DEBUG] Invalid DOWNLOAD_REQ size: %zd, expected %zu\n",
                        n, sizeof(DownloadRequest) + req->name_len);
                break;
            }
            filename[req->name_len] = '\0'; // Ensure null-termination
            
            printf("[DEBUG] Received DOWNLOAD_REQ for job_id=%u, filename=%s\n",
                   req->job_id, filename);
            handle_download_request(sockfd, req, filename, client_addr);
            break;
        }
        
        default:
            fprintf(stderr, "[DEBUG] Unknown message type: %d\n", type);
    }
}

void generate_client_id(uint8_t *client_id) {
    for (int i = 0; i < 16; i++) {
        client_id[i] = rand() % 256;
    }
}

void cleanup_dead_clients(time_t timeout) {
    time_t now = time(NULL);
    pthread_mutex_lock(&clients_mutex);
    
    size_t new_count = 0;
    for (size_t i = 0; i < client_count; i++) {
        if (now - clients[i].last_heartbeat < timeout) {
            clients[new_count++] = clients[i];
        } else {
            printf("[DEBUG] Removing client %02x%02x due to timeout\n",
                   clients[i].client_id[0], clients[i].client_id[1]);
        }
    }
    
    if (new_count < client_count) {
        clients = realloc(clients, new_count * sizeof(ClientInfo));
        client_count = new_count;
        printf("[DEBUG] Cleaned up %zu dead clients, %zu remain\n", 
               client_count, new_count);
    }
    
    pthread_mutex_unlock(&clients_mutex);
}

void *watcher_thread(void *arg) {
    (void)arg;
    while (1) {
        cleanup_dead_clients(HEARTBEAT_TIMEOUT);
        sleep(HEARTBEAT_INTERVAL);
    }
    return NULL;
}

void *processing_thread(void *arg) {
    int sockfd = *(int *)arg;
    while (1) {
        process_pending_jobs(sockfd);
        sleep(1);
    }
    return NULL;
}

void *download_thread(void *arg) {
    int sockfd = *(int *)arg;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    while (1) {
        int client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("[DEBUG] Download accept failed");
            continue;
        }
        
        printf("[DEBUG] Accepted download connection from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        // Wait for a download job
        pthread_mutex_lock(&download_queue.mutex);
        while (download_queue.size == 0) {
            pthread_cond_wait(&download_queue.cond, &download_queue.mutex);
        }
        
        DownloadJob job = download_queue.jobs[0];
        memmove(&download_queue.jobs[0], &download_queue.jobs[1],
                (download_queue.size - 1) * sizeof(DownloadJob));
        download_queue.size--;
        
        pthread_mutex_unlock(&download_queue.mutex);
        
        // Verify client IP and client_id
        int client_valid = 0;
        pthread_mutex_lock(&clients_mutex);
        for (size_t i = 0; i < client_count; i++) {
            if (memcmp(clients[i].client_id, job.client_id, 16) == 0 &&
                clients[i].addr.sin_addr.s_addr == client_addr.sin_addr.s_addr) {
                client_valid = 1;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        
        if (!client_valid) {
            printf("[DEBUG] Invalid client for download: expected client_id=%02x%02x, IP=%s, got IP=%s:%d\n",
                   job.client_id[0], job.client_id[1],
                   inet_ntoa(job.client_addr.sin_addr),
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            close(client_fd);
            continue;
        }
        
        // Send file
        char file_path[512];
        snprintf(file_path, sizeof(file_path), "processing/%02x%02x_%08x/%s",
                 job.client_id[0], job.client_id[1], job.job_id, job.filename);
        
        printf("[DEBUG] Sending file: %s for job_id=%u\n", file_path, job.job_id);
        
        int file_fd = open(file_path, O_RDONLY);
        if (file_fd < 0) {
            perror("[DEBUG] open failed");
            close(client_fd);
            continue;
        }
        
        uint8_t buffer[4096];
        ssize_t bytes_read;
        while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
            if (send(client_fd, buffer, bytes_read, 0) != bytes_read) {
                perror("[DEBUG] send failed");
                break;
            }
        }
        
        close(file_fd);
        close(client_fd);
        printf("[DEBUG] File transfer complete for job_id=%u, filename=%s\n",
               job.job_id, job.filename);
    }
    return NULL;
}
