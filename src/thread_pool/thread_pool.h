#ifndef UVGTORRENT_C_THREAD_POOL_H
#define UVGTORRENT_C_THREAD_POOL_H

#include "sts_queue.h"
#include <stdlib.h>
#include <pthread.h>

struct Queue {
  pthread_mutex_t mutex;
  StsHeader * queue;
  volatile int count;
};

struct Job {
  int (*execute)(struct Queue *, ...);
  struct Queue * result_queue;
  int arg_count;
  size_t arg_size;
  void * args[];
};

struct ThreadPool {
  int thread_count;
  volatile int working_threads;

  struct Queue * work_queue;
  pthread_t threads[];
};

/**
* extern struct Job * job_new(int (*execute)(struct Queue *, ...), struct Queue * result_queue, int arg_count, void* args[])
*
* int (*execute)(struct Queue *, ...)  : pointer to function to execute
* struct Queue * result_queue          : queue to return results
* int arg_count                        : number of arguments to pass to function. must be less than MAX_JOB_ARGS
* void* args[]                         : array of void pointers to args
*
* NOTES   : any memory allocated from this function becomes the responsibility of the owner of the results_queue
* RETURN  : struct Job *
*/
extern struct Job * job_new(int (*execute)(struct Queue *, ...), struct Queue * result_queue, int arg_count, void* args[]);

/**
* int job_execute(struct Job * j)
*
* struct Job * j;
*
* NOTES   : executes the job
* RETURN  : success
*/
int job_execute(struct Job * j);

/**
* extern struct Job * job_free(struct Job * j)
*
* struct ThreadPool * tp;
*
* NOTES   : frees a job
* RETURN  : struct Job *
*/
extern struct Job * job_free(struct Job * j);

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


#endif // UVGTORRENT_C_THREAD_POOL_H
