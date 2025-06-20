#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define SERVER_PORT 5555
#define HEARTBEAT_INTERVAL 10
#define HEARTBEAT_TIMEOUT 30
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
    JOB_RESULT,
    DOWNLOAD_REQ,
    DOWNLOAD_ACK
} MessageType;

// Status codes
typedef enum {
    STATUS_OK = 0,
    STATUS_ERROR,
    STATUS_INVALID_REQUEST,
    STATUS_JOB_EXISTS,
    STATUS_UPLOAD_LIMIT,
    STATUS_FILE_NOT_FOUND
} StatusCode;

// Client ID request (C->S)
typedef struct {
    uint8_t type;       // CLIENT_ID_REQ
    uint32_t message_id;
} ClientIdRequest;

// Client ID response (S->C)
typedef struct {
    uint8_t type;       // CLIENT_ID_ACK
    uint32_t message_id;
    uint8_t client_id[16];
} ClientIdResponse;

// Heartbeat (C->S)
typedef struct {
    uint8_t type;       // HEARTBEAT
    uint8_t client_id[16];
} Heartbeat;

// Job request (C->S)
typedef struct {
    uint8_t type;       // JOB_REQ
    uint32_t message_id;
    uint8_t client_id[16];
    uint32_t job_id;
    uint8_t file_count;
    uint16_t cmd_len;
    // Followed by job_command string (variable length)
} JobRequest;

// Job acknowledgement (S->C)
typedef struct {
    uint8_t type;       // JOB_ACK
    uint32_t message_id;
    uint32_t job_id;
    uint8_t status;
    uint16_t msg_len;
    // Followed by message string (variable length)
} JobResponse;

// Upload request (C->S)
typedef struct {
    uint8_t type;       // UPLOAD_REQ
    uint32_t message_id;
    uint8_t client_id[16];
    uint32_t job_id;
    uint64_t file_size;
    uint16_t name_len;
    // Followed by filename string (variable length)
} UploadRequest;

// Upload acknowledgement (S->C)
typedef struct {
    uint8_t type;       // UPLOAD_ACK
    uint32_t message_id;
    uint16_t name_len;
    // Followed by file_name string (variable length)
    uint8_t status;
    uint32_t ip_address;
    uint16_t tcp_port;
} UploadResponse;

// Job result (S->C)
typedef struct {
    uint8_t type;       // JOB_RESULT
    uint32_t message_id;
    uint8_t client_id[16];
    uint32_t job_id;
    uint8_t status;
    uint16_t msg_len;
    // Followed by message string (variable length)
} JobResult;

// Download request (C->S)
typedef struct {
    uint8_t type;       // DOWNLOAD_REQ
    uint32_t message_id;
    uint8_t client_id[16];
    uint32_t job_id;
    uint16_t name_len;
    // Followed by filename string (variable length)
} DownloadRequest;

// Download acknowledgement (S->C)
typedef struct {
    uint8_t type;       // DOWNLOAD_ACK
    uint32_t message_id;
    uint8_t status;
    uint64_t file_size;
    uint16_t name_len;
    // Followed by filename string (variable length)
} DownloadResponse;

#endif // PROTOCOL_H
