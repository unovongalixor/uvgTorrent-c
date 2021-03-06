#include "thread_pool.h"
#include "../log.h"
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdatomic.h>

/* THREAD POOL */
void *thread_handle(void *args) {
    struct ThreadPool *tp = (struct ThreadPool *) args;
    struct Queue *job_queue = tp->job_queue;

    while (tp->cancel_flag != 1) {
        sem_wait(&tp->job_semaphore);
        if (queue_get_count(job_queue) > 0) {
            struct Job *j = (struct Job *) queue_pop(job_queue);
            if (j) {
                job_execute(j, (_Atomic int *) &tp->cancel_flag);
                job_free(j);
            }
        }
    }
}

struct ThreadPool *thread_pool_new(int max_threads) {
    struct ThreadPool *tp = NULL;

    size_t thread_size = sizeof(pthread_t) * max_threads;
    tp = malloc(sizeof(struct ThreadPool) + thread_size);
    if (!tp) {
        throw("ThreadPool failed to malloc");
    }

    tp->thread_count = 0;
    tp->max_threads = max_threads;
    tp->working_threads = 0;
    tp->cancel_flag = 0;
    tp->job_queue = NULL;

    // initialize a claimed semaphore to prevent worker threads from hogging cpu.
    // when work becomes available, we will release the semaphore
    sem_init(&tp->job_semaphore, 0, 0);
    tp->job_queue = queue_new();
    if (!tp->job_queue) {
        throw("ThreadPool job_queue failed to initialize");
    }

    return tp;

    error:
    return thread_pool_free(tp);
}

struct ThreadPool *thread_pool_free(struct ThreadPool *tp) {
    if (tp) {
        tp->cancel_flag = 1;
        for (int i = 0; i < tp->thread_count; i++) {
            // increment the semaphore enough times to allow each thread to exit cleanly
            // due to the cancel flag we just set
            sem_post(&tp->job_semaphore);
        }
        for (int i = 0; i < tp->thread_count; i++) {
            pthread_join(tp->threads[i], NULL);
        }
        if (tp->job_queue) {
            while (queue_get_count(tp->job_queue) > 0) {
                struct Job *j = (struct Job *) queue_pop(tp->job_queue);
                if (j) {
                    job_free(j);
                }
            }
            queue_free(tp->job_queue);
            tp->job_queue = NULL;
        }
        free(tp);
        tp = NULL;
    }

    return tp;
}

int thread_pool_add_job(struct ThreadPool *tp, struct Job *j) {
    // lazy load threads as work is added
    if (tp->thread_count < tp->max_threads) {
        if (pthread_create(&tp->threads[tp->thread_count], NULL, &thread_handle, (void *) tp)) {
            throw("failed to create threads");
        }
        tp->thread_count += 1;
    }

    int result = queue_push(tp->job_queue, (void *) j);
    sem_post(&tp->job_semaphore); // release the semaphore, work is available.

    return result;
    error:
    return EXIT_FAILURE;
}
