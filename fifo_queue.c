#include "fifo_queue.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

struct list_node
{
    struct list_node* prev;
    struct list_node* next;
    char data[0];
};


void fifo_queue_init(struct fifo_queue* q, size_t data_size)
{
    q->first = NULL;
    q->last = NULL;
    q->data_size = data_size;
    q->size = 0;
}

size_t fifo_queue_size(struct fifo_queue* q)
{
    return q->size;
}

void fifo_queue_enqueue(struct fifo_queue* q, void* data)
{
    // allocate node
    struct list_node* new_node = malloc(sizeof(struct fifo_queue) + q->data_size);

    memcpy(new_node->data, data, q->data_size);
    new_node->prev = NULL;
    new_node->next = NULL;

    // first will be dequeued, last will be enqueued.
    // first node case.
    if(q->last == NULL)
    {
        q->first = new_node;
        q->last = new_node;
    }
    else
    {
        struct list_node* old_last = q->last;

        old_last->next = new_node;
        new_node->prev = old_last;

        q->last = new_node;
    }

    ++q->size;
}

bool fifo_queue_dequeue(struct fifo_queue* q, void* data_out)
{
    if(q->first == NULL)
    {
        return false;
    }

    // pop front element.
    struct list_node* to_pop = q->first;
    struct list_node* next_in_line = to_pop->next;
    if(next_in_line)
    {
        next_in_line->prev = NULL;
        q->first = next_in_line;
    }
    else
    {
        q->first = NULL;
        q->last = NULL;
    }

    memcpy(data_out, to_pop->data, q->data_size);
    free(to_pop);
    --q->size;
}

void debug_traverse_queue(struct fifo_queue* q)
{
    if(!q->first)
        return;

    struct list_node* elem = q->first;
    while(elem != NULL)
    {
        printf("Debug node traversal.\n prev: %p, next: %p, data: %p\n",
                elem->prev, elem->next, elem->data);

        elem = elem->next;
    }
}

void debug_traverse_queue_reverse(struct fifo_queue* q)
{
    if(!q->first)
        return;

    struct list_node* elem = q->last;
    while(elem != NULL)
    {
        printf("Debug node traversal.\n prev: %p, next: %p, data: %p\n",
                elem->prev, elem->next, elem->data);

        elem = elem->prev;
    }
}
