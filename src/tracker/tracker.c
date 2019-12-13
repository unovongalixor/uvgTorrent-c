#include "tracker.h"
#include "../macros.h"
#include <stdlib.h>
#include <stdint.h>
#include <curl/curl.h>

/* private functions */


/* public functions */
struct Tracker * tracker_new(char * url) {
    struct Tracker *t = malloc(sizeof(struct Tracker));
    if (!t) {
      return tracker_free(t);
    }

    /* zero out variables */
    t->url = NULL;

    t->connected = 0;
    t->connection_id = 0;
    t->interval = 0;
    t->seeders = 0;
    t->leechers = 0;

    /* set variables */
    char * decoded_url = curl_unescape(url, strlen(url));
    if (!decoded_url) {
      return tracker_free(t);
    }
    t->url = strndup(decoded_url, strlen(decoded_url));
    free(decoded_url);
    if (!t->url) {
        return tracker_free(t);
    }

    return t;
}

struct Tracker * tracker_free(struct Tracker * t) {
    if (t) {
        if (t->url) { free(t->url); t->url = NULL; }
        free(t);
        t = NULL;
    }

    return t;
}
