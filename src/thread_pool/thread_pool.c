#include "thread_pool.h"
#include "../macros.h"
#include <stdlib.h>
#include <pthread.h>

/* THREAD POOL */
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
