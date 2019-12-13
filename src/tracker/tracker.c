#include "tracker.h"
#include "../macros.h"
#include "../yuarel/yuarel.h"
#include <stdlib.h>
#include <stdint.h>
#include <curl/curl.h>
#define MILL_USE_PREFIX
#include <libmill.h>

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
    tr->host = NULL;

    tr->port = 0;
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

    /* get host and port */
    struct yuarel yurl;
    if (-1 == yuarel_parse(&yurl, decoded_url)) {
        throw("failed to get host:port");
    }
    tr->host = strndup(yurl.host, strlen(yurl.host));
    if (!tr->host) {
        throw("failed to set tracker host");
    }
    tr->port = yurl.port;

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
  log_info("connecting to tracker :: %s on port %i", tr->host, tr->port);
  /*
  ipaddr addr = ipremote(tr->host, tr->port, 0, -1);

  udpsock s = udplisten(addr);
  int port = udpport(s);
  */
}

void tracker_announce(struct Tracker * tr) {

}

struct Tracker * tracker_free(struct Tracker * tr) {
    if (tr) {
        if (tr->url) { free(tr->url); tr->url = NULL; }
        if (tr->host) { free(tr->host); tr->host = NULL; }
        free(tr);
        tr = NULL;
    }

    return tr;
}
