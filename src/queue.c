#include <queue.h>
#include <sys_memory.h>
#include <stdlib.h>

struct queue_node {
    void *data;
    struct queue_node *next;
};

// Allocate new queue node with given size and copy data to it
struct queue_node * queue_node_new(void *data, size_t size) {
    struct queue_node *node;

    node = safe_malloc(sizeof(struct queue_node), "Failed to allocate queue node");
    node->data = safe_malloc(size, "Failed to allocate memory for queue data");
    node->next = NULL;

    memcpy(node->data, data, size);
    return node;
}

// Allocate new queue
struct queue * queue_new(size_t item_size) {
    struct queue *q;

    q = safe_malloc(sizeof(struct queue), "Failed to allocate the queue");

    q->length = 0;
    q->item_size = item_size;
    q->front = q->rear = NULL;

    return q;
}

// Free given queue and all it's elements (nodes)
void queue_free(struct queue *q) {
    
    if (q->length > 0) {
        while (q->front != NULL) {
            struct queue_node *next = q->front->next;

            free(q->front->data);
            free(q->front);
            q->front = next;
        }
    }

    free(q);
}

// Insert given data into queue
void queue_enqueue(struct queue *q, void *data) {
    if (q->length == 0) {
        q->front = queue_node_new(data, q->item_size);
        q->rear = q->front;
    } else {
        q->rear->next = queue_node_new(data, q->item_size);
        q->rear = q->rear->next;
    }

    ++q->length;
}

// Read data from queue to data, if data is NULL
// queue node will be deleted
int queue_dequeue(struct queue *q, void *data) {
    struct queue_node *next;

    if (q->length == 0)
        return 1;

    if (data != NULL)
        memcpy(data, q->front->data, q->item_size);

    next = q->front->next;
    free(q->front->data);
    free(q->front);
    q->front = next;

    if (q->length == 1)
        q->rear = NULL;

    --q->length;
    return 0;
}

// Get a pointer to the queue element at given index
void * queue_peek(struct queue *q, int index) {
    int i;
    struct queue_node *node = q->front;

    if (index > q->length - 1)
        return NULL;

    for (i = 0; i < index; i++)
        node = node->next;

    return node->data;
}

// Get the number of elements in the queue
int queue_get_length(struct queue *q) {
    return q->length;
}

// Checks if given queue is empty
int queue_is_empty(struct queue *q) {
    return q->length == 0;
}