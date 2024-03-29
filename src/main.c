/**
 * @file main.c
 * @author Simon Bursten <smnbursten@gmail.com>
 *
 * @brief welcome to UVGTorrent! UVGTorrent downloads torrents in sequential order
 *        to enable streaming of contents during download.
 *
 * @note the structure is as follows. see these files for futher details:
 *
 *       torrent/torrent.h:  declares the current state of the torrent via mutex protected shared memory
 *                           triggers workers as needed to run tracker and peer related coroutines via multithreading pools.
 *                           processes completed chunks of metadata returned via queue and parses the metadata when it's completed
 *                           processes torrent data chunks returned via queue, validating them and writing them to the output directory
 *
 *       torrent/torrent_data.h: provides a shared interface to encapsulate torrent data for access by peers and the main thread
 *                               peers can claim chunks of a given piece of data, with the claims expiring after a deadline
 *                               peers can read completed chunks for sharing with other peers
 *                               the main thread can write chunks into the data
 *                               the main thread can read completed pieces for validation
 *                               the main thread can get information needed for writing to file, such as relative position of the piece in the torrent and length
 *
 *       tracker/tracker.h:  returns available peers to the main thread via queue
 *
 *       peer/peer.h: establishes and manages the state of a connection with a given peer
 *                    will use torrent_data.h to determine if there is torrent metadata or torrent data that needs requesting
 *                    upon receiving data peer will return this data to the main thread via queue
 *
 * @see torrent/torrent.h
 * @see torrent/torrent_data.h
 * @see tracker/tracker.h
 * @see peer/peer.h
 * @see https://www.libtorrent.org/udp_tracker_protocol.html
 * @see https://wiki.theory.org/index.php/BitTorrentSpecification#Peer_wire_protocol_.28TCP.29
 * @see https://www.bittorrent.org/beps/bep_0005.html
 * @see http://xbtt.sourceforge.net/udp_tracker_protocol.html
 * @see http://bittorrent.org/bittorrentecon.pdf
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <fcntl.h>
#include "args/args.h"
#include "colors.h"
#include "log.h"
#include "messages/messages.h"
#include "torrent/torrent.h"
#include "peer/peer.h"
#include "thread_pool/thread_pool.h"
#include "hash_map/hash_map.h"
#include "ipify/ipify.h"

volatile sig_atomic_t running = 1;
struct ThreadPool *tp = NULL;
struct Torrent *t = NULL;
int has_closed = 0;
/**
 * @brief handle sigint
 * @param signum
 */
