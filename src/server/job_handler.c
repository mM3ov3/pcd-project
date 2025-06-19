#include "job_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

PendingJob *pending_jobs = NULL;
size_t job_count = 0;
pthread_mutex_t jobs_mutex = PTHREAD_MUTEX_INITIALIZER;

void init_job_handler(void) {
    printf("[DEBUG] Initializing job handler\n");
}

int create_job(const uint8_t *client_id, struct sockaddr_in *client_addr, uint32_t job_id, const char *command, int file_count) {
    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "processing/%02x%02x_%08x",
             client_id[0], client_id[1], job_id);
    
    printf("[DEBUG] Creating job directory: %s\n", dir_path);
    
    // Check if directory exists
    struct stat st;
    if (stat(dir_path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            printf("[DEBUG] Reusing existing directory: %s\n", dir_path);
        } else {
            fprintf(stderr, "[DEBUG] Path %s exists but is not a directory\n", dir_path);
            return 0;
        }
    } else {
        if (mkdir(dir_path, 0777) != 0) {
            fprintf(stderr, "[DEBUG] mkdir failed for %s: %s\n", dir_path, strerror(errno));
            return 0;
        }
        printf("[DEBUG] Job directory created successfully: %s\n", dir_path);
    }
    
    pthread_mutex_lock(&jobs_mutex);
    
    PendingJob *new_jobs = realloc(pending_jobs, (job_count + 1) * sizeof(PendingJob));
    if (!new_jobs) {
        fprintf(stderr, "[DEBUG] realloc failed for pending_jobs: %s\n", strerror(errno));
        pthread_mutex_unlock(&jobs_mutex);
        return 0;
    }
    pending_jobs = new_jobs;
    
    memcpy(pending_jobs[job_count].client_id, client_id, 16);
    pending_jobs[job_count].client_addr = *client_addr;
    pending_jobs[job_count].job_id = job_id;
    strncpy(pending_jobs[job_count].command, command, MAX_CMD_LEN - 1);
    pending_jobs[job_count].command[MAX_CMD_LEN - 1] = '\0';
    pending_jobs[job_count].file_count = file_count;
    pending_jobs[job_count].files_received = 0;
    pending_jobs[job_count].last_update = time(NULL);
    
    job_count++;
    
    printf("[DEBUG] Job %u created: client_id=%02x%02x, command=%s, file_count=%d\n",
           job_id, client_id[0], client_id[1], command, file_count);
    
    pthread_mutex_unlock(&jobs_mutex);
    return 1;
}