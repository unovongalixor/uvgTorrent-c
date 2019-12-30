#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/sysinfo.h>

#include "args/args.h"
#include "colors.h"
#include "macros.h"
#include "messages/messages.h"
#include "torrent/torrent.h"
#include "peer/peer.h"
#include "thread_pool/thread_pool.h"

volatile sig_atomic_t running = 1;
struct ThreadPool *tp = NULL;
struct Torrent *t = NULL;

void SIGINT_handle(int signum) {
    log_info("closing uvgTorrent...");
    running = 0;
}

int main(int argc, char *argv[]) {
    struct sigaction a;
    a.sa_handler = SIGINT_handle;
    a.sa_flags = 0;
    sigemptyset(&a.sa_mask);
    sigaction(SIGINT, &a, NULL);

    signal(SIGINT, SIGINT_handle);

    printf(RED "                                                                                                    \n" NO_COLOR);
    printf(RED "  ▄• ▄▌ ▌ ▐· ▄▄ • ▄▄▄▄▄      ▄▄▄  ▄▄▄  ▄▄▄ . ▐ ▄ ▄▄▄▄▄     ▄▄▄·▄▄▄  ▄▄▄ ..▄▄ · ▄▄▄ . ▐ ▄ ▄▄▄▄▄.▄▄ · \n" NO_COLOR);
    printf(RED "  █▪██▌▪█·█▌▐█ ▀ ▪•██  ▪     ▀▄ █·▀▄ █·▀▄.▀·•█▌▐█•██      ▐█ ▄█▀▄ █·▀▄.▀·▐█ ▀. ▀▄.▀·•█▌▐█•██  ▐█ ▀. \n" NO_COLOR);
    printf(RED "  █▌▐█▌▐█▐█•▄█ ▀█▄ ▐█.▪ ▄█▀▄ ▐▀▀▄ ▐▀▀▄ ▐▀▀▪▄▐█▐▐▌ ▐█.▪     ██▀·▐▀▀▄ ▐▀▀▪▄▄▀▀▀█▄▐▀▀▪▄▐█▐▐▌ ▐█.▪▄▀▀▀█▄\n" NO_COLOR);
    printf(RED "  ▐█▄█▌ ███ ▐█▄▪▐█ ▐█▌·▐█▌.▐▌▐█•█▌▐█•█▌▐█▄▄▌██▐█▌ ▐█▌·    ▐█▪·•▐█•█▌▐█▄▄▌▐█▄▪▐█▐█▄▄▌██▐█▌ ▐█▌·▐█▄▪▐█\n" NO_COLOR);
    printf(RED "   ▀▀▀ . ▀  ·▀▀▀▀  ▀▀▀  ▀█▄▀▪.▀  ▀.▀  ▀ ▀▀▀ ▀▀ █▪ ▀▀▀     .▀   .▀  ▀ ▀▀▀  ▀▀▀▀  ▀▀▀ ▀▀ █▪ ▀▀▀  ▀▀▀▀ \n" NO_COLOR);
    printf(RED "                                                                                                    \n" NO_COLOR);
    printf(BLUE "  ██████████████████████████████████████████████████████████████████████████████████████████████████\n" NO_COLOR);
    printf(RED "                                                                                                    \n" NO_COLOR);

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

    /* initialize and parse torrent */
    t = torrent_new(options.magnet_uri, options.path);
    if (!t) {
        throw("torrent failed to initialize");
    }

    /* initialize queue for receiving peers */
    struct Queue * peer_queue = queue_new();

    /* initialize queue for receiving metadata chunks */

    /* initialize queue for receiving file chunks */

    /* initialize thread pool */
    tp = thread_pool_new();
    if (!tp) {
        throw("thread pool failed to init");
    }

    /* start running trackers in separate threads */
    torrent_run_trackers(t, tp, peer_queue);

    while (running) {
        /* STATE MANAGEMENT */
        // collect and initialize peers

        // update metadata with chunks from peers

        // update files with chunks from peers

        // display some kind of progress
    }

    thread_pool_free(tp);

    while(queue_get_count(peer_queue) > 0) {
        struct Peer * p = (struct Peer *) queue_pop(peer_queue);
        peer_free(p);
    }

    queue_free(peer_queue);

    torrent_free(t);

    return EXIT_SUCCESS;

    error:
    thread_pool_free(tp);
    torrent_free(t);
    // return success allows valgrind to test memory freeing during errors
    return EXIT_SUCCESS;
}
