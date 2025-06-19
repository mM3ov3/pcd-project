#ifndef UPLOAD_HANDLER_H
#define UPLOAD_HANDLER_H

#include "protocol.h"
#include <sys/socket.h>
#include <netinet/in.h>

void init_upload_handler(int listen_fd);
void handle_upload_request(int udp_sock, UploadRequest *req, char *filename, struct sockaddr_in *client_addr);
void handle_download_request(int udp_sock, DownloadRequest *req, char *filename, struct sockaddr_in *client_addr);

#endif // UPLOAD_HANDLER_H