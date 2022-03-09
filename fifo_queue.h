#pragma once
#include <stddef.h>
#include <stdbool.h>
// FIFO queue based on doubly-linked list. Every node is malloc'd.
// Zero type safety because im lazy.

struct fifo_queue
{
    struct list_node* first;
    struct list_node* last;
    size_t data_size;
    size_t size;
};

void fifo_queue_init(struct fifo_queue*, size_t data_size);
void fifo_queue_destroy(struct fifo_queue*);

size_t fifo_queue_size(struct fifo_queue*);

void fifo_queue_enqueue(struct fifo_queue*, void* data);

// returns false if nothing to dequeue.
bool fifo_queue_dequeue(struct fifo_queue*, void* data_out);

void debug_traverse_queue(struct fifo_queue*);
void debug_traverse_queue_reverse(struct fifo_queue* q);
