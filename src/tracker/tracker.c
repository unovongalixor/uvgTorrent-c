#include "tracker.h"
#include "../macros.h"
#include <stdlib.h>
#include <stdint.h>
#include <curl/curl.h>

/* private functions */


/* public functions */
struct Tracker * tracker_new(char * url) {
    struct Tracker *tr = NULL;
    char * decoded_url = NULL;
    CURL *curl = NULL;

    tr = malloc(sizeof(struct Tracker));
    if (!tr) {
      throw("tracker failed to malloc");
    }

    /* zero out variables */
    tr->url = NULL;

    tr->connected = 0;
    tr->connection_id = 0;
    tr->interval = 0;
    tr->seeders = 0;
    tr->leechers = 0;

    /* set variables */
    curl = curl_easy_init();
    int out_length;
    decoded_url = curl_easy_unescape(curl, url, 0, &out_length);
    if (!decoded_url) {
      throw("failed to decode tracker url %s", url);
    }
    tr->url = strndup(decoded_url, strlen(decoded_url));
    if (!tr->url) {
        throw("failed to set tracker url");
    }

    curl_easy_cleanup(curl);
    free(decoded_url);
    return tr;
error:
    if (tr) {
      tracker_free(tr);
    }
    if (decoded_url) {
      free(decoded_url);
    }
    if (curl) {
      curl_easy_cleanup(curl);
    }

    return NULL;
}

void tracker_connect(struct Tracker * tr) {

}

void tracker_announce(struct Tracker * tr) {

}

struct Tracker * tracker_free(struct Tracker * tr) {
    if (tr) {
        if (tr->url) { free(tr->url); tr->url = NULL; }
        free(tr);
        tr = NULL;
    }

    return tr;
}
