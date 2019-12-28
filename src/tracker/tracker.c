#include "tracker.h"
#include "../net_utils/net_utils.h"
#include "../thread_pool/thread_pool.h"
#include "../macros.h"
#include "../deadline/deadline.h"
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
#include <string.h>
#include <poll.h>
#include <math.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <arpa/inet.h>


/* private functions */
void tracker_clear_socket(struct Tracker *tr) {
    if (tr->socket) {
        close(tr->socket);
        tr->socket = 0;
    }
}

/* public functions */
struct Tracker *tracker_new(char *url) {
    struct Tracker *tr = NULL;
    char *decoded_url = NULL;
    CURL *curl = NULL;

    tr = malloc(sizeof(struct Tracker));
    if (!tr) {
        throw("tracker failed to malloc");
    }

    /* zero out variables */
    tr->url = NULL;
    tr->host = NULL;

    tr->port = 0;
    tr->connection_id = 0;
    tr->interval = 0;
    tr->seeders = 0;
    tr->leechers = 0;

    tr->socket = 0;

    pthread_mutex_init(&tr->status_mutex, NULL);
    tracker_set_status(tr, TRACKER_UNCONNECTED);
    tr->message_attempts = 0;
    tr->announce_deadline = 0;

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

void tracker_set_status(struct Tracker *tr, enum TrackerStatus s) {
    pthread_mutex_lock(&tr->status_mutex);
    tr->status = s;
    pthread_mutex_unlock(&tr->status_mutex);
}


enum TrackerStatus tracker_get_status(struct Tracker *tr) {
    pthread_mutex_lock(&tr->status_mutex);
    enum TrackerStatus response = tr->status;
    pthread_mutex_unlock(&tr->status_mutex);

