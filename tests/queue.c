#include <queue.h>
#include <debug.h>

int main() {
    int i, value;
    struct queue *q;

    debug_set_fp(stdout);

    q = queue_new(sizeof(int));

    value = 1;
    queue_enqueue(q, &value);
    value = 3;
    queue_enqueue(q, &value);
    value = 4;
    queue_enqueue(q, &value);

    while (!queue_is_empty(q)) {
        queue_dequeue(q, &value);
        debug("value[%d] = %d", i, value);
    }

    value = 22;
    queue_enqueue(q, &value);

    value = 112;
    queue_dequeue(q, &value);
    debug("value = %d", value);

    queue_free(q);
}