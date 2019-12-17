#ifndef UVGTORRENT_C_THREAD_POOL_QUEUE_H
#define UVGTORRENT_C_THREAD_POOL_QUEUE_H

#include "sts_queue.h"
#include <stdlib.h>
#include <pthread.h>

struct Queue {
  pthread_mutex_t mutex;
  StsHeader * queue;
  volatile int count;
};


/**
* extern struct Queue * queue_new()
*
*
* NOTES   : mallocs a new Queue struct
* RETURN  : struct Queue *
*/
extern struct Queue * queue_new();

/**
* void queue_push(void *elem)
*
* void *elem;
*
* NOTES   : push elem into the queue
* RETURN  :
*/
void queue_push(struct Queue * q, void *elem);

/**
* void * queue_pop()
*
* NOTES   : pop elem from queue
* RETURN  : void *
*/
void * queue_pop(struct Queue * q);

/**
* int queue_get_count()
*
* NOTES   : get current number of elements in queue. threadsafe
* RETURN  : int
*/
int queue_get_count(struct Queue * q);

/**
* void * queue_lock()
*
* NOTES   : lock this queue. useful for reading count.
* RETURN  :
*/
void queue_lock(struct Queue *q);

/**
* void * queue_unlock()
*
* NOTES   : unlock this queue. useful for reading count.
* RETURN  :
*/
void queue_unlock(struct Queue *q);

/**
* extern struct Queue * queue_free(struct Queue *)
*
* struct Queue *;
*
* NOTES   : clean up the torrent and all associated tracker objects
* RETURN  : freed and NULL'd struct Torrent *
*/
extern struct Queue * queue_free(struct Queue * q);

#endif // UVGTORRENT_C_THREAD_POOL_QUEUE_H
