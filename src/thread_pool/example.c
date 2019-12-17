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

  job_execute(j);

  queue_lock(q);
  int work_available = q->count > 0;
  queue_unlock(q);

  if (work_available) {
    result = (int *) queue_pop(q);
    log_info("GOT RESULT %i", *result);

  }
  if (result) { free(result); }
  job_free(j);
  queue_free(q);

  return;
error:
  if (result) { free(result); }
  job_free(j);
  queue_free(q);
  return;
}
