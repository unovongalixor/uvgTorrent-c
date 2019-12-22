#ifndef UVGTORRENT_C_THREAD_POOL_QUEUE_H
#define UVGTORRENT_C_THREAD_POOL_QUEUE_H

#include <stdlib.h>
#include <pthread.h>

struct QueueNode {
    void *value;
    struct QueueNode *next;
};

struct Queue {
    pthread_mutex_t mutex;
    volatile int count;
    struct QueueNode *head;
    struct QueueNode *tail;
};


/**
* extern struct Queue * queue_new()
*
*
* NOTES   : mallocs a new Queue struct
* RETURN  : struct Queue *
*/
extern struct Queue *queue_new();

/**
* void queue_push(struct Queue * q, void *elem)
*
* struct Queue * q;
* void *elem;
*
* NOTES   : threadsafe push elem into the queue
* RETURN  : success
*/
extern int queue_push(struct Queue *q, void *elem);

/**
* void * queue_pop(struct Queue * q)
*
* struct Queue * q;
*
* NOTES   : threadsafe pop elem from queue
* RETURN  : void *
*/
extern void *queue_pop(struct Queue *q);

/**
* int queue_get_count(struct Queue * q))
*
* struct Queue * q;
*
* NOTES   : threadsafe get current number of elements in queue
* RETURN  : int
*/
extern int queue_get_count(struct Queue *q);

/**
* extern struct Queue * queue_free(struct Queue *)
*
* struct Queue *;
*
* NOTES   : clean up the torrent and all associated tracker objects
* RETURN  : freed and NULL'd struct Torrent *
*/
extern struct Queue *queue_free(struct Queue *q);

#endif // UVGTORRENT_C_THREAD_POOL_QUEUE_H
