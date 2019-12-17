#ifndef UVGTORRENT_C_THREAD_POOL_H
#define UVGTORRENT_C_THREAD_POOL_H

#include "job.h"
#include "queue.h"
#include <stdlib.h>
#include <pthread.h>

struct ThreadPool {
  int thread_count;
  volatile int working_threads;

  struct Queue * work_queue;
  pthread_t threads[];
};

/**
* extern struct ThreadPool * thread_pool_new(int thread_count)
*
* int thread_count  : maximum number of threads
*
* NOTES   : mallocs a new ThreadPool struct
* RETURN  : struct ThreadPool *
*/
extern struct ThreadPool * thread_pool_new(int thread_count);

/**
* extern struct ThreadPool * thread_pool_free(struct ThreadPool * tp)
*
* struct ThreadPool * tp;
*
* NOTES   : frees a thread pool
* RETURN  : struct ThreadPool *
*/
extern struct ThreadPool * thread_pool_free(struct ThreadPool * tp);

/**
* extern int thread_pool_add_job(struct Job *)
*
* struct Job *;
*
* NOTES   : add a job to the given ThreadPools job queue
* RETURN  : success
*/
extern int thread_pool_add_job(struct Job * j);

#endif // UVGTORRENT_C_THREAD_POOL_H
