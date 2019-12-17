#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "args/args.h"
#include "colors.h"
#include "macros.h"
#include "messages/messages.h"
#include "torrent/torrent.h"
#include "thread_pool/thread_pool.h"

// example job function
int add_numbers(struct Queue * result_queue, ...) {
  int * a = 0;
  int * b = 0;
  int * result = NULL;

  va_list args;
  va_start(args, result_queue);

  result = (int *)va_arg(args, void *);
  a = (int *)va_arg(args, void *);
  b = (int *)va_arg(args, void *);
  log_info("JOB a %i", *a);
  log_info("JOB b %i", *b);
  log_info("r %i", *result);

  int r = *a + *b;
  log_info("JOB RESULT %i", r);
  memcpy(result, &r, sizeof(int));

  queue_push(result_queue, (void *) result);
}

int main (int argc, char* argv[])
{
    printf(RED "                                                                                                    \n" NO_COLOR);
    printf(RED "  ▄• ▄▌ ▌ ▐· ▄▄ • ▄▄▄▄▄      ▄▄▄  ▄▄▄  ▄▄▄ . ▐ ▄ ▄▄▄▄▄     ▄▄▄·▄▄▄  ▄▄▄ ..▄▄ · ▄▄▄ . ▐ ▄ ▄▄▄▄▄.▄▄ · \n" NO_COLOR);
    printf(RED "  █▪██▌▪█·█▌▐█ ▀ ▪•██  ▪     ▀▄ █·▀▄ █·▀▄.▀·•█▌▐█•██      ▐█ ▄█▀▄ █·▀▄.▀·▐█ ▀. ▀▄.▀·•█▌▐█•██  ▐█ ▀. \n" NO_COLOR);
    printf(RED "  █▌▐█▌▐█▐█•▄█ ▀█▄ ▐█.▪ ▄█▀▄ ▐▀▀▄ ▐▀▀▄ ▐▀▀▪▄▐█▐▐▌ ▐█.▪     ██▀·▐▀▀▄ ▐▀▀▪▄▄▀▀▀█▄▐▀▀▪▄▐█▐▐▌ ▐█.▪▄▀▀▀█▄\n" NO_COLOR);
    printf(RED "  ▐█▄█▌ ███ ▐█▄▪▐█ ▐█▌·▐█▌.▐▌▐█•█▌▐█•█▌▐█▄▄▌██▐█▌ ▐█▌·    ▐█▪·•▐█•█▌▐█▄▄▌▐█▄▪▐█▐█▄▄▌██▐█▌ ▐█▌·▐█▄▪▐█\n" NO_COLOR);
    printf(RED "   ▀▀▀ . ▀  ·▀▀▀▀  ▀▀▀  ▀█▄▀▪.▀  ▀.▀  ▀ ▀▀▀ ▀▀ █▪ ▀▀▀     .▀   .▀  ▀ ▀▀▀  ▀▀▀▀  ▀▀▀ ▀▀ █▪ ▀▀▀  ▀▀▀▀ \n" NO_COLOR);
    printf(RED "                                                                                                    \n" NO_COLOR);
    printf(BLUE "  ██████████████████████████████████████████████████████████████████████████████████████████████████\n" NO_COLOR);
    printf(RED "                                                                                                    \n" NO_COLOR);

    struct Torrent *t = NULL;

    /* Read command line options */
    options_t options;
    options_parser(argc, argv, &options);

    /* validate that needed inputs are available */
    if (options.magnet_uri[0] == '\0') {
        help();
        exit(EXIT_SUCCESS);
    }
    if (options.path[0] == '\0') {
        help();
        exit(EXIT_SUCCESS);
    }

    /* test job execution */
    int * result = NULL;
    result = malloc(sizeof(int));
    if(!result) {
      throw("result failed to malloc");
    }
    *result = 10;

    struct Queue * q = queue_new();
    if (!q) {
      throw("queue failed to init");
    }

    int a = 10;
    int b = 5;

    void * args[3];
    args[0] = (void *)result;
    args[1] = (void *)&a;
    args[2] = (void *)&b;

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

    free(result);
    result = NULL;

    queue_free(q);
    job_free(j);

    /* initialize and parse torrent */
    t = torrent_new(options.magnet_uri, options.path);
    if (!t) {
        throw("torrent failed to initialize");
    }

    /* connect trackers */
    torrent_connect_trackers(t);

    /* wait for connect to finish */

    /* announce connected trackers */

    /* scrape connected trackers (parallel to announce) */

    /* wait for incoming peers */


    torrent_free(t);

    return EXIT_SUCCESS;

error:
    queue_free(q);
    torrent_free(t);
    if (!result) {
      free(result);
      result = NULL;
    }
    // return success allows valgrind to test memory freeing during errors
    return EXIT_SUCCESS;
}
