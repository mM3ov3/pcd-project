#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>
#include <pthread.h>
#include <netinet/in.h>
#include "protocol.h"

typedef struct {
    uint8_t client_id[16];
    struct sockaddr_in addr;
    time_t last_heartbeat;
} ClientInfo;

typedef struct {
    uint8_t client_id[16];
    uint32_t job_id;
    char filename[MAX_FILENAME_LEN];
    uint32_t message_id;
    struct sockaddr_in client_addr;
} DownloadJob;

typedef struct {
    DownloadJob *jobs;
    int size;
    int capacity;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} DownloadQueue;

extern ClientInfo *clients;
extern size_t client_count;
extern pthread_mutex_t clients_mutex;
extern DownloadQueue download_queue;

#endif