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
 * extern struct ThreadPool * thread_pool_new()
 *
 * NOTES   : mallocs a new ThreadPool struct
 * RETURN  : struct ThreadPool *
 */

/**
 * @brief initialize a new thread pool
 * @return struct ThreadPool * or NULL on failure
 */
extern struct ThreadPool *thread_pool_new();

/**
 * @brief free the given thread pool
 * @param tp
 * @warning before the threadpool can free all the threads need to exit.
 *          the threadpool signals the threads to exit by setting cancel_flag to 1
 *          it's critical your job execute functions check cancel_flag to see when they should exit
 * @return the given struct ThreadPool *. NULL on success
 */
extern struct ThreadPool *thread_pool_free(struct ThreadPool *tp);

/**
 * @brief add a job to the work queue
 * @param tp
 * @param j
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
extern int thread_pool_add_job(struct ThreadPool *tp, struct Job *j);

#endif // UVGTORRENT_C_THREAD_POOL_H
