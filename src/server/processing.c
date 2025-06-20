#include "processing.h"
#include "admin_handler.h"
#include "job_handler.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

void init_processing() {
    printf("[DEBUG] Initializing processing module\n");
}

void process_pending_jobs(int sockfd) {
    char original_dir[512];
    if (getcwd(original_dir, sizeof(original_dir)) == NULL) {
        perror("[DEBUG] getcwd failed");
        return;
    }

    pthread_mutex_lock(&jobs_mutex);
    for (size_t i = 0; i < job_count; i++) {
        if (pending_jobs[i].files_received >= pending_jobs[i].file_count) {
            printf("[DEBUG] Processing job_id=%u, command=%s\n",
                   pending_jobs[i].job_id, pending_jobs[i].command);
			// Log start of job
			log_append("[PROCESSING]", "Starting job_id=%u for client_id=0x%02x0x%02x (command='%s')",
					pending_jobs[i].job_id,
					pending_jobs[i].client_id[0], pending_jobs[i].client_id[1],
					pending_jobs[i].command);

            
            // Change to job directory
            char dir_path[256];
            snprintf(dir_path, sizeof(dir_path), "processing/%02x%02x_%08x",
                     pending_jobs[i].client_id[0], pending_jobs[i].client_id[1],
                     pending_jobs[i].job_id);
            if (chdir(dir_path) != 0) {
                fprintf(stderr, "[DEBUG] chdir failed for %s: %s\n",
                        dir_path, strerror(errno));
                continue;
            }
            
            // Execute command
            int ret = system(pending_jobs[i].command);
            int status = ret == 0 ? STATUS_OK : STATUS_ERROR;
            const char *msg = ret == 0 ? "Job completed successfully" : "Job execution failed";

			// Log result
			log_append("[PROCESSING]", "Job_id=%u for client_id=0x%02x0x%02x completed with status=%s",
					pending_jobs[i].job_id,
					pending_jobs[i].client_id[0], pending_jobs[i].client_id[1],
					(status == STATUS_OK ? "OK" : "ERROR"));
            
            // Send JOB_RESULT
            JobResult result;
            result.type = JOB_RESULT;
            result.job_id = pending_jobs[i].job_id;
            result.status = status;
            result.msg_len = strlen(msg);
            
            size_t result_size = sizeof(result) + result.msg_len;
            uint8_t *send_buf = malloc(result_size);
            if (!send_buf) {
                fprintf(stderr, "[DEBUG] malloc failed for JOB_RESULT\n");
                chdir(original_dir);
                continue;
            }
            memcpy(send_buf, &result, sizeof(result));
            memcpy(send_buf + sizeof(result), msg, result.msg_len);
            
            // Use client_addr from PendingJob
            struct sockaddr_in client_addr = pending_jobs[i].client_addr;
            socklen_t addr_len = sizeof(client_addr);
            
            printf("[DEBUG] Sending JOB_RESULT for job_id=%u to %s:%d, status=%d\n",
                   pending_jobs[i].job_id, inet_ntoa(client_addr.sin_addr),
                   ntohs(client_addr.sin_port), status);
            if (sendto(sockfd, send_buf, result_size, 0,
                       (struct sockaddr *)&client_addr, addr_len) < 0) {
                perror("[DEBUG] sendto failed for JOB_RESULT");
            }
            free(send_buf);
            
            // Remove job
            memmove(&pending_jobs[i], &pending_jobs[i + 1],
                    (job_count - i - 1) * sizeof(PendingJob));
            job_count--;
            pending_jobs = realloc(pending_jobs, job_count * sizeof(PendingJob));
            
            // Restore directory
            if (chdir(original_dir) != 0) {
                fprintf(stderr, "[DEBUG] chdir back to %s failed: %s\n",
                        original_dir, strerror(errno));
            }
        }
    }
    pthread_mutex_unlock(&jobs_mutex);
}
