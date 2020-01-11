//
// Created by vongalixor on 2019-12-10.
//

#include "torrent.h"
#include "../tracker/tracker.h"
#include "../yuarel/yuarel.h"
#include "../macros.h"
#include "../thread_pool/thread_pool.h"
#include "../hash_map/hash_map.h"
#include "../peer/peer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string.h>

/* private functions */
static int torrent_parse_magnet_uri(struct Torrent *t) {
    struct yuarel url;

    char prefix[] = "http://blank.com";
    char ignore[] = "magnet:";

    if (strncmp(ignore, t->magnet_uri, strlen(ignore)) != 0) {
        return EXIT_FAILURE;
    }

    int prefix_size = (sizeof(prefix) / sizeof(prefix[0]) - 1);
    int ignore_size = (sizeof(ignore) / sizeof(ignore[0]) - 1);

    char url_string[sizeof(prefix) + strlen(t->magnet_uri) + 1];

    strncpy(url_string, prefix, sizeof(prefix));
    strncpy(url_string + (prefix_size), t->magnet_uri + (ignore_size), strlen(t->magnet_uri) + 1);

    if (-1 == yuarel_parse(&url, url_string)) {
        return EXIT_FAILURE;
    }

    {
        int p;
        struct yuarel_param params[10];

        p = yuarel_parse_query(url.query, '&', params, 10);
        while (p-- > 0) {

            if (strcmp(params[p].key, "dn") == 0) {
                t->name = strndup(params[p].val, strlen(params[p].val));
                if (!t->name) {
                    throw("failed to set torrent name");
                }

            } else if (strcmp(params[p].key, "xt") == 0) {
                t->info_hash = strndup(params[p].val, strlen(params[p].val));
                if (!t->info_hash) {
                    throw("failed to set torrent info_hash");
                }

                char * trimmed_info_hash = strrchr(t->info_hash, ':') + 1;
                int pos = 0;
                /* hex string to int8_t array */
                for(int count = 0; count < sizeof(t->info_hash_hex); count++) {
                    sscanf(trimmed_info_hash + pos, "%2hhx", &t->info_hash_hex[count]);
                    pos += 2 * sizeof(char);
                }

            } else if (strcmp(params[p].key, "tr") == 0) {
                if (torrent_add_tracker(t, params[p].val) == EXIT_FAILURE) {
                    throw("failed to add tracker");
                }
            }
        }
    }

    return EXIT_SUCCESS;
    error:
    return EXIT_FAILURE;
}

/* public functions */
struct Torrent *torrent_new(char *magnet_uri, char *path) {
    struct Torrent *t = NULL;

    t = malloc(sizeof(struct Torrent));
    if (!t) {
        throw("Torrent failed to malloc")
    }
    /* zero out variables */
    t->magnet_uri = NULL;
    t->path = NULL;
    t->name = NULL;
    t->info_hash = NULL;

    t->tracker_count = 0;

    pthread_mutex_init(&t->downloaded_mutex, NULL);
    t->downloaded = 0;
    pthread_mutex_init(&t->left_mutex, NULL);
    t->left = 0;
    pthread_mutex_init(&t->uploaded_mutex, NULL);
    t->uploaded = 0;

    memset(t->trackers, 0, sizeof t->trackers);
    t->peers = NULL;

    /* set variables */
    t->magnet_uri = strndup(magnet_uri, strlen(magnet_uri));
    if (!t->magnet_uri) {
        throw("torrent failed to set magnet_uri");
    }

    t->path = strndup(path, strlen(path));
    if (!t->path) {
        throw("torrent failed to set path");
    }

    /* try to parse given magnet uri */
    if (torrent_parse_magnet_uri(t) == EXIT_FAILURE) {
        throw("torrent failed to parse magnet_uri");
    }

    log_info("preparing to download torrent :: %s", t->name);
    log_info("torrent info_hash :: %s", t->info_hash);
    log_info("saving torrent to path :: %s", t->path);

    for (int i = 0; i < t->tracker_count; i++) {
        struct Tracker *tr = t->trackers[i];
        log_info("tracker :: %s", tr->url);
    }

    t->peers = hashmap_new(500);

    return t;
    error:
    torrent_free(t);

    return NULL;
}

int torrent_add_tracker(struct Torrent *t, char *url) {
    if (t->tracker_count < MAX_TRACKERS) {
        struct Tracker *tr = tracker_new(url);
        if (!tr) {
            throw("tracker failed to init");
        }

        t->trackers[t->tracker_count] = tr;
        t->tracker_count++;

    } else {
        log_warn("tracker being ignored :: %s", url);
    }
    return EXIT_SUCCESS;
    error:
    return EXIT_FAILURE;
}

int torrent_run_trackers(struct Torrent *t, struct ThreadPool *tp, struct Queue * peer_queue) {
    struct Job *j = NULL;
    for (int i = 0; i < t->tracker_count; i++) {
        struct Tracker *tr = t->trackers[i];
        struct JobArg args[6] = {
                {
                        .arg = (void *) tr,
                        .mutex = NULL
                },
                {
                        .arg = (void *) &t->downloaded,
                        .mutex = (void *) &t->downloaded_mutex
                },
                {
                        .arg = (void *) &t->left,
                        .mutex = (void *) &t->left_mutex
                },
                {
                        .arg = (void *) &t->uploaded,
                        .mutex =  (void *) &t->uploaded_mutex
                },
                {
                        .arg = (void *) &t->info_hash_hex,
                        .mutex =  NULL
                },
                {
                        .arg = (void *) peer_queue,
                        .mutex =  NULL
                }
        };
        j = job_new(
                &tracker_run,
                sizeof(args) / sizeof(struct JobArg),
                args
        );
        if (!j) {
            throw("job failed to init");
        }

        thread_pool_add_job(tp, j);
    }
    return EXIT_SUCCESS;

    error:
    job_free(j);
    return EXIT_FAILURE;
}

struct Torrent *torrent_free(struct Torrent *t) {
    if (t) {
        pthread_mutex_destroy(&t->downloaded_mutex);
        pthread_mutex_destroy(&t->left_mutex);
        pthread_mutex_destroy(&t->uploaded_mutex);

        if (t->magnet_uri) {
            free(t->magnet_uri);
            t->magnet_uri = NULL;
        }
        if (t->path) {
            free(t->path);
            t->path = NULL;
        }
        if (t->name) {
            free(t->name);
            t->name = NULL;
        }
        if (t->info_hash) {
            free(t->info_hash);
            t->info_hash = NULL;
        }

        for (int i = 0; i < t->tracker_count; i++) {
            struct Tracker *tr = t->trackers[i];
            if (tr) {
                tracker_free(tr);
                tr = NULL;
            }
        }

        if (t->peers != NULL) {
            struct Peer * p = hashmap_empty(t->peers);
            while (p != NULL) {
                peer_free(p);
                p = hashmap_empty(t->peers);
            }

            hashmap_free(t->peers);
        }
        
        free(t);
        t = NULL;
    }

    return t;
}
