#include "upload_handler.h"
#include "job_handler.h"
#include "common.h"
#include "server.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>

typedef struct {
    uint8_t client_id[16];
    uint32_t job_id;
    char filename[MAX_FILENAME_LEN];
    uint64_t file_size;
    time_t arrival_time;
    double priority;
} UploadJob;

typedef struct {
    UploadJob *jobs;
    int size;
    int capacity;
    pthread_mutex_t mutex;
} UploadQueue;

static void *upload_thread_func(void *arg);
static void process_upload(UploadJob *job);

extern pthread_mutex_t jobs_mutex;
extern size_t job_count;
extern PendingJob *pending_jobs;

static UploadQueue upload_queue;
static int tcp_listen_fd;
static int active_uploads = 0;
static pthread_mutex_t active_uploads_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t upload_available = PTHREAD_COND_INITIALIZER;
static pthread_t upload_threads[MAX_UPLOADS];

void init_upload_handler(int listen_fd) {
    tcp_listen_fd = listen_fd;
    upload_queue.jobs = malloc(10 * sizeof(UploadJob));
    upload_queue.capacity = 10;
    upload_queue.size = 0;
    pthread_mutex_init(&upload_queue.mutex, NULL);

    printf("[DEBUG] Initializing upload handler, listen_fd=%d\n", listen_fd);

    for (int i = 0; i < MAX_UPLOADS; i++) {
        printf("[DEBUG] Creating upload thread %d\n", i);
        pthread_create(&upload_threads[i], NULL, upload_thread_func, NULL);
    }
}

static void *upload_thread_func(void *arg) {
    (void)arg;

    printf("[DEBUG] Upload thread started (tid=%lu)\n", pthread_self());

    while (1) {
        pthread_mutex_lock(&upload_queue.mutex);

        while (upload_queue.size == 0 || active_uploads >= MAX_UPLOADS) {
            printf("[DEBUG] Thread %lu waiting: queue size=%d, active_uploads=%d\n",
                   pthread_self(), upload_queue.size, active_uploads);
            pthread_cond_wait(&upload_available, &upload_queue.mutex);
        }

        UploadJob job = upload_queue.jobs[0];
        memmove(&upload_queue.jobs[0], &upload_queue.jobs[1],
               (upload_queue.size - 1) * sizeof(UploadJob));
        upload_queue.size--;

        pthread_mutex_lock(&active_uploads_mutex);
        active_uploads++;
        printf("[DEBUG] Thread %lu picked job: job_id=%u, filename=%s, active_uploads=%d\n",
               pthread_self(), job.job_id, job.filename, active_uploads);
        pthread_mutex_unlock(&active_uploads_mutex);

        pthread_mutex_unlock(&upload_queue.mutex);

        process_upload(&job);

        pthread_mutex_lock(&active_uploads_mutex);
        active_uploads--;
        printf("[DEBUG] Thread %lu finished job: job_id=%u, filename=%s, active_uploads=%d\n",
               pthread_self(), job.job_id, job.filename, active_uploads);
        pthread_cond_signal(&upload_available);
        pthread_mutex_unlock(&active_uploads_mutex);
    }
    return NULL;
}

