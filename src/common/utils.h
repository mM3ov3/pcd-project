#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <time.h>

void generate_uuid(uint8_t *uuid);
uint32_t get_public_ip();

int create_directory(const char *path);
time_t get_current_time();
double compute_priority(double file_size, time_t arrival_time, double aging_factor);

#endif // UTILS_H
