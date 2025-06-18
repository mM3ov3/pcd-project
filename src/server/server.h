#ifndef SERVER_H
#define SERVER_H

#include "protocol.h"
#include <pthread.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 4096

typedef struct {
    uint8_t client_id[16];
    struct sockaddr_in addr;
    time_t last_heartbeat;
} ClientInfo;

typedef struct PendingJob {
    uint8_t client_id[16];
    uint32_t job_id;
    uint32_t files_to_upload;
    uint32_t files_arrived;
    uint32_t files_queued;
    time_t last_update;
    struct PendingJob *next;
} PendingJob;

typedef struct {
    int client_id;
    int job_id;
    char filename[MAX_FILENAME_LEN];
    double size_mb;
    time_t arrival_time;
    double priority;
} UploadJob;

typedef struct {
    UploadJob *data;
    int capacity;
    int size;
    pthread_mutex_t lock;
} MinHeap;

typedef struct HashMapNode {
    ClientInfo client;
    struct HashMapNode *next;
} HashMapNode;

typedef struct {
    HashMapNode **buckets;
    size_t bucket_count;
    size_t size;
    pthread_mutex_t lock;
} ClientHashMap;

// Server initialization
int server_init();
void server_run();
void server_cleanup();

// Client management
void handle_new_client(struct sockaddr_in *client_addr);
void handle_heartbeat(uint8_t client_id[16], struct sockaddr_in *client_addr);
void cleanup_dead_clients();

// Job management
void handle_job_request(JobReq *req, struct sockaddr_in *client_addr);
void handle_upload_request(UploadReq *req, struct sockaddr_in *client_addr);

// HashMap functions
void hashmap_init(ClientHashMap *map);
void hashmap_put(ClientHashMap *map, ClientInfo *client);
ClientInfo *hashmap_get(ClientHashMap *map, uint8_t client_id[16]);
void hashmap_remove(ClientHashMap *map, uint8_t client_id[16]);
void hashmap_free(ClientHashMap *map);

// MinHeap functions
void minheap_init(MinHeap *heap, int capacity);
void minheap_push(MinHeap *heap, UploadJob *job);
UploadJob *minheap_pop(MinHeap *heap);
void minheap_free(MinHeap *heap);

void *upload_thread_func(void *arg);

extern int sockfd;
extern ClientHashMap client_map;
extern MinHeap upload_queue;
extern pthread_t upload_thread;

#endif // SERVER_H