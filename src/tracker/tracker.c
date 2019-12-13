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
    decoded_url = curl_unescape(url, strlen(url));
    if (!decoded_url) {
      throw("failed to decode tracker url %s", url);
    }
    tr->url = strndup(decoded_url, strlen(decoded_url));
    if (!tr->url) {
        throw("failed to set tracker url");
    }

    free(decoded_url);
    return tr;
error:
    if(tr){
      tracker_free(tr);
    }
    if(decoded_url){
      free(decoded_url);
    }

    return NULL;
}

struct Tracker * tracker_free(struct Tracker * tr) {
    if (tr) {
        if (tr->url) { free(tr->url); tr->url = NULL; }
        free(tr);
        tr = NULL;
    }

    return tr;
}
