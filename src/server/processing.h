#ifndef PROCESSING_H
#define PROCESSING_H

#include <stdint.h>
#include "protocol.h"

void init_processing(void);
void process_pending_jobs(int sockfd);

#endif