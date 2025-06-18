#include "server.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

void handle_new_client(struct sockaddr_in *client_addr) {
    ClientInfo client;
    generate_uuid(client.client_id);
    client.addr = *client_addr;
    client.last_heartbeat = get_current_time();

    pthread_mutex_lock(&client_map.lock);
    hashmap_put(&client_map, &client);
    pthread_mutex_unlock(&client_map.lock);

    // Send CLIENT_ID_ACK
    ClientIdAck ack = {
        .type = CLIENT_ID_ACK,
        .message_id = 0, // TODO: generate unique message_id
    };
    memcpy(ack.client_id, client.client_id, 16);

    sendto(sockfd, &ack, sizeof(ack), 0,
           (struct sockaddr *)client_addr, sizeof(*client_addr));
}

void handle_heartbeat(uint8_t client_id[16], struct sockaddr_in *client_addr) {
    pthread_mutex_lock(&client_map.lock);
    ClientInfo *client = hashmap_get(&client_map, client_id);
    if (client) {
        client->addr = *client_addr;
        client->last_heartbeat = get_current_time();
    }
    pthread_mutex_unlock(&client_map.lock);
}

void cleanup_dead_clients() {
    time_t now = get_current_time();
    pthread_mutex_lock(&client_map.lock);
    
    // Iterate through all clients and remove inactive ones
    for (size_t i = 0; i < client_map.bucket_count; i++) {
        // Implementation depends on your hashmap structure
    }
    
    pthread_mutex_unlock(&client_map.lock);
}

void handle_job_request(JobReq *req, struct sockaddr_in *client_addr) {
    // Create job directory
    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "processing/%.16s_%u", req->client_id, req->job_id);
    create_directory(dir_path);

    // Create job spec file
    char spec_path[256];
    snprintf(spec_path, sizeof(spec_path), "%s/job_spec.json", dir_path);
    FILE *fp = fopen(spec_path, "w");
    if (fp) {
        fprintf(fp, "{\"command\":\"%.*s\",\"file_count\":%d}", 
                req->cmd_len, req->job_command, req->file_count);
        fclose(fp);
    }

    // Add to pending jobs
    PendingJob job = {
        .client_id = {0},
        .job_id = req->job_id,
        .files_to_upload = req->file_count,
        .files_arrived = 0,
        .files_queued = 0,
        .last_update = get_current_time()
    };
    memcpy(job.client_id, req->client_id, 16);

    // TODO: Add to pending jobs hashmap

    // Send JOB_ACK
    JobAck ack = {
        .type = JOB_ACK,
        .message_id = req->message_id,
        .job_id = req->job_id,
        .status = 1, // Success
        .msg_len = 0
    };
    
    sendto(sockfd, &ack, sizeof(ack), 0,
           (struct sockaddr *)client_addr, sizeof(*client_addr));
}

void handle_upload_request(UploadReq *req, struct sockaddr_in *client_addr) {
    // Create upload job
    UploadJob job = {
        .client_id = 0, // TODO: convert client_id to int
        .job_id = req->job_id,
        .size_mb = req->file_size / (1024.0 * 1024.0),
        .arrival_time = get_current_time(),
        .priority = 0.0
    };
    strncpy(job.filename, req->filename, MAX_FILENAME_LEN);
    job.priority = compute_priority(job.size_mb, job.arrival_time, 0.001);

    // Add to upload queue
    pthread_mutex_lock(&upload_queue.lock);
    minheap_push(&upload_queue, &job);
    pthread_mutex_unlock(&upload_queue.lock);

    // TODO: Update pending job count

    // Send UPLOAD_ACK (assuming we can process immediately)
    UploadAck ack = {
        .type = UPLOAD_ACK,
        .message_id = req->message_id,
        .name_len = req->name_len,
        .status = 1, // Accepted
        .ip_address = get_public_ip(),
        .tcp_port = SERVER_PORT + 1 // Different port for uploads
    };
    strncpy(ack.file_name, req->filename, MAX_FILENAME_LEN);
    
    sendto(sockfd, &ack, sizeof(ack), 0,
           (struct sockaddr *)client_addr, sizeof(*client_addr));
}
