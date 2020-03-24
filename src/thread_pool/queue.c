#include "queue.h"
#include "../log.h"
#include <stdlib.h>
#include <pthread.h>

/* PRIVATE FUNCTIONS */
void queue_lock(struct Queue *q) {
    pthread_mutex_lock(&q->mutex);
}

void queue_unlock(struct Queue *q) {
    pthread_mutex_unlock(&q->mutex);
}

/* QUEUE */
extern struct Queue *queue_new() {
    struct Queue *q = NULL;

    q = malloc(sizeof(struct Queue));
    if (!q) {
        throw("queue failed to malloc");
    }

    pthread_mutex_init(&q->mutex, NULL);

    q->count = 0;
    q->head = NULL;
    q->tail = NULL;

    return q;
    error:
    return queue_free(q);
}

int queue_push(struct Queue *q, void *elem) {
    queue_lock(q);

    struct QueueNode *node = malloc(sizeof(struct QueueNode));
    if (node == NULL) {
        throw("queue_push failed to malloc")
    }
    node->value = elem;
    node->next = NULL;

    if (q->head == NULL) {
        q->head = node;
        q->tail = node;
        q->count += 1;

        queue_unlock(q);
        return EXIT_SUCCESS;
    }

    q->tail->next = node;
    q->tail = node;
    q->count += 1;

    queue_unlock(q);
    return EXIT_SUCCESS;
    error:
    if (node) { free(node); };
    queue_unlock(q);
    return EXIT_FAILURE;
}

void *queue_pop(struct Queue *q) {
    queue_lock(q);

    if (q->count == 0) {
        queue_unlock(q);
        return NULL;
    }


    void *elem = NULL;
    struct QueueNode *tmp = NULL;

    elem = q->head->value;
    tmp = q->head;

    q->head = q->head->next;
    q->count -= 1;

    free(tmp);

    queue_unlock(q);

    return elem;
}

int queue_get_count(struct Queue *q) {
    queue_lock(q);
    int count = q->count;
    queue_unlock(q);

    return count;
}

extern struct Queue *queue_free(struct Queue *q) {
    pthread_mutex_destroy(&q->mutex);
    if (q->count > 0) {
        log_error("trying to free a queue with items in it. memory is leaking");
    }
    if (q != NULL) {
        free(q);
        q = NULL;
    }

    return q;
}
