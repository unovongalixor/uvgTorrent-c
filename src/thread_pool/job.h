#ifndef UVGTORRENT_C_THREAD_POOL_JOB_H
#define UVGTORRENT_C_THREAD_POOL_JOB_H

#include "queue.h"
#include <stdlib.h>
#include <pthread.h>

/**
 * @brief JobArg provides a mechanism to pass arguments to a job coupled with a mutex.
 * @note mutex can be NULL if you don't need any lock protection
 * @note use job_arg_lock & job_arg_unlock to handle locking and unlocking
 */
struct JobArg {

    void * arg;   /* pointer to the argument to be read by your worker function */
    void * mutex; /* mutex to protect the arg pointer. pass NULL if there's no mutex needed */
};

/**
 * @brief Lock JobArg.mutex is mutex is not NULL
 * @param ja JobArg to lock
 * @note passes without doing anything if mutex is NULL
 */
extern void job_arg_lock(struct JobArg ja);

/**
 * @brief Unlock JobArg.mutex is mutex is not NULL
 * @param ja JobArg to unlock
 * @note passes without doing anything if mutex is NULL
 */
extern void job_arg_unlock(struct JobArg ja);

/**
 * @brief Job struct encapsulating a job to be ran by one of our threads
 */
struct Job {
    int (*execute)(int *cancel_flag, struct Queue *result_queue, ...);

    struct Queue *result_queue;
    int arg_count;
    size_t arg_size;
    struct JobArg args[];
};

/**
 * @brief initialize a new job
 * @param execute a pointer to our execution function.
 *        must accept cancel flag to handle SIGINT and take other arugments as variadic arguments
 * @param result_queue
 * @param arg_count number of arguments passed in args
 * @param args variable length array of arguments to pass to job execute function
 * @return newly initialized Job or NULL if function failed
 * @warning once you add the job to the thread pool the thread pool takes responsibility for freeing
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

/**
 * @brief execute the provided job
 * @param j job to execute
 * @param cancel_flag pointer to an int which can be set to 1 to interrupt the job.
 * @return EXIT_SUCCESS or EXIT_FAILURE
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

/**
 * @brief frees a given job
 * @param j
 * @return j after freeing. NULL on success
 */
extern struct Job *job_free(struct Job *j);

#endif
