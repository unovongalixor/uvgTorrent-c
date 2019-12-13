//
// Created by vongalixor on 2019-12-10.
//

#include "torrent.h"
#include "../tracker/tracker.h"
#include "../yuarel/yuarel.h"
#include "../macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string.h>

/* private functions */
static int torrent_parse_magnet_uri(struct Torrent * t) {
    struct yuarel url;

    char prefix[] = "http://blank.com";
    char ignore[] = "magnet:";

    if (strncmp(ignore, t->magnet_uri, strlen(ignore)) != 0) {
        // we aren't dealign with a properly formatted magnet_uri
        return EXIT_FAILURE;
    }

    int prefix_size = (sizeof(prefix) / sizeof(prefix[0]) - 1);
    int ignore_size = (sizeof(ignore) / sizeof(ignore[0]) - 1);

    char url_string[sizeof(prefix) + strlen(t->magnet_uri) + 1];

    strncpy(url_string, prefix, sizeof(prefix));
    strncpy(url_string+(prefix_size), t->magnet_uri + (ignore_size), strlen(t->magnet_uri) + 1);

    if (-1 == yuarel_parse(&url, url_string)) {
        return EXIT_FAILURE;
    }

    int p;
    struct yuarel_param params[10];

    p = yuarel_parse_query(url.query, '&', params, 10);
    while (p-- > 0) {

        if (strcmp(params[p].key, "dn")==0) {
            t->name = strndup(params[p].val, strlen(params[p].val));
            if (!t->name) {
                return EXIT_FAILURE;
            }

        } else if (strcmp(params[p].key, "xt")==0) {
            t->hash = strndup(params[p].val, strlen(params[p].val));
            if (!t->hash) {
                return EXIT_FAILURE;
            }

        } else if (strcmp(params[p].key, "tr")==0) {
            if(torrent_add_tracker(t, params[p].val) == EXIT_FAILURE){
              return EXIT_FAILURE;
            }
        }

    }

    return EXIT_SUCCESS;
}

/* public functions */
struct Torrent * torrent_new(char * magnet_uri, char * path) {
    struct Torrent *t = NULL;

    t = malloc(sizeof(struct Torrent));
    if (!t) {
      throw("Torrent failed to malloc")
    }
    /* zero out variables */
    t->magnet_uri = NULL;
    t->path = NULL;
    t->name = NULL;
    t->hash = NULL;

    t->tracker_count = 0;
    t->downloaded = 0;
    t->left = 0;
    t->uploaded = 0;

    memset(t->trackers, 0, sizeof t->trackers);

    /* set variables */
    t->magnet_uri = strndup(magnet_uri, strlen(magnet_uri));
    if (!t->magnet_uri) {
        throw("torrent failed to set magnet_uri")
    }

    t->path = strndup(path, strlen(path));
    if (!t->path) {
        throw("torrent failed to set path")
    }

    /* try to parse given magnet uri */
    if (torrent_parse_magnet_uri(t) == EXIT_FAILURE) {
        throw("torrent failed to parse magnet_uri")
    }

    log_info("preparing to download torrent :: %s", t->name);
    log_info("torrent hash :: %s", t->hash);
    log_info("saving torrent to path :: %s", t->path);

    for ( int i = 0; i < t->tracker_count ; i++ ) {
        struct Tracker * tr = t->trackers[i];
        log_info("tracker :: %s", tr->url);
    }

    return t;
error:
    if (t) {
      torrent_free(t);
    }

    return NULL;
}

int torrent_add_tracker(struct Torrent * t, char * url) {
  struct Tracker * tr = tracker_new(url);
  if (!tr) {
    return EXIT_FAILURE;
  }

  t->trackers[t->tracker_count] = tr;
  t->tracker_count++;

  return EXIT_SUCCESS;
}

void torrent_announce_trackers(struct Torrent * t) {
  for ( int i = 0; i < t->tracker_count ; i++ ) {
      struct Tracker * tr = t->trackers[i];
      tracker_announce(tr);
  }
}

struct Torrent * torrent_free(struct Torrent * t) {
    if (t) {
        if (t->magnet_uri) { free(t->magnet_uri); t->magnet_uri = NULL; }
        if (t->path) { free(t->path); t->path = NULL; }
        if (t->name) { free(t->name); t->name = NULL; }
        if (t->hash) { free(t->hash); t->hash = NULL; }

        for ( int i = 0; i < t->tracker_count ; i++ ) {
            struct Tracker * tr = t->trackers[i];
            if (tr) { if(tracker_free(tr)){ return t; } }
        }

        free(t);
        t = NULL;
    }

    return t;
}
