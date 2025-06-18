#include "server.h"
#include <stdlib.h>
#include <string.h>

void hashmap_init(ClientHashMap *map) {
    map->bucket_count = 16;
    map->size = 0;
    map->buckets = calloc(map->bucket_count, sizeof(HashMapNode *));
    pthread_mutex_init(&map->lock, NULL);
}

void hashmap_put(ClientHashMap *map, ClientInfo *client) {
    // TODO: Implement hashmap put operation
}

ClientInfo *hashmap_get(ClientHashMap *map, uint8_t client_id[16]) {
    // TODO: Implement hashmap get operation
    return NULL;
}

void hashmap_remove(ClientHashMap *map, uint8_t client_id[16]) {
    // TODO: Implement hashmap remove operation
}

void hashmap_free(ClientHashMap *map) {
    // TODO: Implement hashmap cleanup
    pthread_mutex_destroy(&map->lock);
}