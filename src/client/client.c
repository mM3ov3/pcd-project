#include "client.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

int client_init(PCDClient *client, const char *server_ip) {
    // Create UDP socket
    client->udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (client->udp_sock < 0) {
        perror("socket");
        return -1;
    }

    // Set server address
    memset(&client->server_addr, 0, sizeof(client->server_addr));
    client->server_addr.sin_family = AF_INET;
    client->server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, server_ip, &client->server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(client->udp_sock);
        return -1;
    }

    client->connected = false;
    memset(client->client_id, 0, 16);

    return 0;
}

void client_cleanup(PCDClient *client) {
    close(client->udp_sock);
    if (client->tcp_sock > 0) {
        close(client->tcp_sock);
    }
}

int request_client_id(PCDClient *client) {
    ClientIdReq req = {
        .type = CLIENT_ID_REQ,
        .message_id = 1 // TODO: generate unique message_id
    };

    if (sendto(client->udp_sock, &req, sizeof(req), 0,
               (struct sockaddr *)&client->server_addr, sizeof(client->server_addr)) < 0) {
        perror("sendto");
        return -1;
    }

    // Wait for response
    struct pollfd fds[1];
    fds[0].fd = client->udp_sock;
    fds[0].events = POLLIN;

    char buffer[BUFFER_SIZE];
    int ret = poll(fds, 1, 5000); // 5 second timeout
    if (ret <= 0) {
        printf("Timeout waiting for client ID\n");
        return -1;
    }

    ssize_t n = recvfrom(client->udp_sock, buffer, BUFFER_SIZE, 0, NULL, NULL);
    if (n < 0) {
        perror("recvfrom");
        return -1;
    }

    if (n >= sizeof(ClientIdAck) && buffer[0] == CLIENT_ID_ACK) {
        ClientIdAck *ack = (ClientIdAck *)buffer;
        memcpy(client->client_id, ack->client_id, 16);
        client->connected = true;
        return 0;
    }

    return -1;
}

int send_heartbeat(PCDClient *client) {
    if (!client->connected) return -1;

    Heartbeat hb = {
        .type = HEARTBEAT
    };
    memcpy(hb.client_id, client->client_id, 16);

    if (sendto(client->udp_sock, &hb, sizeof(hb), 0,
               (struct sockaddr *)&client->server_addr, sizeof(client->server_addr)) < 0) {
        perror("sendto");
        return -1;
    }

    return 0;
}

int submit_job(PCDClient *client, const char *command, uint8_t file_count) {
    if (!client->connected) return -1;

    JobReq req = {
        .type = JOB_REQ,
        .message_id = 2, // TODO: generate unique message_id
        .file_count = file_count,
        .cmd_len = (uint16_t)strlen(command)
    };
    memcpy(req.client_id, client->client_id, 16);
    // TODO: generate job_id
    strncpy(req.job_command, command, MAX_CMD_LEN);

    if (sendto(client->udp_sock, &req, sizeof(req), 0,
               (struct sockaddr *)&client->server_addr, sizeof(client->server_addr)) < 0) {
        perror("sendto");
        return -1;
    }

    // Wait for JOB_ACK
    struct pollfd fds[1];
    fds[0].fd = client->udp_sock;
    fds[0].events = POLLIN;

    char buffer[BUFFER_SIZE];
    int ret = poll(fds, 1, 5000); // 5 second timeout
    if (ret <= 0) {
        printf("Timeout waiting for job ack\n");
        return -1;
    }

    ssize_t n = recvfrom(client->udp_sock, buffer, BUFFER_SIZE, 0, NULL, NULL);
    if (n < 0) {
        perror("recvfrom");
        return -1;
    }

    if (n >= sizeof(JobAck) && buffer[0] == JOB_ACK) {
        JobAck *ack = (JobAck *)buffer;
        if (ack->status) {
            return ack->job_id;
        }
    }

    return -1;
}

int request_upload(PCDClient *client, const char *filename, uint64_t file_size, uint32_t job_id) {
    if (!client->connected) return -1;

    UploadReq req = {
        .type = UPLOAD_REQ,
        .message_id = 3, // TODO: generate unique message_id
        .job_id = job_id,
        .file_size = file_size,
        .name_len = (uint8_t)strlen(filename)
    };
    memcpy(req.client_id, client->client_id, 16);
    strncpy(req.filename, filename, MAX_FILENAME_LEN);

    if (sendto(client->udp_sock, &req, sizeof(req), 0,
               (struct sockaddr *)&client->server_addr, sizeof(client->server_addr)) < 0) {
        perror("sendto");
        return -1;
    }

    // Wait for UPLOAD_ACK
    struct pollfd fds[1];
    fds[0].fd = client->udp_sock;
    fds[0].events = POLLIN;

    char buffer[BUFFER_SIZE];
    int ret = poll(fds, 1, 5000); // 5 second timeout
    if (ret <= 0) {
        printf("Timeout waiting for upload ack\n");
        return -1;
    }

    ssize_t n = recvfrom(client->udp_sock, buffer, BUFFER_SIZE, 0, NULL, NULL);
    if (n < 0) {
        perror("recvfrom");
        return -1;
    }

    if (n >= sizeof(UploadAck) && buffer[0] == UPLOAD_ACK) {
        UploadAck *ack = (UploadAck *)buffer;
        if (ack->status) {
            // Create TCP connection for upload
            client->tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
            if (client->tcp_sock < 0) {
                perror("socket");
                return -1;
            }

            struct sockaddr_in upload_addr = client->server_addr;
            upload_addr.sin_port = htons(ack->tcp_port);

            if (connect(client->tcp_sock, (struct sockaddr *)&upload_addr, sizeof(upload_addr)) < 0) {
                perror("connect");
                close(client->tcp_sock);
                client->tcp_sock = -1;
                return -1;
            }

            return 0;
        }
    }

    return -1;
}

int upload_file(PCDClient *client, const char *filename, uint32_t job_id) {
    if (client->tcp_sock <= 0) return -1;

    // Open file
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("fopen");
        return -1;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Send file header (optional)
    // TODO: Implement if needed

    // Send file data
    char buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (send(client->tcp_sock, buffer, bytes_read, 0) < 0) {
            perror("send");
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    close(client->tcp_sock);
    client->tcp_sock = -1;
    return 0;
}