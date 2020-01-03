#include "tracker.h"
#include "../net_utils/net_utils.h"
#include "../thread_pool/thread_pool.h"
#include "../macros.h"
#include "../deadline/deadline.h"
#include "../yuarel/yuarel.h"
#include "../peer/peer.h"
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
#include <sched.h>
#include <arpa/inet.h>

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
    tr->announce_deadline = 0;
    tr->scrape_deadline = 0;
    tr->seeders = 0;
    tr->leechers = 0;

    tr->socket = 0;

    tr->status = TRACKER_IDLE;
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

int tracker_run(int *cancel_flag, ...) {
    va_list args;
    va_start(args, cancel_flag);

    struct JobArg tr_job_arg = va_arg(args, struct JobArg);
    struct Tracker *tr = (struct Tracker *) tr_job_arg.arg;

    /* state info */
    struct JobArg downloaded_job_arg = va_arg(args, struct JobArg);
    int64_t * downloaded = (int64_t *) downloaded_job_arg.arg;
    struct JobArg left_job_arg = va_arg(args, struct JobArg);
    int64_t * left = (int64_t *) left_job_arg.arg;
    struct JobArg uploaded_job_arg = va_arg(args, struct JobArg);
    int64_t * uploaded = (int64_t *) uploaded_job_arg.arg;

    /* torrent info */
    struct JobArg info_hash_job_arg = va_arg(args, struct JobArg);
    char * info_hash = (char *) info_hash_job_arg.arg;

    /* resonse queues */
    struct JobArg peer_queue_job_arg = va_arg(args, struct JobArg);
    struct Queue * peer_queue = (struct Queue *) peer_queue_job_arg.arg;

    while (*cancel_flag != 1) {
        if (tracker_should_announce(tr)) {
            if (tracker_connect(tr, cancel_flag) == EXIT_SUCCESS){
                job_arg_lock(downloaded_job_arg);
                job_arg_lock(left_job_arg);
                job_arg_lock(uploaded_job_arg);
                tracker_announce(tr, cancel_flag, *downloaded, *left, *uploaded, info_hash, peer_queue);
                job_arg_unlock(downloaded_job_arg);
                job_arg_unlock(left_job_arg);
                job_arg_unlock(uploaded_job_arg);

                tracker_disconnect(tr);
            }
        }

        /* add scrape request */

        /* sleep the thread until we are supposed to perform the next announce or scrape */
        int64_t current_time = now();
        if (tr->announce_deadline < now() | tr->announce_deadline < now()) {
            pthread_cond_t condition;
            pthread_mutex_t mutex;

            pthread_cond_init(&condition, NULL);
            pthread_mutex_init(&mutex, NULL);
            pthread_mutex_unlock(&mutex);

            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            struct timespec timeout_spec;
            clock_gettime(CLOCK_REALTIME, &timeout_spec);
            timeout_spec.tv_sec += 1;

            pthread_cond_timedwait(&condition, &mutex, &timeout_spec);

            pthread_cond_destroy(&condition);
            pthread_mutex_destroy(&mutex);
        }
    }
}

int tracker_disconnect(struct Tracker *tr) {
    if (tr->socket) {
        close(tr->socket);
        tr->socket = 0;
    }
}

