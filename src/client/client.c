#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include "protocol.h"
#include "common.h"
#include "menu.h"
#include "ffmpeg_commands.h"

#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 4096
#define MAX_RETRIES 3
#define UPLOAD_TIMEOUT 10
#define RESPONSE_TIMEOUT 5
#define JOB_RESULT_TIMEOUT 30 // New timeout for JOB_RESULT

uint8_t client_id[16];
uint32_t next_message_id = 1;
struct sockaddr_in server_addr;
int sockfd;
int download_sockfd;
pthread_t heartbeat_tid;

int upload_file(uint32_t job_id, const char *filename); // Updated declaration
void download_file(uint32_t job_id, const char *filename);
void send_heartbeat(void);
void* heartbeat_thread(void *arg);

int init_udp_socket() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("invalid server address");
        exit(EXIT_FAILURE);
    }
    
    return sock;
}

void get_client_id() {
    ClientIdRequest req = {
        .type = CLIENT_ID_REQ,
        .message_id = next_message_id++
    };
    
    for (int retry = 0; retry < MAX_RETRIES; retry++) {
        if (sendto(sockfd, &req, sizeof(req), 0, 
                  (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("sendto failed");
            continue;
        }
        
        uint8_t buffer[BUFFER_SIZE];
        socklen_t addr_len = sizeof(server_addr);
        ssize_t n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, 
                            (struct sockaddr *)&server_addr, &addr_len);
        
        if (n >= (ssize_t)sizeof(ClientIdResponse)) {
            ClientIdResponse *resp = (ClientIdResponse *)buffer;
            memcpy(client_id, resp->client_id, 16);
            printf("Got client ID: ");
            for (int i = 0; i < 16; i++) printf("%02x", client_id[i]);
            printf("\n");
            return;
        }
    }
    
    fprintf(stderr, "Failed to get client ID after %d retries\n", MAX_RETRIES);
    exit(EXIT_FAILURE);
}

void send_heartbeat() {
    Heartbeat hb = {
        .type = HEARTBEAT
    };
    memcpy(hb.client_id, client_id, 16);
    
    if (sendto(sockfd, &hb, sizeof(hb), 0, 
              (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("heartbeat send failed");
    }
}

void* heartbeat_thread(void *arg) {
    (void)arg;
    
    while (1) {
        send_heartbeat();
        sleep(HEARTBEAT_INTERVAL);
    }
    return NULL;
}

uint32_t submit_job(const char *command, const char **files, int file_count) {
    if (!command || !files || file_count <= 0) {
        fprintf(stderr, "[DEBUG] Invalid job parameters\n");
        return 0;
    }

    uint32_t job_id = rand();
    size_t cmd_len = strlen(command);
    
    if (cmd_len >= MAX_CMD_LEN) {
        fprintf(stderr, "[DEBUG] Command too long (max %d chars)\n", MAX_CMD_LEN-1);
        return 0;
    }

    printf("[DEBUG] Submitting job %u with command: %s\n", job_id, command);

    size_t req_size = sizeof(JobRequest) + cmd_len;
    uint8_t *buffer = malloc(req_size + 1);
    if (!buffer) {
        perror("[DEBUG] malloc failed");
        return 0;
    }
    memset(buffer, 0, req_size + 1);

    JobRequest *req = (JobRequest *)buffer;
    req->type = JOB_REQ;
    req->message_id = next_message_id++;
    memcpy(req->client_id, client_id, 16);
    req->job_id = job_id;
    req->file_count = file_count;
    req->cmd_len = cmd_len;
    memcpy(buffer + sizeof(JobRequest), command, cmd_len);

    if (sendto(sockfd, buffer, req_size, 0, 
              (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[DEBUG] sendto failed");
        free(buffer);
        return 0;
    }
    free(buffer);

    uint8_t resp_buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(server_addr);
    struct timeval tv = { .tv_sec = RESPONSE_TIMEOUT, .tv_usec = 0 };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    printf("[DEBUG] Waiting for JOB_ACK for job %u\n", job_id);
    ssize_t n = recvfrom(sockfd, resp_buffer, BUFFER_SIZE, 0, 
                        (struct sockaddr *)&server_addr, &addr_len);

    if (n < (ssize_t)sizeof(JobResponse)) {
        fprintf(stderr, "[DEBUG] Invalid JOB_ACK received (size=%zd)\n", n);
        return 0;
    }

    JobResponse *resp = (JobResponse *)resp_buffer;
    char *message = (char *)(resp_buffer + sizeof(JobResponse));
    if (resp->msg_len > 0 && resp->msg_len < BUFFER_SIZE - sizeof(JobResponse)) {
        message[resp->msg_len] = '\0';
    } else {
        message[0] = '\0';
    }

    printf("Job %u: %s\n", resp->job_id, message);

    if (resp->status != STATUS_OK) {
        fprintf(stderr, "[DEBUG] JOB_ACK status not OK (status=%d)\n", resp->status);
        return 0;
    }

    // Upload files
    int upload_success = 1;
    for (int i = 0; i < file_count; i++) {
        if (files[i]) {
            printf("[DEBUG] Attempting to upload file %d/%d: %s\n", i+1, file_count, files[i]);
            if (!upload_file(job_id, files[i])) {
                fprintf(stderr, "[DEBUG] Upload failed for file %s\n", files[i]);
                upload_success = 0;
            }
        }
    }

    if (!upload_success) {
        fprintf(stderr, "[DEBUG] One or more file uploads failed\n");
        return 0;
    }

    // Wait for JOB_RESULT with a timeout
    tv.tv_sec = JOB_RESULT_TIMEOUT;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    printf("[DEBUG] Waiting for JOB_RESULT for job %u\n", job_id);

    while (1) {
        n = recvfrom(sockfd, resp_buffer, BUFFER_SIZE, 0, 
                    (struct sockaddr *)&server_addr, &addr_len);
        
        if (n < 0) {
            fprintf(stderr, "[DEBUG] Timeout or error waiting for JOB_RESULT: %s\n", strerror(errno));
            return 0;
        }

        if (n > 0 && resp_buffer[0] == JOB_RESULT) {
            JobResult *result = (JobResult *)resp_buffer;
            message = (char *)(resp_buffer + sizeof(JobResult));
            if (result->msg_len > 0 && result->msg_len < BUFFER_SIZE - sizeof(JobResult)) {
                message[result->msg_len] = '\0';
            } else {
                message[0] = '\0';
            }
            
            printf("Job %u result: %s\n", result->job_id, message);
            if (result->status == STATUS_OK) {
                return job_id; // Return job_id only if job succeeded
            } else {
                fprintf(stderr, "[DEBUG] Job %u failed: %s\n", result->job_id, message);
                return 0;
            }
        }
    }
}

int upload_file(uint32_t job_id, const char *filename) {
    if (!filename || strlen(filename) == 0) {
        fprintf(stderr, "[DEBUG] Error: Invalid filename\n");
        return 0;
    }

    printf("[DEBUG] Uploading file: %s for job %u\n", filename, job_id);

    struct stat st;
    if (stat(filename, &st) != 0) {
        fprintf(stderr, "[DEBUG] Error: Cannot access file '%s' - %s\n", filename, strerror(errno));
        return 0;
    }

    size_t name_len = strlen(filename);
    if (name_len > MAX_FILENAME_LEN) {
        fprintf(stderr, "[DEBUG] Error: Filename too long\n");
        return 0;
    }

    size_t req_size = sizeof(UploadRequest) + name_len;
    uint8_t *buffer = malloc(req_size);
    if (!buffer) {
        perror("[DEBUG] Error: malloc failed");
        return 0;
    }

    UploadRequest *req = (UploadRequest *)buffer;
    req->type = UPLOAD_REQ;
    req->message_id = next_message_id++;
    memcpy(req->client_id, client_id, 16);
    req->job_id = job_id;
    req->file_size = st.st_size;
    req->name_len = name_len;
    memcpy(buffer + sizeof(UploadRequest), filename, name_len);

    struct timeval tv;
    tv.tv_sec = RESPONSE_TIMEOUT;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        printf("[DEBUG] Sending UPLOAD_REQ for %s, attempt %d\n", filename, attempt+1);
        if (sendto(sockfd, buffer, req_size, 0,
                  (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            fprintf(stderr, "[DEBUG] Attempt %d: Send failed - %s\n", attempt+1, strerror(errno));
            continue;
        }

        uint8_t resp_buffer[BUFFER_SIZE];
        socklen_t addr_len = sizeof(server_addr);
        ssize_t n = recvfrom(sockfd, resp_buffer, BUFFER_SIZE, 0,
                            (struct sockaddr *)&server_addr, &addr_len);

        if (n < (ssize_t)sizeof(UploadResponse)) {
            fprintf(stderr, "[DEBUG] Attempt %d: Invalid response size (%zd)\n", attempt+1, n);
            continue;
        }

        UploadResponse *resp = (UploadResponse *)resp_buffer;
        if (resp->status != STATUS_OK) {
            char *rejected_name = "unknown";
            if (resp->name_len > 0 && n >= (ssize_t)(sizeof(UploadResponse) + resp->name_len)) {
                rejected_name = (char *)(resp_buffer + sizeof(UploadResponse));
                rejected_name[resp->name_len] = '\0';
            }
            fprintf(stderr, "[DEBUG] Upload rejected for: %s (Status: %d)\n", rejected_name, resp->status);
            free(buffer);
            return 0;
        }

        if (resp->tcp_port == 0) {
            fprintf(stderr, "[DEBUG] Error: Server returned invalid port 0\n");
            free(buffer);
            return 0;
        }

        printf("[DEBUG] Server ready on port %d, starting transfer...\n", ntohs(resp->tcp_port));

        int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (tcp_sock < 0) {
            perror("[DEBUG] TCP socket failed");
            free(buffer);
            return 0;
        }

        tv.tv_sec = UPLOAD_TIMEOUT;
        setsockopt(tcp_sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        struct sockaddr_in tcp_addr;
        memcpy(&tcp_addr, &server_addr, sizeof(tcp_addr));
        tcp_addr.sin_port = resp->tcp_port;

        if (connect(tcp_sock, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr)) < 0) {
            fprintf(stderr, "[DEBUG] TCP connect failed: %s\n", strerror(errno));
            close(tcp_sock);
            free(buffer);
            return 0;
        }

        int file_fd = open(filename, O_RDONLY);
        if (file_fd < 0) {
            fprintf(stderr, "[DEBUG] Failed to open %s: %s\n", filename, strerror(errno));
            close(tcp_sock);
            free(buffer);
            return 0;
        }

        uint8_t file_buffer[4096];
        ssize_t bytes_read, bytes_sent;
        size_t total_sent = 0;
        struct timeval start, current;
        gettimeofday(&start, NULL);

        while ((bytes_read = read(file_fd, file_buffer, sizeof(file_buffer))) > 0) {
            bytes_sent = send(tcp_sock, file_buffer, bytes_read, 0);
            if (bytes_sent <= 0) {
                fprintf(stderr, "[DEBUG] Send error: %s\n", strerror(errno));
                break;
            }
            total_sent += bytes_sent;
            
            gettimeofday(&current, NULL);
            if (current.tv_sec - start.tv_sec >= 1) {
                printf("\r[DEBUG] Uploaded: %zu/%zu bytes (%.1f%%)", 
                      total_sent, st.st_size, (double)total_sent/st.st_size*100);
                fflush(stdout);
                start = current;
            }
        }

        close(file_fd);
        close(tcp_sock);
        free(buffer);

        if (bytes_read < 0) {
            fprintf(stderr, "[DEBUG] Read error: %s\n", strerror(errno));
            return 0;
        }

        printf("\n[DEBUG] Successfully uploaded %s (%zu bytes)\n", filename, total_sent);
        return 1; // Success
    }

    fprintf(stderr, "[DEBUG] Upload failed after %d attempts\n", MAX_RETRIES);
    free(buffer);
    return 0;
}

void download_file(uint32_t job_id, const char *filename) {
    if (!filename || strlen(filename) == 0) {
        fprintf(stderr, "[DEBUG] Error: Invalid filename for download\n");
        return;
    }

    printf("[DEBUG] Downloading file: %s for job %u\n", filename, job_id);

    size_t name_len = strlen(filename);
    if (name_len > MAX_FILENAME_LEN) {
        fprintf(stderr, "[DEBUG] Error: Download filename too long\n");
        return;
    }

    size_t req_size = sizeof(DownloadRequest) + name_len;
    uint8_t *buffer = malloc(req_size);
    if (!buffer) {
        perror("[DEBUG] Error: malloc failed");
        return;
    }

    DownloadRequest *req = (DownloadRequest *)buffer;
    req->type = DOWNLOAD_REQ;
    req->message_id = next_message_id++;
    memcpy(req->client_id, client_id, 16);
    req->job_id = job_id;
    req->name_len = name_len;
    memcpy(buffer + sizeof(DownloadRequest), filename, name_len);
    
    if (sendto(sockfd, buffer, req_size, 0, 
          (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[DEBUG] sendto failed");
        free(buffer);
        return;
    }
    free(buffer);
    
    uint8_t resp_buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(server_addr);
    struct timeval tv = { .tv_sec = RESPONSE_TIMEOUT, .tv_usec = 0 };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    printf("[DEBUG] Waiting for DOWNLOAD_ACK for %s\n", filename);
    ssize_t n = recvfrom(sockfd, resp_buffer, BUFFER_SIZE, 0, 
                        (struct sockaddr *)&server_addr, &addr_len);
    
    if (n < (ssize_t)sizeof(DownloadResponse)) {
        fprintf(stderr, "[DEBUG] Invalid DOWNLOAD_ACK received\n");
        return;
    }
    
    DownloadResponse *resp = (DownloadResponse *)resp_buffer;
    char *file_name = (char *)(resp_buffer + sizeof(DownloadResponse));
    if (resp->name_len > 0 && resp->name_len < BUFFER_SIZE - sizeof(DownloadResponse)) {
        file_name[resp->name_len] = '\0';
    } else {
        file_name[0] = '\0';
    }
    
    if (resp->status != STATUS_OK) {
        printf("[DEBUG] Download rejected for file: %s (Status: %d)\n", file_name, resp->status);
        return;
    }
    
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock < 0) {
        perror("[DEBUG] TCP socket creation failed");
        return;
    }
    
    struct sockaddr_in tcp_addr;
    memcpy(&tcp_addr, &server_addr, sizeof(tcp_addr));
    tcp_addr.sin_port = htons(SERVER_PORT + 1);
    
    printf("[DEBUG] Connecting to server for download on port %d\n", SERVER_PORT + 1);
    if (connect(tcp_sock, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr)) < 0) {
        perror("[DEBUG] TCP connect failed");
        close(tcp_sock);
        return;
    }
    
    int file_fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file_fd < 0) {
        perror("[DEBUG] open failed");
        close(tcp_sock);
        return;
    }
    
    uint8_t file_buffer[4096];
    ssize_t bytes_received;
    size_t bytes_remaining = resp->file_size;
    
    while (bytes_remaining > 0 && 
           (bytes_received = recv(tcp_sock, file_buffer, 
                                MIN((size_t)bytes_remaining, sizeof(file_buffer)), 0)) > 0) {
        if (write(file_fd, file_buffer, bytes_received) != bytes_received) {
            perror("[DEBUG] write failed");
            break;
        }
        bytes_remaining -= bytes_received;
    }
    
    close(file_fd);
    close(tcp_sock);
    if (bytes_remaining == 0) {
        printf("[DEBUG] Successfully downloaded file: %s (%zu bytes)\n", file_name, resp->file_size);
    } else {
        fprintf(stderr, "[DEBUG] Download incomplete: %s\n", file_name);
    }
}

void process_menu_choice(int choice) {
    char input_file[MAX_FILENAME_LEN] = {0};
    char output_file[MAX_FILENAME_LEN] = {0};
    char command[MAX_CMD_LEN] = {0};
    const char *files[1] = {input_file};
    
    printf("Enter input file: ");
    scanf("%255s", input_file);
    printf("Enter output file: ");
    scanf("%255s", output_file);
    
    switch (choice) {
        case 1: build_trim_command(input_file, output_file, command, sizeof(command)); break;
        case 2: build_resize_command(input_file, output_file, command, sizeof(command)); break;
        case 3: build_convert_command(input_file, output_file, command, sizeof(command)); break;
        case 4: build_extract_audio_command(input_file, output_file, command, sizeof(command)); break;
        case 5: build_extract_video_command(input_file, output_file, command, sizeof(command)); break;
        case 6: build_adjust_brightness_command(input_file, output_file, command, sizeof(command)); break;
        case 7: build_adjust_contrast_command(input_file, output_file, command, sizeof(command)); break;
        case 8: build_adjust_saturation_command(input_file, output_file, command, sizeof(command)); break;
        case 9: build_rotate_command(input_file, output_file, command, sizeof(command)); break;
        case 10: build_crop_command(input_file, output_file, command, sizeof(command)); break;
        case 11: build_add_watermark_command(input_file, output_file, command, sizeof(command)); break;
        case 12: build_add_subtitles_command(input_file, output_file, command, sizeof(command)); break;
        case 13: build_change_speed_command(input_file, output_file, command, sizeof(command)); break;
        case 14: build_reverse_command(input_file, output_file, command, sizeof(command)); break;
        case 15: build_extract_frame_command(input_file, output_file, command, sizeof(command)); break;
        case 16: build_create_gif_command(input_file, output_file, command, sizeof(command)); break;
        case 17: build_denoise_command(input_file, output_file, command, sizeof(command)); break;
        case 18: build_stabilize_command(input_file, output_file, command, sizeof(command)); break;
        case 19: build_merge_command(input_file, output_file, command, sizeof(command)); break;
        case 20: build_add_audio_command(input_file, output_file, command, sizeof(command)); break;
        default:
            printf("Invalid choice\n");
            return;
    }
    
    printf("Generated command: %s\n", command);
    uint32_t job_id = submit_job(command, files, 1);
    if (job_id != 0) {
        download_file(job_id, output_file);
    } else {
        fprintf(stderr, "Job submission failed, download aborted\n");
    }
}

int main() {
    srand(time(NULL));
    sockfd = init_udp_socket();
    download_sockfd = init_udp_socket();
    
    get_client_id();
    
    pthread_create(&heartbeat_tid, NULL, heartbeat_thread, NULL);
    
    while (1) {
        display_main_menu();
        int choice = get_menu_choice();
        
        if (choice == 0) {
            break;
        }
        
        process_menu_choice(choice);
    }
    
    close(sockfd);
    close(download_sockfd);
    return EXIT_SUCCESS;
}