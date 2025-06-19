#ifndef JOB_HANDLER_H
#define JOB_HANDLER_H

#include <stdint.h>
#include <pthread.h>
#include <netinet/in.h>
#include "common.h"
#include "protocol.h"

typedef struct {
    uint8_t client_id[16];
    struct sockaddr_in client_addr;
    uint32_t job_id;
    char command[MAX_CMD_LEN];
    int file_count;
    int files_received;
    time_t last_update;
} PendingJob;

extern PendingJob *pending_jobs;
extern size_t job_count;
extern pthread_mutex_t jobs_mutex;

void init_job_handler(void);
int create_job(const uint8_t *client_id, struct sockaddr_in *client_addr, uint32_t job_id, const char *command, int file_count);

#endif