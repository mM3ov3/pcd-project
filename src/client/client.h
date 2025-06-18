#ifndef CLIENT_H
#define CLIENT_H

#include "protocol.h"
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 4096

typedef struct {
    uint8_t client_id[16];
    struct sockaddr_in server_addr;
    int udp_sock;
    int tcp_sock;
    bool connected;
} PCDClient;

// Client initialization
int client_init(PCDClient *client, const char *server_ip);
void client_cleanup(PCDClient *client);

// Protocol functions
int request_client_id(PCDClient *client);
int send_heartbeat(PCDClient *client);
int submit_job(PCDClient *client, const char *command, uint8_t file_count);
int request_upload(PCDClient *client, const char *filename, uint64_t file_size, uint32_t job_id);
int upload_file(PCDClient *client, const char *filename, uint32_t job_id);

#endif // CLIENT_H