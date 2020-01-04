#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <fcntl.h>
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

/**
 * @brief handle sigint
 * @param signum
 */
void SIGINT_handle(int signum) {
    log_info("closing uvgTorrent...");
    running = 0;
}

/**
 * @brief check to see if there's any user input available for me to real
 * @note used to prevent getchar() from blocking the main thread
 * @return 1 or 0
 */
int stdin_available() {
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
    return (FD_ISSET(0, &fds));
}


int main(int argc, char *argv[]) {
    /* set up sigint handler */
    struct sigaction a;
    a.sa_handler = SIGINT_handle;
    a.sa_flags = 0;
    sigemptyset(&a.sa_mask);
    sigaction(SIGINT, &a, NULL);
    signal(SIGINT, SIGINT_handle);

    /* logo */
    printf(RED "                                                                                                    \n" NO_COLOR);
    printf(RED "  ▄• ▄▌ ▌ ▐· ▄▄ • ▄▄▄▄▄      ▄▄▄  ▄▄▄  ▄▄▄ . ▐ ▄ ▄▄▄▄▄     ▄▄▄·▄▄▄  ▄▄▄ ..▄▄ · ▄▄▄ . ▐ ▄ ▄▄▄▄▄.▄▄ · \n" NO_COLOR);
    printf(RED "  █▪██▌▪█·█▌▐█ ▀ ▪•██  ▪     ▀▄ █·▀▄ █·▀▄.▀·•█▌▐█•██      ▐█ ▄█▀▄ █·▀▄.▀·▐█ ▀. ▀▄.▀·•█▌▐█•██  ▐█ ▀. \n" NO_COLOR);
    printf(RED "  █▌▐█▌▐█▐█•▄█ ▀█▄ ▐█.▪ ▄█▀▄ ▐▀▀▄ ▐▀▀▄ ▐▀▀▪▄▐█▐▐▌ ▐█.▪     ██▀·▐▀▀▄ ▐▀▀▪▄▄▀▀▀█▄▐▀▀▪▄▐█▐▐▌ ▐█.▪▄▀▀▀█▄\n" NO_COLOR);
    printf(RED "  ▐█▄█▌ ███ ▐█▄▪▐█ ▐█▌·▐█▌.▐▌▐█•█▌▐█•█▌▐█▄▄▌██▐█▌ ▐█▌·    ▐█▪·•▐█•█▌▐█▄▄▌▐█▄▪▐█▐█▄▄▌██▐█▌ ▐█▌·▐█▄▪▐█\n" NO_COLOR);
    printf(RED "   ▀▀▀ . ▀  ·▀▀▀▀  ▀▀▀  ▀█▄▀▪.▀  ▀.▀  ▀ ▀▀▀ ▀▀ █▪ ▀▀▀     .▀   .▀  ▀ ▀▀▀  ▀▀▀▀  ▀▀▀ ▀▀ █▪ ▀▀▀  ▀▀▀▀ \n" NO_COLOR);
    printf(RED "                                                                                                    \n" NO_COLOR);
    printf(BLUE "  ██████████████████████████████████  press q + enter to quit  █████████████████████████████████████\n" NO_COLOR);
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
    tp = thread_pool_new(1000);
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

        // check to see if the user has typed "q + enter" to quit
        if (stdin_available()) {
            char c = getchar();
            if (c == 'q') {
                running = 0;
            }
        }
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

    while(queue_get_count(peer_queue) > 0) {
        struct Peer * p = (struct Peer *) queue_pop(peer_queue);
        peer_free(p);
    }

    queue_free(peer_queue);

    torrent_free(t);
    return EXIT_FAILURE;
}
