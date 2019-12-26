#ifndef UVGTORRENT_C_THREAD_POOL_JOB_H
#define UVGTORRENT_C_THREAD_POOL_JOB_H

#include "queue.h"
#include <stdlib.h>
#include <pthread.h>

struct JobArg {
    void * arg; // pointer to the argument to be read by your worker function
    void * mutex; // mutex to protect the arg pointer in case it is being shared
                  // you don't need to manage this mutex in the worker function, it'll be locked before your
                  // worker function is called and unlocked after
};

struct Job {
    int (*execute)(int *cancel_flag, struct Queue *result_queue, ...);

    struct Queue *result_queue;
    int arg_count;
    size_t arg_size;
    struct JobArg args[];
};


/**
 * extern struct Job * job_new(int (*execute)(int * cancel_flag, struct Queue * result_queue, ...), struct Queue * result_queue, int arg_count, struct JobArg args[])
 *
 * int (*execute)(struct Queue *, ...)  : pointer to function to execute
 * struct Queue * result_queue          : queue to return results
 * int arg_count                        : number of arguments to pass to function. must be less than MAX_JOB_ARGS
 * struct JobArg args[]                 : array of JobArgs. these contain void pointers to arguments for the jobs
 *                                      : and an optional pointer to a mutex incase the argument is shared between
 *                                      : different threads. set mutex = NULL if you don't need any mutex protection
 *
 * NOTES   : any memory allocated from this function becomes the responsibility of the owner of the results_queue
 * RETURN  : struct Job *
 */
extern struct Job *
job_new(int (*execute)(int *cancel_flag, struct Queue *result_queue, ...), struct Queue *result_queue, int arg_count,
        struct JobArg args[]);

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
