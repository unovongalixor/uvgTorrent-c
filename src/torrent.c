//
// Created by vongalixor on 2019-12-10.
//

#include "torrent.h"
#include "yuarel.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


struct Torrent * new_torrent(char * magnet_uri) {
    struct Torrent *t = malloc(sizeof(struct Torrent));

    /* zero out variables */
    memset(t->magnet_uri, '\0', sizeof(t->magnet_uri));
    memset(t->path, '\0', sizeof(t->path));
    memset(t->name, '\0', sizeof(t->name));
    memset(t->hash, '\0', sizeof(t->hash));

    t->metadata_loaded = 0;
    t->chunk_size = 0;
    t->size = 0;

    /* set variables */
    strncpy(t->magnet_uri, magnet_uri, sizeof(t->magnet_uri));
    torrent_parse_magnet_uri(t);
    return t;
}

int torrent_parse_magnet_uri(struct Torrent * t) {
    struct yuarel url;

    char prefix[] = "http://blank.com";
    char ignore[] = "magnet:";

    int prefix_size = (sizeof(prefix) / sizeof(prefix[0]) - 1);
    int ignore_size = (sizeof(ignore) / sizeof(ignore[0]) - 1);

    char url_string[sizeof(prefix) + sizeof(t->magnet_uri)];

    strncpy(url_string, prefix, sizeof(prefix));
    strncpy(url_string+(prefix_size), t->magnet_uri + (ignore_size), sizeof(t->magnet_uri));

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