void SIGINT_handle(int signum) {
    if (has_closed == 0) {
        log_info("closing uvgTorrent...");
        has_closed = 1;
    }
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

/**
 * @brief start a thread listening to peers connecting to us
 * @param t
 * @param tp
 * @param peer_queue
 * @return
 */
int listen_for_peers(struct Torrent * t, struct ThreadPool * tp, struct Queue * peer_queue) {
    struct JobArg args[2] = {
            {
                    .arg = (void *) t,
                    .mutex = NULL
            },
            {
                    .arg = (void *) peer_queue,
                    .mutex = NULL
            }
    };
    struct Job * j = job_new(
            &torrent_listen_for_peers,
            sizeof(args) / sizeof(struct JobArg),
            args
    );
    if (!j) {
        throw("job failed to init");
    }
    return thread_pool_add_job(tp, j);
    error:
    return EXIT_FAILURE;
}

int main(int argc, char *argv[]) {
    /* set up sigint handler */
    struct sigaction a;
    a.sa_handler = SIGINT_handle;
    a.sa_flags = 0;
    sigemptyset(&a.sa_mask);
    sigaction(SIGINT, &a, NULL);
    signal(SIGINT, SIGINT_handle);
    signal(SIGSEGV, SIGINT_handle);
    signal(SIGQUIT, SIGINT_handle);
    signal(SIGHUP, SIGINT_handle);
    signal(SIGTERM, SIGINT_handle);
    signal(SIGPIPE, SIG_IGN);

    log_set_level(LOG_INFO);
    
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

    ipify_connect();
    char * ipptr = ipify_getIP();
    ipify_disconnect();

    /* initialize and parse torrent */
    t = torrent_new(options.magnet_uri, options.path, options.port, ipptr);
    if (!t) {
        throw("torrent failed to initialize");
    }

    /* initialize queue for receiving peers */
    struct Queue * peer_queue = queue_new();

    /* initialize queue for receiving metadata chunks */
    struct Queue * metadata_queue = queue_new();

    /* initialize queue for receiving file chunks */
    struct Queue * data_queue = queue_new();

    /* initialize thread pool */
    tp = thread_pool_new(10);
    if (!tp) {
        throw("thread pool failed to init");
    }

    /* listen for connecting peers */
    if (listen_for_peers(t, tp, peer_queue) == EXIT_FAILURE) {
        throw("failed to listen for peers");
    }

    /* if we're in debug mode, initialize a peer on localhost.
     *
     * this allows us to simulate 2 sides of a peer connection
     * and make sure we're playing nicely with others.
     *
     * longer term, testing should be done with someone elses
     * implementation, but this is ok for dev
     */
    if (options.debug == 1) {
        // 2130706433 == 127.0.0.1
        struct Peer * p = peer_new(2130706433, 5000);
        torrent_add_peer(t, tp, p);
    }

    while (running) {
        /* STATE MANAGEMENT */

        // run any trackers that have actions to perform
        if (options.debug == 0) {
            torrent_run_trackers(t, tp, peer_queue);
        }

        // collect and initialize peers
        while (queue_get_count(peer_queue) > 0) {
            struct Peer * p = queue_pop(peer_queue);
            torrent_add_peer(t, tp, p);
        }

        // run any peers that have actions to perform
        torrent_assign_upload_slots(t);
        torrent_run_peers(t, tp, metadata_queue, data_queue);

        // update metadata with chunks from peers
        torrent_data_release_expired_claims(t->torrent_metadata);
        if (queue_get_count(metadata_queue) > 0) {
            struct PEER_MSG_EXTENSION * metadata_msg = (struct PEER_MSG_EXTENSION *) queue_pop(metadata_queue);
            torrent_process_metadata_piece(t, metadata_msg);
            free(metadata_msg);
        }

        // update files with chunks from peers
        torrent_data_release_expired_claims(t->torrent_data);
        if (queue_get_count(data_queue) > 0) {
            struct PEER_MSG_PIECE * data_msg = (struct PEER_MSG_PIECE *) queue_pop(data_queue);
            torrent_process_data_chunk(t, data_msg);
            free(data_msg);
        }

        // display some kind of progress
        if (stdin_available()) {
            if (running == 1) {
                char c = getchar();
                if (c == 'q') {
                    running = 0;
                }
            }
        }
    }

    thread_pool_free(tp);

    while(queue_get_count(peer_queue) > 0) {
        struct Peer * p = (struct Peer *) queue_pop(peer_queue);
        peer_free(p);
    }

    while(queue_get_count(metadata_queue) > 0) {
        struct PEER_MSG_EXTENSION * metadata_msg = (struct PEER_MSG_EXTENSION *) queue_pop(metadata_queue);
        free(metadata_msg);
    }

    while(queue_get_count(data_queue) > 0) {
        struct PEER_MSG_PIECE * data_msg = (struct PEER_MSG_PIECE *) queue_pop(data_queue);
        free(data_msg);
    }

    queue_free(peer_queue);
    queue_free(metadata_queue);
    queue_free(data_queue);

    torrent_free(t);

    return EXIT_SUCCESS;

    error:
    thread_pool_free(tp);

    while(queue_get_count(peer_queue) > 0) {
        struct Peer * p = (struct Peer *) queue_pop(peer_queue);
        peer_free(p);
    }

    while(queue_get_count(metadata_queue) > 0) {
        struct PEER_EXTENSION * metadata_msg = queue_pop(metadata_queue);
        free(metadata_msg);
    }

    queue_free(peer_queue);

    torrent_free(t);
    return EXIT_FAILURE;
}
