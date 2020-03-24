#include "thread_pool.h"
#include "../log.h"
#include <stdarg.h>
#include <sys/sysinfo.h>
#include <stdatomic.h>

// example job function
// everything has to be done with pointers - either to heap or stack
int add_numbers(_Atomic int *cancel_flag, ...) {
    int *a = NULL;
    int *b = NULL;

    // we can allocate to the heap, whoever owns the queue is responsible
    // for freeing
    int *result = malloc(sizeof(int));

    va_list args;
    va_start(args, cancel_flag);

    a = (int *) va_arg(args, int *);
    b = (int *) va_arg(args, int *);
    struct Queue * result_queue = va_arg(args, struct Queue *);

    log_info("JOB a %i", *a);
    log_info("JOB b %i", *b);

    int r = *a + *b;
    *result = r;
    log_info("JOB RESULT %i", r);

    queue_push(result_queue, (void *) result);
}

extern void run_threadpool_example() {
    /* queue containing job results */
    int *result = NULL;
    struct Queue *q = queue_new();
    if (!q) {
        throw("queue failed to init");
    }

    struct ThreadPool *tp = thread_pool_new(get_nprocs_conf() - 1);
    if (!tp) {
        throw("thread pool failed to init");
    }

    int a = 10;
    int b = 5;
    struct JobArg args[3] = {
            {
                .arg = &a,
                .mutex = NULL
            },
            {
                .arg = &b,
                .mutex = NULL
            },
            {
                    .arg = &q,
                    .mutex = NULL
            }
    };

    struct Job *j = job_new(
            &add_numbers,
            sizeof(args) / sizeof(struct JobArg),
            args
    );
    if (!j) {
        throw("job failed to init");
    }

    thread_pool_add_job(tp, j);
    while (1) {
        if (queue_get_count(q) > 0) {
            result = (int *) queue_pop(q);
            log_info("GOT RESULT %i", *result);
            free(result);
            break;
        }
    }

    queue_free(q);
    thread_pool_free(tp);

    return;
    error:
    if (result) { free(result); }
    queue_free(q);
    thread_pool_free(tp);
    return;
}
