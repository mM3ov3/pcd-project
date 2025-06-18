#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>

void generate_uuid(uint8_t *uuid) {
    uuid_generate_random(uuid);
}

int create_directory(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        return mkdir(path, 0700);
    }
    return 0;
}

time_t get_current_time() {
    return time(NULL);
}

double compute_priority(double file_size, time_t arrival_time, double aging_factor) {
    time_t wait_time = difftime(get_current_time(), arrival_time);
    return (1.0 / file_size) + (wait_time * aging_factor);
}