int tracker_connect(struct Tracker *tr, int *cancel_flag) {
    tr->status = TRACKER_CONNECTING;

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
        if (net_utils.connect(tr->socket, result_addrinfo->ai_addr, result_addrinfo->ai_addrlen, &timeout) == 0) {
            // success
            break;
        } else {
            // fail
            tracker_disconnect(tr);
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

    struct TRACKER_UDP_CONNECT_RECEIVE connect_receive;

    struct timeval connect_timeout;
    connect_timeout.tv_sec = tracker_get_timeout(tr);
    connect_timeout.tv_usec = 0;

    while(connect_timeout.tv_sec > 0) {
        if (write(tr->socket, &connect_send, sizeof(connect_send)) != sizeof(connect_send)) {
            tracker_message_failed(tr);
            throw("partial write :: %s %i", tr->host, tr->port);
        }

        // set socket read timeout
        int read_timeout_length = 5;

        struct timeval read_timeout;
        read_timeout.tv_sec = read_timeout_length;
        read_timeout.tv_usec = 0;

        size_t response_length = net_utils.read(tr->socket, &connect_receive, sizeof(connect_receive), &read_timeout, cancel_flag);
        if (response_length == -1) {
            // error
            tracker_message_failed(tr);
            throw("read failed :: %s on port %i", tr->host, tr->port);
        } else if (response_length == 0) {
            // timeout
        } else if (response_length != sizeof(struct TRACKER_UDP_CONNECT_RECEIVE)) {
            // corruption
            tracker_message_failed(tr);
            throw("incomplete read :: %s on port %i", tr->host, tr->port);
        } else {
            // success
            break;
        }

        if (*cancel_flag == 1) {
            throw("exiting uvgTorrent :: %s on port %i", tr->host, tr->port);
        }

        connect_timeout.tv_sec -= read_timeout_length;
        if (connect_timeout.tv_sec == 0) {
            tracker_message_failed(tr);
            throw("connect timedout :: %s on port %i", tr->host, tr->port);
        }
    }

    connect_receive.action = net_utils.ntohl(connect_receive.action);
    connect_receive.transaction_id = net_utils.ntohl(connect_receive.transaction_id);
    connect_receive.connection_id = net_utils.ntohll(connect_receive.connection_id);

    if (connect_receive.action == 0) {
        if (connect_receive.transaction_id == transaction_id) {
            log_info("connected to tracker :: %s on port %i", tr->host, tr->port);
            tr->connection_id = connect_receive.connection_id;
            tr->status = TRACKER_CONNECTED;
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
    if (tr->status == TRACKER_IDLE && tr->announce_deadline < now()) {
        return 1;
    }
    return 0;
}

int tracker_announce(struct Tracker *tr, int *cancel_flag, int64_t downloaded, int64_t left, int64_t uploaded, char * info_hash, struct Queue * peer_queue) {
    if(tr->status != TRACKER_CONNECTED) {
        return EXIT_FAILURE;
    }

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

    tr->status = TRACKER_ANNOUNCING;

    log_info("announcing tracker :: %s on port %i", tr->host, tr->port);

    // prepare request
    int32_t transaction_id = random();
    struct TRACKER_UDP_ANNOUNCE_SEND announce_send = {
            .connection_id=net_utils.htonll(tr->connection_id),
            .action=net_utils.htonl(1),
            .transaction_id=net_utils.htonl(transaction_id),
            .peer_id=*"UVG01234567891234567",  // byte ordering doesn't matter for array of single bytes
            .downloaded=net_utils.htonll(downloaded),
            .left=net_utils.htonll(left),
            .uploaded=net_utils.htonll(uploaded),
            .event=net_utils.htonl(0),
            .ip=net_utils.htonl(0),
            .key=net_utils.htonl(1),
            .num_want=net_utils.htonl(-1),
            .port=net_utils.htons(0),
            .extensions=net_utils.htons(0)
    };

    memcpy(announce_send.info_hash, info_hash_hex, sizeof(info_hash_hex));

    // prepare response
    int8_t raw_response[65507]; // 65,507 bytes, practical udp datagram size limit
    // (https://en.wikipedia.org/wiki/User_Datagram_Protocol)
    memset(&raw_response, 0, sizeof(raw_response));
    struct TRACKER_UDP_ANNOUNCE_RECEIVE * announce_receive = (struct TRACKER_UDP_ANNOUNCE_RECEIVE *) &raw_response;
    size_t response_length = 0;

    struct timeval announce_timeout;
    announce_timeout.tv_sec = tracker_get_timeout(tr);
    announce_timeout.tv_usec = 0;
    while(announce_timeout.tv_sec > 0) {
        if (write(tr->socket, &announce_send, sizeof(announce_send)) != sizeof(announce_send)) {
            tracker_message_failed(tr);
            throw("partial write :: %s %i", tr->host, tr->port);
        }

        // set socket read timeout
        int read_timeout_length = 5;
        struct timeval read_timeout;
        read_timeout.tv_sec = read_timeout_length;
        read_timeout.tv_usec = 0;

        response_length = net_utils.read(tr->socket, &raw_response, sizeof(raw_response), &read_timeout, cancel_flag);
        if (response_length == -1) {
            // failed
            tracker_message_failed(tr);
            throw("read failed :: %s on port %i", tr->host, tr->port);
        } else if(response_length == 0) {
            // timeout
        } else if (response_length < sizeof(struct TRACKER_UDP_ANNOUNCE_RECEIVE)) {
            // corruption
            tracker_message_failed(tr);
            throw("incomplete read :: %s on port %i", tr->host, tr->port);
        } else {
            // success
            break;
        }

        if (*cancel_flag == 1) {
            throw("exiting uvgTorrent :: %s on port %i", tr->host, tr->port);
        }

        announce_timeout.tv_sec -= read_timeout_length;
        if (announce_timeout.tv_sec == 0) {
            tracker_message_failed(tr);
            throw("announce timedout :: %s on port %i", tr->host, tr->port);
        }
    }

    announce_receive->action = net_utils.ntohl(announce_receive->action);
    announce_receive->transaction_id = net_utils.ntohl(announce_receive->transaction_id);
    announce_receive->interval = net_utils.ntohl(announce_receive->interval);
    announce_receive->leechers = net_utils.ntohl(announce_receive->leechers);
    announce_receive->seeders = net_utils.ntohl(announce_receive->seeders);
    
    if (announce_receive->action == 1) {
        if (announce_receive->transaction_id == transaction_id) {
            tr->announce_deadline = now() + announce_receive->interval * 1000;

            log_info("announced to tracker with interval of " MAGENTA "%i seconds" NO_COLOR " :: "GREEN"%s:%i"NO_COLOR, announce_receive->interval, tr->host, tr->port);

            size_t position = sizeof(struct TRACKER_UDP_ANNOUNCE_RECEIVE);
            size_t peer_size = sizeof(struct TRACKER_UDP_ANNOUNCE_RECEIVE_PEER);

            while (position < response_length) {
                struct TRACKER_UDP_ANNOUNCE_RECEIVE_PEER * current_peer = (struct TRACKER_UDP_ANNOUNCE_RECEIVE_PEER *)
                        announce_receive + position;

                if(current_peer->ip == 0){
                    break;
                }

                struct Peer * p = peer_new(current_peer->ip, current_peer->port);
                if(queue_push(peer_queue, (void *) p) == EXIT_FAILURE) {
                    throw("unable to return peer to torrent :: %s on port %i", tr->host, tr->port);
                }

                position += peer_size;
            }

            tr->status = TRACKER_IDLE;
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
    tracker_disconnect(tr);
    tr->status = TRACKER_IDLE;
}

void tracker_message_succeded(struct Tracker *tr) {
    tr->message_attempts = 0;
}


struct Tracker *tracker_free(struct Tracker *tr) {
    if (tr) {
        if (tr->url) {
            free(tr->url);
            tr->url = NULL;
        }
        if (tr->host) {
            free(tr->host);
            tr->host = NULL;
        }
        tracker_disconnect(tr);
        free(tr);
        tr = NULL;
    }

    return tr;
}