    return response;
}

int tracker_should_connect(struct Tracker *tr) {
    return tracker_get_status(tr) == TRACKER_UNCONNECTED;
}

int tracker_connect(int *cancel_flag, struct Queue *q, ...) {
    va_list args;
    va_start(args, q);

    struct JobArg tr_job_arg = va_arg(args, struct JobArg);
    struct Tracker *tr = (struct Tracker *) tr_job_arg.arg;

    tracker_set_status(tr, TRACKER_CONNECTING);

    log_info("connecting to tracker :: %s on port %i", tr->host, tr->port);

    struct addrinfo *remote_addrinfo, *result_addrinfo;
    struct addrinfo remote_hints;
    memset(&remote_hints, 0, sizeof(remote_hints));
    remote_hints.ai_family = AF_UNSPEC;
    remote_hints.ai_socktype = SOCK_DGRAM;
    remote_hints.ai_protocol = 0;
    remote_hints.ai_flags = AI_ADDRCONFIG;

    char str_port[10];
    memset(str_port, '\0', sizeof(str_port));
    sprintf(str_port, "%i", tr->port);

    if (getaddrinfo(tr->host, str_port, &remote_hints, &remote_addrinfo) != 0) {
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

    if (remote_addrinfo) {
        freeaddrinfo(remote_addrinfo);
        remote_addrinfo = NULL;
    }

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

    // set socket read timeout
    struct timeval read_timeout;
    read_timeout.tv_sec = tracker_get_timeout(tr);
    read_timeout.tv_usec = 0;

    size_t response_length = net_utils.read(tr->socket, cancel_flag, &connect_receive, sizeof(connect_receive), &read_timeout);
    if (response_length == -1) {
        tracker_message_failed(tr);
        throw("read failed :: %s on port %i", tr->host, tr->port);
    } else if (response_length != sizeof(connect_receive)) {
        tracker_message_failed(tr);
        throw("incomplete read :: %s on port %i", tr->host, tr->port);
    }

    connect_receive.action = net_utils.ntohl(connect_receive.action);
    connect_receive.transaction_id = net_utils.ntohl(connect_receive.transaction_id);
    connect_receive.connection_id = net_utils.ntohll(connect_receive.connection_id);

    if (connect_receive.action == 0) {
        if (connect_receive.transaction_id == transaction_id) {
            log_info("connected to tracker :: %s on port %i", tr->host, tr->port);
            tr->connection_id = connect_receive.connection_id;
            tracker_set_status(tr, TRACKER_CONNECTED);
        } else {
            tracker_message_failed(tr);
            throw("incorrect transaction_id from tracker :: %s on port %i", tr->host, tr->port);
        }
    } else {
        tracker_message_failed(tr);
        throw("incorrect action from tracker :: %s on port %i", tr->host, tr->port);
    }

    return EXIT_SUCCESS;
    error:
    if (tr->socket) {
        close(tr->socket);
        tr->socket = 0;
    }
    if (remote_addrinfo) {
        freeaddrinfo(remote_addrinfo);
        remote_addrinfo = NULL;
    }
    return EXIT_FAILURE;
}

int tracker_should_announce(struct Tracker *tr) {
    if (tracker_get_status(tr) == TRACKER_CONNECTED && tr->announce_deadline < now()) {
        return 1;
    }
    return 0;
}

int tracker_announce(int *cancel_flag, struct Queue *q, ...) {
    va_list args;
    va_start(args, q);

    struct JobArg tr_job_arg = va_arg(args, struct JobArg);
    struct Tracker *tr = (struct Tracker *) tr_job_arg.arg;
    struct JobArg downloaded_job_arg = va_arg(args, struct JobArg);
    int64_t * downloaded = (int64_t *) downloaded_job_arg.arg;
    struct JobArg left_job_arg = va_arg(args, struct JobArg);
    int64_t * left = (int64_t *) left_job_arg.arg;
    struct JobArg uploaded_job_arg = va_arg(args, struct JobArg);
    int64_t * uploaded = (int64_t *) uploaded_job_arg.arg;
    struct JobArg info_hash_job_arg = va_arg(args, struct JobArg);
    char * info_hash = (char *) info_hash_job_arg.arg;

    /*
    struct Tracker *tr = (struct Tracker *) va_arg(args, struct Tracker *);
    int64_t * downloaded = (int64_t *) va_arg(args, int64_t *);
    int64_t * left = (int64_t *) va_arg(args, int64_t *);
    int64_t * uploaded = (int64_t *) va_arg(args, int64_t *);
     */

    // format info_hash for the announce request
    // char * info_hash = (char *) va_arg(args, char *);
    char * trimmed_info_hash = strrchr(info_hash, ':') + 1;
    int8_t info_hash_hex[20];
    int pos = 0;
    /* hex string to int8_t array */
    for(int count = 0; count < sizeof(info_hash_hex); count++) {
        sscanf(trimmed_info_hash + pos, "%2hhx", &info_hash_hex[count]);
        pos += 2 * sizeof(char);
    }

    tracker_set_status(tr, TRACKER_ANNOUNCING);

    log_info("announcing tracker :: %s on port %i", tr->host, tr->port);

    int32_t transaction_id = random();

    job_arg_lock(downloaded_job_arg);
    job_arg_lock(left_job_arg);
    job_arg_lock(uploaded_job_arg);
    struct TRACKER_UDP_ANNOUNCE_SEND announce_send = {
            .connection_id=net_utils.htonll(tr->connection_id),
            .action=net_utils.htonl(1),
            .transaction_id=net_utils.htonl(transaction_id),
            .peer_id=*"UVG01234567891234567",  // byte ordering doesn't matter for array of single bytes
            .downloaded=net_utils.htonll(*downloaded),
            .left=net_utils.htonll(*left),
            .uploaded=net_utils.htonll(*uploaded),
            .event=net_utils.htonl(0),
            .ip=net_utils.htonl(0),
            .key=net_utils.htonl(1),
            .num_want=net_utils.htonl(-1),
            .port=net_utils.htons(0),
            .extensions=net_utils.htons(0)
    };
    job_arg_unlock(downloaded_job_arg);
    job_arg_unlock(left_job_arg);
    job_arg_unlock(uploaded_job_arg);
    memcpy(announce_send.info_hash, info_hash_hex, sizeof(info_hash_hex));

    if (write(tr->socket, &announce_send, sizeof(announce_send)) != sizeof(announce_send)) {
        tracker_message_failed(tr);
        throw("partial write :: %s %i", tr->host, tr->port);
    }

    int8_t raw_response[65507]; // 65,507 bytes, practical udp datagram size limit
                                // (https://en.wikipedia.org/wiki/User_Datagram_Protocol)
    struct TRACKER_UDP_ANNOUNCE_RECEIVE * announce_receive = (struct TRACKER_UDP_ANNOUNCE_RECEIVE *) &raw_response;

    // set socket read timeout
    struct timeval read_timeout;
    read_timeout.tv_sec = tracker_get_timeout(tr);
    read_timeout.tv_usec = 0;

    size_t response_length = net_utils.read(tr->socket, cancel_flag, &raw_response, sizeof(raw_response), &read_timeout);
    if (response_length == -1) {
        tracker_message_failed(tr);
        throw("read failed :: %s on port %i", tr->host, tr->port);
    }

    announce_receive->action = net_utils.ntohl(announce_receive->action);
    announce_receive->transaction_id = net_utils.ntohl(announce_receive->transaction_id);
    announce_receive->interval = net_utils.ntohl(announce_receive->interval);
    announce_receive->leechers = net_utils.ntohl(announce_receive->leechers);
    announce_receive->seeders = net_utils.ntohl(announce_receive->seeders);
    
    if (announce_receive->action == 1) {
        if (announce_receive->transaction_id == transaction_id) {
            log_info("announced to tracker :: %s on port %i", tr->host, tr->port);

            size_t position = sizeof(struct TRACKER_UDP_ANNOUNCE_RECEIVE);
            size_t peer_size = sizeof(struct TRACKER_UDP_ANNOUNCE_RECEIVE_PEER);

            while (position < response_length) {
                struct TRACKER_UDP_ANNOUNCE_RECEIVE_PEER * current_peer = (struct TRACKER_UDP_ANNOUNCE_RECEIVE_PEER *)
                        announce_receive + position;

                struct in_addr peer_ip;
                peer_ip.s_addr = current_peer->ip;
                char * ip = inet_ntoa(peer_ip);

                if(strcmp(ip,"0.0.0.0") == 0){
                    break;
                }

                log_info("got peer %s:%" PRIu16 "", ip, current_peer->port);
                position += peer_size;
            }

            tracker_set_status(tr, TRACKER_ANNOUNCED);
        } else {
            tracker_message_failed(tr);
            throw("incorrect transaction_id from tracker :: %s on port %i", tr->host, tr->port);
        }
    } else {
        tracker_message_failed(tr);
        throw("incorrect action from tracker :: %s on port %i", tr->host, tr->port);
    }

    return EXIT_SUCCESS;
    error:
    return EXIT_FAILURE;
}

int tracker_get_timeout(struct Tracker *tr) {
    return 60 * pow(2, tr->message_attempts);
}

void tracker_message_failed(struct Tracker *tr) {
    /*
    Set n to 0.
    If no response is received after 60 * 2 ^ n seconds, resend the connect request and increase n.
    If a response is received, reset n to 0.
    */
    tr->message_attempts++;
    tracker_clear_socket(tr);
    tracker_set_status(tr, TRACKER_UNCONNECTED);
}

void tracker_message_succeded(struct Tracker *tr) {
    tr->message_attempts = 0;
}


struct Tracker *tracker_free(struct Tracker *tr) {
    if (tr) {
        pthread_mutex_destroy(&tr->status_mutex);
        if (tr->url) {
            free(tr->url);
            tr->url = NULL;
        }
        if (tr->host) {
            free(tr->host);
            tr->host = NULL;
        }
        tracker_clear_socket(tr);
        free(tr);
        tr = NULL;
    }

    return tr;
}
