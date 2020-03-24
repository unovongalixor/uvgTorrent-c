#include "job.h"
#include "../log.h"
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>

#define MAX_JOB_ARGS 7

void job_arg_lock(struct JobArg ja) {
    if (ja.mutex != NULL) {
        pthread_mutex_lock(ja.mutex);
    }
}

void job_arg_unlock(struct JobArg ja) {
    if (ja.mutex != NULL) {
        pthread_mutex_unlock(ja.mutex);
    }
}

/* JOB */
extern struct Job *
job_new(int (*execute)(_Atomic int *cancel_flag, ...), int arg_count,
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
    j->arg_count = 0;
    j->arg_size = arg_size;

    j->execute = execute;
    j->arg_count = arg_count;

    if (j->arg_count != 0) {
        memcpy(j->args, args, j->arg_size);
    }

    return j;

    error:
    return job_free(j);
}

int job_execute(struct Job *j, _Atomic int *cancel_flag) {
    switch (j->arg_count) {
        case 0:
            return j->execute(cancel_flag);
        case 1:
            return j->execute(cancel_flag, j->args[0]);
        case 2:
            return j->execute(cancel_flag, j->args[0], j->args[1]);
        case 3:
            return j->execute(cancel_flag, j->args[0], j->args[1], j->args[2]);
        case 4:
            return j->execute(cancel_flag, j->args[0], j->args[1], j->args[2], j->args[3]);
        case 5:
            return j->execute(cancel_flag, j->args[0], j->args[1], j->args[2], j->args[3], j->args[4]);
        case 6:
            return j->execute(cancel_flag, j->args[0], j->args[1], j->args[2], j->args[3], j->args[4], j->args[5]);
        case 7:
            return j->execute(cancel_flag, j->args[0], j->args[1], j->args[2], j->args[3], j->args[4], j->args[5], j->args[6]);
        default: throw("invalid number of job args")
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
