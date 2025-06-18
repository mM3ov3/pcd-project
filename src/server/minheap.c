#include "server.h"
#include <stdlib.h>

void minheap_init(MinHeap *heap, int capacity) {
    heap->capacity = capacity;
    heap->size = 0;
    heap->data = malloc(capacity * sizeof(UploadJob));
    pthread_mutex_init(&heap->lock, NULL);
}

void minheap_push(MinHeap *heap, UploadJob *job) {
    if (heap->size >= heap->capacity) return;
    
    heap->data[heap->size] = *job;
    heapify_up(heap, heap->size);
    heap->size++;
}

UploadJob *minheap_pop(MinHeap *heap) {
    if (heap->size == 0) return NULL;
    
    UploadJob *result = malloc(sizeof(UploadJob));
    *result = heap->data[0];
    
    heap->data[0] = heap->data[heap->size - 1];
    heap->size--;
    heapify_down(heap, 0);
    
    return result;
}

void heapify_up(MinHeap *heap, int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (heap->data[index].priority >= heap->data[parent].priority) break;
        
        UploadJob temp = heap->data[index];
        heap->data[index] = heap->data[parent];
        heap->data[parent] = temp;
        
        index = parent;
    }
}

void heapify_down(MinHeap *heap, int index) {
    while (1) {
        int left = 2 * index + 1;
        int right = 2 * index + 2;
        int smallest = index;
        
        if (left < heap->size && 
            heap->data[left].priority < heap->data[smallest].priority) {
            smallest = left;
        }
        
        if (right < heap->size && 
            heap->data[right].priority < heap->data[smallest].priority) {
            smallest = right;
        }
        
        if (smallest == index) break;
        
        UploadJob temp = heap->data[index];
        heap->data[index] = heap->data[smallest];
        heap->data[smallest] = temp;
        
        index = smallest;
    }
}

void minheap_free(MinHeap *heap) {
    free(heap->data);
    pthread_mutex_destroy(&heap->lock);
}