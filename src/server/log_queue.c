#include "log_queue.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

void log_queue_init(LogQueue *q) {
	q->head = 0;
	q->tail = 0;
	q->count = 0;
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->cond, NULL);
}

// Push a log entry into the queue (thread-safe)
void log_queue_push(LogQueue *q, const char *entry) {
	pthread_mutex_lock(&q->mutex);
	if (q->count == LOG_QUEUE_SIZE) {
		// Overwrite oldest if full
		q->head = (q->head + 1) % LOG_QUEUE_SIZE;
		q->count--;
	}
	strncpy(q->entries[q->tail], entry, LOG_ENTRY_MAX - 1);
	q->entries[q->tail][LOG_ENTRY_MAX - 1] = '\0';
	q->tail = (q->tail + 1) % LOG_QUEUE_SIZE;
	q->count++;
	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex);
}

// Pop a log entry, blocking until available or timeout
// timeout_ms: -1 = wait forever, else timeout in milliseconds
// returns 1 if got entry, 0 if timeout, -1 on error
int log_queue_pop_timed(LogQueue *q, char *buffer, int timeout_ms) {
	struct timespec ts;
	int ret = 0;

	pthread_mutex_lock(&q->mutex);
	while (q->count == 0) {
		if (timeout_ms < 0) {
			ret = pthread_cond_wait(&q->cond, &q->mutex);
		} else {
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += timeout_ms / 1000;
			ts.tv_nsec += (timeout_ms % 1000) * 1000000;
			if (ts.tv_nsec >= 1000000000) {
				ts.tv_sec++;
				ts.tv_nsec -= 1000000000;
			}
			ret = pthread_cond_timedwait(&q->cond, &q->mutex, &ts);
			if (ret == ETIMEDOUT) {
				pthread_mutex_unlock(&q->mutex);
				return 0; // timeout
			}
		}
		if (ret != 0) {
			pthread_mutex_unlock(&q->mutex);
			return -1; // error
		}
	}
	strncpy(buffer, q->entries[q->head], LOG_ENTRY_MAX);
	q->head = (q->head + 1) % LOG_QUEUE_SIZE;
	q->count--;
	pthread_mutex_unlock(&q->mutex);
	return 1;
}