static void process_upload(UploadJob *job) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    // Find client address from PendingJob
    pthread_mutex_lock(&jobs_mutex);
    int found = 0;
    for (size_t i = 0; i < job_count; i++) {
        if (memcmp(pending_jobs[i].client_id, job->client_id, 16) == 0 &&
            pending_jobs[i].job_id == job->job_id) {
            client_addr = pending_jobs[i].client_addr;
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&jobs_mutex);
    
    if (!found) {
        fprintf(stderr, "[DEBUG] No job found for upload: job_id=%u\n", job->job_id);
        return;
    }
    
    printf("[DEBUG] process_upload: Waiting for client connection for job_id=%u, filename=%s from %s:%d\n",
           job->job_id, job->filename, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    
    int client_fd = accept(tcp_listen_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
        perror("[DEBUG] accept failed");
        return;
    }
    printf("[DEBUG] Accepted TCP connection from %s:%d for job_id=%u, filename=%s\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), job->job_id, job->filename);

    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "processing/%02x%02x_%08x",
             job->client_id[0], job->client_id[1], job->job_id);

    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, job->filename);
    int file_fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file_fd < 0) {
        perror("[DEBUG] open failed");
        close(client_fd);
        return;
    }
    printf("[DEBUG] Receiving file: %s (size=%lu bytes)\n", file_path, job->file_size);

    uint8_t buffer[4096];
    ssize_t bytes_received;
    uint64_t bytes_remaining = job->file_size;
    uint64_t total_received = 0;

    while (bytes_remaining > 0) {
        size_t to_read = bytes_remaining > sizeof(buffer) ? sizeof(buffer) : (size_t)bytes_remaining;
        bytes_received = recv(client_fd, buffer, to_read, 0);

        if (bytes_received <= 0) {
            if (bytes_received < 0) {
                perror("[DEBUG] recv failed");
            }
            printf("[DEBUG] Connection closed or error during recv for job_id=%u\n", job->job_id);
            break;
        }

        if (write(file_fd, buffer, bytes_received) != bytes_received) {
            perror("[DEBUG] write failed");
            break;
        }

        bytes_remaining -= bytes_received;
        total_received += bytes_received;
        printf("[DEBUG] Received %zd bytes, %lu bytes remaining for job_id=%u\n",
               bytes_received, bytes_remaining, job->job_id);
    }

    printf("[DEBUG] File transfer complete for job_id=%u, total_received=%lu\n",
           job->job_id, total_received);

    close(file_fd);
    close(client_fd);

    pthread_mutex_lock(&jobs_mutex);
    for (size_t i = 0; i < job_count; i++) {
        if (memcmp(pending_jobs[i].client_id, job->client_id, 16) == 0 &&
            pending_jobs[i].job_id == job->job_id) {
            pending_jobs[i].files_received++;
            pending_jobs[i].last_update = time(NULL);
            printf("[DEBUG] Updated pending job: job_id=%u, files_received=%d\n",
                   job->job_id, pending_jobs[i].files_received);
            break;
        }
    }
    pthread_mutex_unlock(&jobs_mutex);
}

void handle_upload_request(int udp_sock, UploadRequest *req, char *filename,
                         struct sockaddr_in *client_addr) {
    UploadJob job;
    memcpy(job.client_id, req->client_id, 16);
    job.job_id = req->job_id;
    strncpy(job.filename, filename, sizeof(job.filename));
    job.filename[sizeof(job.filename)-1] = '\0';
    job.file_size = req->file_size;
    job.arrival_time = time(NULL);
    job.priority = 1.0 / (double)req->file_size;

    printf("[DEBUG] handle_upload_request: job_id=%u, filename=%s, file_size=%lu, priority=%f\n",
           job.job_id, job.filename, job.file_size, job.priority);

    pthread_mutex_lock(&upload_queue.mutex);

    if (upload_queue.size == upload_queue.capacity) {
        upload_queue.capacity *= 2;
        upload_queue.jobs = realloc(upload_queue.jobs,
                                  upload_queue.capacity * sizeof(UploadJob));
        printf("[DEBUG] upload_queue resized: new capacity=%d\n", upload_queue.capacity);
    }

    int i;
    for (i = upload_queue.size - 1; i >= 0; i--) {
        if (job.priority > upload_queue.jobs[i].priority) {
            upload_queue.jobs[i+1] = upload_queue.jobs[i];
        } else {
            break;
        }
    }
    upload_queue.jobs[i+1] = job;
    upload_queue.size++;

    printf("[DEBUG] Job enqueued: job_id=%u, filename=%s, queue size=%d\n",
           job.job_id, job.filename, upload_queue.size);

    pthread_cond_signal(&upload_available);
    pthread_mutex_unlock(&upload_queue.mutex);

    UploadResponse resp;
    resp.type = UPLOAD_ACK;
    resp.message_id = req->message_id;
    resp.name_len = strlen(filename);
    resp.status = STATUS_OK;
    resp.ip_address = client_addr->sin_addr.s_addr;
    resp.tcp_port = htons(SERVER_PORT);

    size_t resp_size = sizeof(resp) + resp.name_len;
    uint8_t *send_buf = malloc(resp_size);
    memcpy(send_buf, &resp, sizeof(resp));
    memcpy(send_buf + sizeof(resp), filename, resp.name_len);

    printf("[DEBUG] Sending UPLOAD_ACK to %s:%d for job_id=%u\n",
           inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port), job.job_id);

    sendto(udp_sock, send_buf, resp_size, 0,
          (struct sockaddr *)client_addr, sizeof(*client_addr));
    free(send_buf);
}

