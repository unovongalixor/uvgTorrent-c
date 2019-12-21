#include "thread_pool.h"
#include "../macros.h"
#include <stdlib.h>
#include <pthread.h>

/* THREAD POOL */
void * thread_handle(void * args) {
  struct ThreadPool * tp = (struct ThreadPool *) args;
  struct Queue * job_queue = tp->work_queue;

  while (1) {
    if (queue_get_count(job_queue) > 0) {
      struct Job * j = (struct Job *) queue_pop(job_queue);
      if(j) {
          job_execute(j, &tp->cancel_flag);
          job_free(j);
      }
    }

    if (tp->cancel_flag == 1) {
      break;
    }
  }
}

struct ThreadPool * thread_pool_new(int thread_count) {
  struct ThreadPool * tp = NULL;

  size_t thread_size = sizeof(pthread_t) * thread_count;
  tp = malloc(sizeof(struct ThreadPool) + thread_size);
  if (!tp) {
    throw("ThreadPool failed to malloc");
  }

  tp->thread_count = thread_count;
  tp->working_threads = 0;
  tp->cancel_flag = 0;
  tp->work_queue = NULL;

  tp->work_queue = queue_new();
  if (!tp->work_queue) {
    throw("ThreadPool work_queue failed to initialize");
  }

  int last_state, last_type;
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &last_state);
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &last_type);

  for(int i=0; i<tp->thread_count; i++) {
    if (pthread_create(&tp->threads[i], NULL, &thread_handle, (void *) tp)) {
      throw("failed to create threads");
    }
  }

  return tp;

error:
  return thread_pool_free(tp);
}

struct ThreadPool * thread_pool_free(struct ThreadPool * tp) {
  if(tp) {
    tp->cancel_flag = 1;
    for(int i=0; i<tp->thread_count; i++) {
      pthread_join(tp->threads[i], NULL);
    }
  }
  if(tp->work_queue) {
    while(queue_get_count(tp->work_queue) > 0) {
      struct Job * j = (struct Job *) queue_pop(tp->work_queue);
      if(j) {
        job_free(j);
      }
    }
    queue_free(tp->work_queue);
    tp->work_queue = NULL;
  }
  if(tp) { free(tp); tp = NULL; }

  return tp;
}

int thread_pool_add_job(struct ThreadPool * tp, struct Job * j) {
  queue_push(tp->work_queue, (void *) j);
  return EXIT_SUCCESS;
}
