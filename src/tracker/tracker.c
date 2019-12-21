#include "tracker.h"
#include "../net_utils/net_utils.h"
#include "../thread_pool/thread_pool.h"
#include "../macros.h"
#include "../yuarel/yuarel.h"
#include <stdlib.h>
#include <stdint.h>
#include <curl/curl.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdarg.h>

/* private functions */

// returns timeout
int tracker_get_timeout(struct Tracker * tr) {
  //return 1;
  return 60 * 2 ^ tr->message_attempts;
}

void tracker_clear_socket(struct Tracker * tr) {
  if(tr->socket) {
    close(tr->socket);
    tr->socket = 0;
  }
}

/*
Set n to 0.
If no response is received after 60 * 2 ^ n seconds, resend the connect request and increase n.
If a response is received, reset n to 0.
*/
void tracker_message_failed(struct Tracker * tr) {
  tr->message_attempts++;
  tracker_clear_socket(tr);
  tracker_set_status(tr, TRACKER_UNCONNECTED);
}

void tracker_message_succeded(struct Tracker * tr) {
  tr->message_attempts = 0;
}

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

    tr->socket = 0;

    tracker_set_status(tr, TRACKER_UNCONNECTED);
    tr->message_attempts = 0;

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

void tracker_set_status(struct Tracker * tr, enum TrackerStatus s) {
  tr->status = s;
}

int tracker_should_connect(struct Tracker * tr) {
  return tr->status == TRACKER_UNCONNECTED;
}

int tracker_connect(int cancel_flag, struct Queue * q, ...) {
  va_list args;
  va_start(args, q);

  struct Tracker * tr = (struct Tracker *)va_arg(args, struct Tracker *);

  tracker_set_status(tr, TRACKER_CONNECTING);

  log_info("connecting to tracker :: %s on port %i", tr->host, tr->port);

  struct addrinfo *remote_addrinfo, *result_addrinfo;
  struct addrinfo remote_hints;
  memset(&remote_hints,0,sizeof(remote_hints));
  remote_hints.ai_family=AF_UNSPEC;
  remote_hints.ai_socktype=SOCK_DGRAM;
  remote_hints.ai_protocol=0;
  remote_hints.ai_flags=AI_ADDRCONFIG;

  char str_port[10];
  memset(str_port, '\0', sizeof(str_port));
  sprintf(str_port, "%i", tr->port);

  if(getaddrinfo(tr->host, str_port, &remote_hints, &remote_addrinfo) != 0) {
    throw("failed to getaddrinfo :: %s %i", tr->host, tr->port);
  }

  for (result_addrinfo = remote_addrinfo; result_addrinfo != NULL; result_addrinfo = result_addrinfo->ai_next) {
        tr->socket = socket(result_addrinfo->ai_family, result_addrinfo->ai_socktype, result_addrinfo->ai_protocol);
        if (tr->socket == -1) {
          tr->socket = 0;
          continue;
        }

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        if (net_utils.connect(tr->socket, result_addrinfo->ai_addr, result_addrinfo->ai_addrlen, &timeout) == -1) {
          // fail
          tracker_clear_socket(tr);
        } else {
          // success
          break;
        }
  }
  if(remote_addrinfo) { freeaddrinfo(remote_addrinfo); remote_addrinfo=NULL; }

  // no addr succeeded
  if (result_addrinfo == NULL) {
    tracker_message_failed(tr);
    throw("could not connect");
  }

  struct TRACKER_UDP_CONNECT_SEND connect_send;
  connect_send.connection_id = net_utils.htonll(0x41727101980);
  connect_send.action = net_utils.htonl(0);
  int32_t transaction_id = random();
  connect_send.transaction_id = net_utils.htonl(transaction_id);

  if (write(tr->socket, &connect_send, sizeof(connect_send)) != sizeof(connect_send)) {
    tracker_message_failed(tr);
    throw("partial write :: %s %i", tr->host, tr->port);
  }

  struct TRACKER_UDP_CONNECT_RECEIVE connect_receive;
  connect_receive.action = 0;
  connect_receive.transaction_id = 0;
  connect_receive.connection_id = 0;

  // set socket read timeout
  struct timeval read_timeout;
  read_timeout.tv_sec = tracker_get_timeout(tr);
  read_timeout.tv_usec = 0;

  struct timeval increment_timeout;
  increment_timeout.tv_sec = 1;
  increment_timeout.tv_usec = 0;

  FD_ZERO(&tr->fdread);
  FD_SET(tr->socket, &tr->fdread );

  while(1){
    int selectStatus = select(tr->socket+1, &tr->fdread, NULL, NULL, &increment_timeout);
    if (selectStatus == -1) {
      tracker_message_failed(tr);
      throw("socket error :: %s %i", tr->host, tr->port);
    } else if (selectStatus == 0) {
      if (cancel_flag == 1) {
        throw("exiting uvgTorrent :: %s %i", tr->host, tr->port);
      }
      read_timeout.tv_sec--;
      if (read_timeout.tv_sec == 0){
        tracker_message_failed(tr);
        throw("connect timed out :: %s %i", tr->host, tr->port);
      }
    } else {
      if (read(tr->socket, &connect_receive, sizeof(connect_receive)) == -1) {
        tracker_message_failed(tr);
        throw("read failed :: %s %i", tr->host, tr->port);
      }
      tracker_message_succeded(tr);
      break;
    }
  }

  connect_receive.action =  net_utils.ntohl(connect_receive.action);
  connect_receive.transaction_id =  net_utils.ntohl(connect_receive.transaction_id);
  connect_receive.connection_id =  net_utils.ntohll(connect_receive.connection_id);

  if(connect_receive.action == 0 && connect_receive.transaction_id == transaction_id) {
    log_info("connected to tracker :: %s on port %i", tr->host, tr->port);
    tracker_set_status(tr, TRACKER_CONNECTED);
  }

  return EXIT_SUCCESS;
error:
  if(tr->socket) { close(tr->socket); tr->socket = 0; }
  if(remote_addrinfo) { freeaddrinfo(remote_addrinfo); remote_addrinfo=NULL; }
  return EXIT_FAILURE;
}

void tracker_announce(struct Tracker * tr) {

}

struct Tracker * tracker_free(struct Tracker * tr) {
    if (tr) {
        if (tr->url) { free(tr->url); tr->url = NULL; }
        if (tr->host) { free(tr->host); tr->host = NULL; }
        tracker_clear_socket(tr);
        free(tr);
        tr = NULL;
    }

    return tr;
}
