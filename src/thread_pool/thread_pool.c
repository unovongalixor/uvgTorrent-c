#include "thread_pool.h"
#include "../macros.h"
#include <stdlib.h>
#include <pthread.h>

#define MAX_JOB_ARGS 3

/* JOB */
extern struct Job * job_new(int (*execute)(struct Queue *, ...), struct Queue * result_queue, int arg_count, void* args[]) {
  struct Job *j = NULL;

  if(arg_count > MAX_JOB_ARGS) {
    throw("invalid number of job arguments. cannot excede %i", MAX_JOB_ARGS);
  }
  size_t arg_size = sizeof(void*) * arg_count;

  j = malloc(sizeof(struct Job) + arg_size);
  if (!j) {
    throw("Job failed to malloc");
  }
  j->result_queue = NULL;
  j->arg_count = 0;
  j->arg_size = arg_size;

  j->execute = execute;
  j->result_queue = result_queue;
  j->arg_count = arg_count;

  if (j->arg_count != 0) {
    memcpy(j->args, args, j->arg_size);
  }

  return j;

error:
  return job_free(j);
}

int job_execute(struct Job * j){
  switch(j->arg_count) {
    case 0:
      return j->execute(j->result_queue);
      break;
    case 1:
      return j->execute(j->result_queue, j->args[0]);
      break;
    case 2:
      return j->execute(j->result_queue, j->args[0], j->args[1]);
      break;
    case 3:
      return j->execute(j->result_queue, j->args[0], j->args[1], j->args[2]);
      break;
    default:
      throw("invalid number of job args")
  }
error:
  return EXIT_FAILURE;
}

extern struct Job * job_free(struct Job * j) {
  if(j) { free(j); j = NULL; }

  return j;
}


/* QUEUE */
extern struct Queue * queue_new() {
  struct Queue *q = NULL;

  q = malloc(sizeof(struct Queue));
  if (!q) {
    throw("queue failed to malloc");
  }

  q->queue = NULL;
  q->count = 0;

  pthread_mutex_init(&q->mutex, NULL);
  q->queue = StsQueue.create();

  return q;
error:
  return queue_free(q);
}

void queue_push(struct Queue * q, void *elem) {
  pthread_mutex_lock(&q->mutex);
  StsQueue.push(q->queue, elem);
  q->count++;
  pthread_mutex_unlock(&q->mutex);
}

void * queue_pop(struct Queue * q) {
  pthread_mutex_lock(&q->mutex);
  void *elem = StsQueue.pop(q->queue);
  q->count--;
  pthread_mutex_unlock(&q->mutex);

  return elem;
}

extern struct Queue * queue_free(struct Queue * q) {
  pthread_mutex_destroy(&q->mutex);
  if (q->queue) { StsQueue.destroy(q->queue); q->queue = NULL; }
  if (q) { free(q); q = NULL; }

  return q;
}
