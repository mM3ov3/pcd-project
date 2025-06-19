#ifndef LOG_QUEUE
#define LOG_QUEUE
#include <pthread.h>

#define LOG_QUEUE_SIZE 100
#define LOG_ENTRY_MAX 512

typedef struct {
	char entries[LOG_QUEUE_SIZE][LOG_ENTRY_MAX];
	int head; // next read index
	int tail; // next write index
	int count;

	pthread_mutex_t mutex;
	pthread_cond_t cond;
} LogQueue;

void log_queue_init(LogQueue *q);
void log_queue_push(LogQueue *q, const char *entry);
int log_queue_pop_timed(LogQueue *q, char *buffer, int timeout_ms);
#endif
