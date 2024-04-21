#ifndef _INCLUDE_QUEUE_H_
#define _INCLUDE_QUEUE_H_

#include <stdlib.h>

struct queue_node;

struct queue {
    int length;
    size_t item_size;

    struct queue_node *rear;
    struct queue_node *front;
};

// Allocate new queue
struct queue * queue_new(size_t item_size);

// Free given queue and all it's elements (nodes)
void queue_free(struct queue *q);

// Insert given data into queue
void queue_enqueue(struct queue *q, void *data);

// Read data from queue to data, if data is NULL
// queue node will be deleted, returns 0 on success and 1 on failure
int queue_dequeue(struct queue *q, void *data);

// Get a pointer to the queue element at given index
void * queue_peek(struct queue *q, int index);

// Get the number of elements in the queue
int queue_get_length(struct queue *q);

// Checks if given queue is empty
int queue_is_empty(struct queue *q);

#endif