#ifndef ADMIN_HANDLER_H
#define ADMIN_HANDLER_H

void *admin_thread(void *arg);
void log_append(const char *category, const char *fmt, ...);

#endif
