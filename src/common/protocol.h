#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define SERVER_PORT 5555
#define MAX_UPLOADS 10
#define HEARTBEAT_TIMEOUT 30
#define HEARTBEAT_INTERVAL 10
#define MAX_FILENAME_LEN 256
#define MAX_CMD_LEN 1024

// Message types
typedef enum {
    CLIENT_ID_REQ = 1,
    CLIENT_ID_ACK,
    HEARTBEAT,
    JOB_REQ,
    JOB_ACK,
    UPLOAD_REQ,
    UPLOAD_ACK,
    JOB_RESULT
} MessageType;

// Message structures
#pragma pack(push, 1)
typedef struct {
    uint8_t type;
    uint32_t message_id;
} ClientIdReq;

typedef struct {
    uint8_t type;
    uint32_t message_id;
    uint8_t client_id[16];
} ClientIdAck;

typedef struct {
    uint8_t type;
    uint8_t client_id[16];
} Heartbeat;

typedef struct {
    uint8_t type;
    uint32_t message_id;
    uint8_t client_id[16];
    uint32_t job_id;
    uint8_t file_count;
    uint16_t cmd_len;
    char job_command[MAX_CMD_LEN];
} JobReq;

typedef struct {
    uint8_t type;
    uint32_t message_id;
    uint32_t job_id;
    uint8_t status;
    uint8_t msg_len;
    char message[MAX_CMD_LEN];
} JobAck;

typedef struct {
    uint8_t type;
    uint32_t message_id;
    uint8_t client_id[16];
    uint32_t job_id;
    uint64_t file_size;
    uint8_t name_len;
    char filename[MAX_FILENAME_LEN];
} UploadReq;

typedef struct {
    uint8_t type;
    uint32_t message_id;
    uint8_t name_len;
    char file_name[MAX_FILENAME_LEN];
    uint8_t status;
    uint32_t ip_address;
    uint16_t tcp_port;
} UploadAck;

typedef struct {
    uint8_t type;
    uint32_t message_id;
    uint8_t client_id[16];
    uint32_t job_id;
    uint8_t status;
    uint8_t msg_len;
    char message[MAX_CMD_LEN];
} JobResult;
#pragma pack(pop)

#endif // PROTOCOL_H
