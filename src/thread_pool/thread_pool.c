#include "thread_pool.h"
#include "../macros.h"
#include <stdlib.h>
#include <pthread.h>



/* QUEUE */
extern struct Queue * queue_new() {
  struct Queue *q = NULL;

  q = malloc(sizeof(struct Queue));
  if (!q) {
    throw("queue failed to malloc");
  }

  q->mutex = NULL;
  q->queue = NULL;
  q->count = 0;

  pthread_mutex_init(q->mutex, NULL);
  q->queue = StsQueue.create();

  return q;
error:
  return queue_free(q);
}

void queue_push(struct Queue * q, void *elem) {
  pthread_mutex_lock(q->mutex);
  StsQueue.push(q->queue, elem);
  pthread_mutex_unlock(q->mutex);
}

void * queue_pop(struct Queue * q) {
  pthread_mutex_lock(q->mutex);
  void *elem = StsQueue.pop(q->queue);
  pthread_mutex_unlock(q->mutex);

  return elem;
}

extern struct Queue * queue_free(struct Queue * q) {
  if (q->mutex != 0) {pthread_mutex_destroy(q->mutex); q->mutex=NULL; }
  if (q->queue) { StsQueue.destroy(q->queue); q->queue = NULL; }
  if (q) { free(q); q = NULL; }

  return q;
}
