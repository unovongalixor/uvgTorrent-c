#ifndef UVGTORRENT_C_TORRENT_H
#define UVGTORRENT_C_TORRENT_H

#include "sts_queue.h"

struct Queue {
  pthread_mutex_t * mutex;
  StsHeader * queue;
};

struct Job {
  void (*func)(struct Queue *, ...);
  struct Queue * result_queue;
  void * args[];
};

struct ThreadPool {
  int thread_count;

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
* extern struct ThreadPool * thread_pool_new(int thread_count)
*
* int thread_count  : maximum number of threads
*
* NOTES   : mallocs a new ThreadPool struct
* RETURN  : struct ThreadPool *
*/
extern struct ThreadPool * thread_pool_new(int thread_count);

/**
* extern struct Queue * queue_new()
*
*
* NOTES   : mallocs a new Queue struct
* RETURN  : struct Queue *
*/
extern struct Queue * queue_new();

#endif // UVGTORRENT_C_TORRENT_H
