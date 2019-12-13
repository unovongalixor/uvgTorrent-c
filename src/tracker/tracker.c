#include "tracker.h"
#include "../net_utils.h"
#include "../macros.h"
#include "../yuarel/yuarel.h"
#include <stdlib.h>
#include <stdint.h>
#include <curl/curl.h>
#include <libdill.h>
#include <inttypes.h>

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
    tr->local_port = 0;
    tr->connected = 0;
    tr->connection_id = 0;
    tr->interval = 0;
    tr->seeders = 0;
    tr->leechers = 0;

    tr->socket = 0;

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

  struct ipaddr local;
  ipaddr_local(&local, NULL, 0, 0);

  int64_t deadline = now() + 1000;
  struct ipaddr remote;
  ipaddr_remote(&remote, tr->host, tr->port, 0, deadline);

  tr->socket = udp_open(&local, &remote);
  if (!tr->socket) {
    throw("failed to connect");
  }

  struct TRACKER_UDP_CONNECT_SEND connect_send;
  connect_send.connection_id = net_utils.htonll(0x41727101980);
  connect_send.action = net_utils.htonl(0);
  int32_t r = random();
  connect_send.transaction_id = net_utils.htonl(r);

  udp_send(tr->socket, NULL, &connect_send, sizeof(struct TRACKER_UDP_CONNECT_SEND));

  struct TRACKER_UDP_CONNECT_RECEIVE connect_receive;
  connect_receive.action = 0;
  connect_receive.transaction_id = 0;
  connect_receive.connection_id = 0;

  deadline = now() + 1000;
  ssize_t sz = udp_recv(tr->socket, NULL, &connect_receive, sizeof(struct TRACKER_UDP_CONNECT_RECEIVE), deadline);

  connect_receive.action =  net_utils.ntohl(connect_receive.action);
  connect_receive.transaction_id =  net_utils.ntohl(connect_receive.transaction_id);
  connect_receive.connection_id =  net_utils.ntohll(connect_receive.connection_id);

  log_info("ssize_t sz %zd", sz);
  printf("action %" PRId32 "\n", connect_receive.action);
  printf("connect_receive.transaction_id %" PRId32 "\n", connect_receive.transaction_id);
  printf("connect_send.transaction_id %" PRId32 "\n", r);


  /*

  */
error:
  return;
}

void tracker_announce(struct Tracker * tr) {

}

struct Tracker * tracker_free(struct Tracker * tr) {
    if (tr) {
        if (tr->url) { free(tr->url); tr->url = NULL; }
        if (tr->host) { free(tr->host); tr->host = NULL; }
        hclose(tr->socket); tr->socket = 0;
        free(tr);
        tr = NULL;
    }

    return tr;
}
