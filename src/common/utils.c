#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <arpa/inet.h>
#include <curl/curl.h>

#define IP_BUF_SIZE 64


struct write_data {
	char *buffer;
	size_t size;
	size_t pos;
};

static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) 
{
	size_t total = size * nmemb;
	struct write_data *data = (struct write_data *)userdata;

	size_t space_left = data->size - data->pos - 1; // keep room for null terminator
	size_t to_copy = (total < space_left) ? total : space_left;

	if (to_copy > 0) {
		memcpy(data->buffer + data->pos, ptr, to_copy);
		data->pos += to_copy;
		data->buffer[data->pos] = '\0';  // null terminate
	}

	return total; // tell curl all bytes handled (even if truncated)
}

static void trim_trailing_whitespace(char *str) 
{
	size_t len = strlen(str);
	while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\n' || str[len-1] == '\r' || str[len-1] == '\t')) {
		str[len-1] = '\0';
		len--;
	}
}

uint32_t get_public_ip() 
{
	CURL *curl = curl_easy_init();
	if (!curl) {
		fprintf(stderr, "curl init failed\n");
		return 0;
	}

	char ip_buf[IP_BUF_SIZE] = {0};
	struct write_data data = {
		.buffer = ip_buf,
		.size = IP_BUF_SIZE,
		.pos = 0
	};

	curl_easy_setopt(curl, CURLOPT_URL, "https://api.ipify.org");
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	if (res != CURLE_OK) {
		fprintf(stderr, "curl failed: %s\n", curl_easy_strerror(res));
		return 0;
	}

	trim_trailing_whitespace(ip_buf);

	struct in_addr addr;
	if (inet_pton(AF_INET, ip_buf, &addr) != 1) {
		fprintf(stderr, "inet_pton failed on '%s'\n", ip_buf);
		return 0;
	}

	return addr.s_addr;  // network byte order
}

void generate_uuid(uint8_t *uuid) 
{
	uuid_generate_random(uuid);
}


int create_directory(const char *path) 
{
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
