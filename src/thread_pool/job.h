#ifndef UVGTORRENT_C_THREAD_POOL_JOB_H
#define UVGTORRENT_C_THREAD_POOL_JOB_H

#include "queue.h"
#include <stdlib.h>
#include <pthread.h>

struct Job {
    int (*execute)(int *cancel_flag, struct Queue *result_queue, ...);

    struct Queue *result_queue;
    int arg_count;
    size_t arg_size;
    void *args[];
};


/**
 * extern struct Job * job_new(int (*execute)(int * cancel_flag, struct Queue * result_queue, ...), struct Queue * result_queue, int arg_count, void* args[])
 *
 * int (*execute)(struct Queue *, ...)  : pointer to function to execute
 * struct Queue * result_queue          : queue to return results
 * int arg_count                        : number of arguments to pass to function. must be less than MAX_JOB_ARGS
 * void* args[]                         : array of void pointers to args
 *
 * NOTES   : any memory allocated from this function becomes the responsibility of the owner of the results_queue
 * RETURN  : struct Job *
 */
extern struct Job *
job_new(int (*execute)(int *cancel_flag, struct Queue *result_queue, ...), struct Queue *result_queue, int arg_count,
        void *args[]);

/**
 * int job_execute(struct Job * j, int * cancel_flag)
 *
 * struct Job * j;
 * int * cancel_flag: pointer to a cancel flag. check to see if you need to gracefully exit
 *
 * NOTES   : executes the job
 * RETURN  : success
 */
int job_execute(struct Job *j, int *cancel_flag);

/**
 * extern struct Job * job_free(struct Job * j)
 *
 * struct ThreadPool * tp;
 *
 * NOTES   : frees a job
 * RETURN  : struct Job *
 */
extern struct Job *job_free(struct Job *j);

#endif
