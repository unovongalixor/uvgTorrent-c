#ifndef UVGTORRENT_C_THREAD_POOL_H
#define UVGTORRENT_C_THREAD_POOL_H

#include "job.h"
#include "queue.h"
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>


struct ThreadPool {
    volatile int cancel_flag;
    volatile int working_threads;
    int thread_count;

    sem_t job_semaphore;
    struct Queue *work_queue;

    struct ThreadHandleArgs *thread_handle_args;
    pthread_t threads[];
};

/**
 * extern struct ThreadPool * thread_pool_new(int thread_count)
 *
 * NOTES   : mallocs a new ThreadPool struct
 * RETURN  : struct ThreadPool *
 */
extern struct ThreadPool *thread_pool_new();

/**
 * extern struct ThreadPool * thread_pool_cancel(struct ThreadPool * tp)
 *
 * struct ThreadPool * tp;
 *
 * NOTES   : cancels a thread pool. still need to call free after
 * RETURN  : struct ThreadPool *
 */
extern void thread_pool_cancel(struct ThreadPool *tp);

/**
 * extern struct ThreadPool * thread_pool_free(struct ThreadPool * tp)
 *
 * struct ThreadPool * tp;
 *
 * NOTES   : frees a thread pool
 * RETURN  : struct ThreadPool *
 */
extern struct ThreadPool *thread_pool_free(struct ThreadPool *tp);

/**
 * extern int thread_pool_add_job(struct Job *)
 *
 * struct Job *;
 *
 * NOTES   : add a job to the given ThreadPools job queue
 * RETURN  : success
 */
extern int thread_pool_add_job(struct ThreadPool *tp, struct Job *j);

#endif // UVGTORRENT_C_THREAD_POOL_H