void handle_download_request(int udp_sock, DownloadRequest *req, char *filename,
                           struct sockaddr_in *client_addr) {
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "processing/%02x%02x_%08x/%s",
             req->client_id[0], req->client_id[1], req->job_id, filename);

    printf("[DEBUG] handle_download_request: job_id=%u, filename=%s, file_path=%s\n",
           req->job_id, filename, file_path);

    struct stat st;
    if (stat(file_path, &st) != 0) {
        fprintf(stderr, "[DEBUG] stat failed for %s: %s\n", file_path, strerror(errno));
        DownloadResponse resp;
        resp.type = DOWNLOAD_ACK;
        resp.message_id = req->message_id;
        resp.status = STATUS_FILE_NOT_FOUND;
        resp.file_size = 0;
        resp.name_len = strlen(filename);

        size_t resp_size = sizeof(resp) + resp.name_len;
        uint8_t *send_buf = malloc(resp_size);
        memcpy(send_buf, &resp, sizeof(resp));
        memcpy(send_buf + sizeof(resp), filename, resp.name_len);

        sendto(udp_sock, send_buf, resp_size, 0,
              (struct sockaddr *)client_addr, sizeof(*client_addr));
        free(send_buf);
        return;
    }

    // Ensure file is readable
    if (chmod(file_path, 0666) != 0) {
        fprintf(stderr, "[DEBUG] chmod failed for %s: %s\n", file_path, strerror(errno));
    }

    DownloadResponse resp;
    resp.type = DOWNLOAD_ACK;
    resp.message_id = req->message_id;
    resp.status = STATUS_OK;
    resp.file_size = st.st_size;
    resp.name_len = strlen(filename);

    size_t resp_size = sizeof(resp) + resp.name_len;
    uint8_t *send_buf = malloc(resp_size);
    memcpy(send_buf, &resp, sizeof(resp));
    memcpy(send_buf + sizeof(resp), filename, resp.name_len);

    printf("[DEBUG] Sending DOWNLOAD_ACK to %s:%d for job_id=%u, filename=%s, file_size=%lu\n",
           inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port),
           req->job_id, filename, (unsigned long)st.st_size);

    sendto(udp_sock, send_buf, resp_size, 0,
          (struct sockaddr *)client_addr, sizeof(*client_addr));
    free(send_buf);

    // Enqueue download job
    DownloadJob job;
    memcpy(job.client_id, req->client_id, 16);
    job.job_id = req->job_id;
    strncpy(job.filename, filename, MAX_FILENAME_LEN - 1);
    job.filename[MAX_FILENAME_LEN - 1] = '\0';
    job.message_id = req->message_id;
    job.client_addr = *client_addr;

    pthread_mutex_lock(&download_queue.mutex);
    if (download_queue.size == download_queue.capacity) {
        download_queue.capacity *= 2;
        download_queue.jobs = realloc(download_queue.jobs, 
                                   download_queue.capacity * sizeof(DownloadJob));
        printf("[DEBUG] Download queue resized: new capacity=%d\n", 
                   download_queue.capacity);
    }
    download_queue.jobs[download_queue.size++] = job;
    pthread_cond_signal(&download_queue.cond);
    pthread_mutex_unlock(&download_queue.mutex);

    printf("[DEBUG] Enqueued download job for job_id=%u, filename=%s\n",
           job.job_id, job.filename);
}