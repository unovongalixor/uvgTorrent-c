//
// Created by vongalixor on 2019-12-10.
//

#include "torrent.h"
#include "yuarel.h"
#include "macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string.h>


struct Torrent * torrent_new(char * magnet_uri) {
    struct Torrent *t = malloc(sizeof(struct Torrent));

    /* zero out variables */
    t->magnet_uri = NULL;

    t->metadata_loaded = 0;
    t->chunk_size = 0;
    t->size = 0;

    /* set variables */
    t->magnet_uri = strndup(magnet_uri, strlen(magnet_uri));
    if (torrent_parse_magnet_uri(t) == EXIT_FAILURE) {
        torrent_free(t);
        return NULL;
    }
    return t;
}

void torrent_free(struct Torrent * t) {
    if (t) {
        if (t->magnet_uri) { free(t->magnet_uri); }
        free(t);
    }
}

int torrent_parse_magnet_uri(struct Torrent * t) {
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
        printf("\t%s: %s\n", params[p].key, params[p].val);
    }

    return EXIT_SUCCESS;
}