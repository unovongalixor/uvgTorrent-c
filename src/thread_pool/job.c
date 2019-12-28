#include "job.h"
#include "../macros.h"
#include <stdlib.h>
#include <pthread.h>

#define MAX_JOB_ARGS 5

/* JOB */
extern struct Job *
job_new(int (*execute)(int *cancel_flag, struct Queue *result_queue, ...), struct Queue *result_queue, int arg_count,
        struct JobArg args[]) {
    struct Job *j = NULL;

    if (arg_count > MAX_JOB_ARGS) {
        throw("invalid number of job arguments. cannot excede %i", MAX_JOB_ARGS);
    }
    size_t arg_size = sizeof(struct JobArg) * arg_count;

    j = malloc(sizeof(struct Job) + arg_size);
    if (!j) {
        throw("Job failed to malloc");
    }
    j->result_queue = NULL;
    j->arg_count = 0;
    j->arg_size = arg_size;

    j->execute = execute;
    j->result_queue = result_queue;
    j->arg_count = arg_count;

    if (j->arg_count != 0) {
        memcpy(j->args, args, j->arg_size);
    }

    return j;

    error:
    return job_free(j);
}

int job_execute(struct Job *j, int *cancel_flag) {
    for(int i = j->arg_count; i<j->arg_count; i++) {
        if (j->args[i].mutex != NULL) {
            pthread_mutex_lock(j->args[i].mutex);
        }
    }
    switch (j->arg_count) {
        case 0:
            return j->execute(cancel_flag, j->result_queue);
            break;
        case 1:
            return j->execute(cancel_flag, j->result_queue, j->args[0]);
            break;
        case 2:
            return j->execute(cancel_flag, j->result_queue, j->args[0], j->args[1]);
            break;
        case 3:
            return j->execute(cancel_flag, j->result_queue, j->args[0], j->args[1], j->args[2]);
            break;
        case 4:
            return j->execute(cancel_flag, j->result_queue, j->args[0], j->args[1], j->args[2], j->args[3]);
            break;
        case 5:
            return j->execute(cancel_flag, j->result_queue, j->args[0], j->args[1], j->args[2], j->args[3], j->args[4]);
            break;
        default: throw("invalid number of job args")
    }
    for(int i = j->arg_count; i<j->arg_count; i++) {
        if (j->args[i].mutex != NULL) {
            pthread_mutex_unlock(j->args[i].mutex);
        }
    }
    error:
    return EXIT_FAILURE;
}

extern struct Job *job_free(struct Job *j) {
    if (j) {
        free(j);
        j = NULL;
    }

    return j;
}
