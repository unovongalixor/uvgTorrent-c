#include "thread_pool.h"
#include "../macros.h"
#include <stdarg.h>

// example job function
// everything has to be done with pointers - either to heap or stack
int add_numbers(struct Queue * result_queue, ...) {
  int * a = NULL;
  int * b = NULL;

  // we can allocate to the heap, whoever owns the queue is responsible
  // for freeing
  int * result = malloc(sizeof(int));

  va_list args;
  va_start(args, result_queue);

  a = (int *)va_arg(args, void *);
  b = (int *)va_arg(args, void *);

  log_info("JOB a %i", *a);
  log_info("JOB b %i", *b);

  int r = *a + *b;
  *result = r;
  log_info("JOB RESULT %i", r);

  queue_push(result_queue, (void *) result);
}

extern void run_threadpool_example() {
  /* queue containing job results */
  int * result = NULL;
  struct Queue * q = queue_new();
  if (!q) {
    throw("queue failed to init");
  }

  struct ThreadPool * tp = thread_pool_new(20);
  if (!tp) {
    throw("thread pool failed to init");
  }

  int a = 10;
  int b = 5;
  void * args[2] = {
    (void *)&a,
    (void *)&b
  };

  struct Job * j = job_new(
    &add_numbers,
    q,
    sizeof(args) / sizeof(void *),
    args
  );
  if (!j) {
    throw("job failed to init");
  }

  thread_pool_add_job(tp, j);
  while(1) {
    if (queue_get_count(q) > 0) {
      result = (int *) queue_pop(q);
      log_info("GOT RESULT %i", *result);
      free(result);
      break;
    }
  }

  job_free(j);
  queue_free(q);

  return;
error:
  if (result) { free(result); }
  job_free(j);
  queue_free(q);
  return;
}