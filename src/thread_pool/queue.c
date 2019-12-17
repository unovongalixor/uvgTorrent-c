#include "queue.h"
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

  q->queue = NULL;
  q->count = 0;

  pthread_mutex_init(&(q->mutex), NULL);
  q->queue = StsQueue.create();

  return q;
error:
  return queue_free(q);
}

void queue_push(struct Queue * q, void *elem) {
  queue_lock(q);
  StsQueue.push(q->queue, elem);
  q->count++;
  queue_unlock(q);
}

void * queue_pop(struct Queue * q) {
  queue_lock(q);
  void *elem = StsQueue.pop(q->queue);
  q->count--;
  queue_unlock(q);

  return elem;
}

int queue_get_count(struct Queue * q) {
  queue_lock(q);
  int count = q->count;
  queue_unlock(q);

  return count;
}

void queue_lock(struct Queue *q){
    pthread_mutex_lock(&(q->mutex));
}

void queue_unlock(struct Queue *q){
    pthread_mutex_unlock(&(q->mutex));
}

extern struct Queue * queue_free(struct Queue * q) {
  pthread_mutex_destroy(&(q->mutex));
  if (q->queue) { StsQueue.destroy(q->queue); q->queue = NULL; }
  if (q) { free(q); q = NULL; }

  return q;
}
