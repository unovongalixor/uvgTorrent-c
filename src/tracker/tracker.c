#include "tracker.h"
#include "../net_utils/net_utils.h"
#include "../thread_pool/thread_pool.h"
#include "../log.h"
#include "../deadline/deadline.h"
#include "../yuarel/yuarel.h"
#include "../peer/peer.h"
#include "../torrent/torrent_data.h"
#include <stdlib.h>
#include <stdint.h>
#include <curl/curl.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <string.h>
#include <poll.h>
#include <math.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <sched.h>
#include <arpa/inet.h>
#include <ifaddrs.h>


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

    tr->running = 0;

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

int tracker_should_run(struct Tracker *tr) {
    return (tracker_should_announce(tr) |
            tracker_should_scrape(tr)) & tr->running == 0;
}

int tracker_run(_Atomic int *cancel_flag, ...) {
    va_list args;
    va_start(args, cancel_flag);

    struct JobArg tr_job_arg = va_arg(args, struct JobArg);
    struct Tracker *tr = (struct Tracker *) tr_job_arg.arg;

    /* state info */
    struct JobArg torrent_data_job_arg = va_arg(args, struct JobArg);
    struct TorrentData * torrent_data = (struct TorrentData *) torrent_data_job_arg.arg;

    struct JobArg port_job_arg = va_arg(args, struct JobArg);
    uint16_t * port = (uint16_t *) port_job_arg.arg;

    /* torrent info */
    struct JobArg info_hash_job_arg = va_arg(args, struct JobArg);
    uint8_t (* info_hash) [20] = (uint8_t (*) [20]) info_hash_job_arg.arg;

    uint8_t info_hash_hex[20];
    memcpy(&info_hash_hex, info_hash, sizeof(info_hash_hex));

    /* resonse queues */
    struct JobArg peer_queue_job_arg = va_arg(args, struct JobArg);
    struct Queue * peer_queue = (struct Queue *) peer_queue_job_arg.arg;

    if (*cancel_flag == 1) { return EXIT_FAILURE; }

    /* ANNOUNCE */
    if (tracker_should_announce(tr)) {
        if (tracker_connect(tr, cancel_flag) == EXIT_SUCCESS){
            tracker_announce(tr, cancel_flag, torrent_data->downloaded, torrent_data->left, torrent_data->uploaded, *port, info_hash_hex, peer_queue);
            tracker_disconnect(tr);

            tr->running = 0;
            return EXIT_SUCCESS;
        }
    }

    if (*cancel_flag == 1) { return EXIT_FAILURE; }
    /* SCRAPE */
    if (tracker_should_scrape(tr)) {
        if (tracker_connect(tr, cancel_flag) == EXIT_SUCCESS) {
            tracker_scrape(tr, cancel_flag, info_hash_hex);
            tracker_disconnect(tr);

            tr->running = 0;
            return EXIT_SUCCESS;
        }
    }

    tr->running = 0;
    return EXIT_FAILURE;
}

int tracker_disconnect(struct Tracker *tr) {
    if (tr->socket) {
        close(tr->socket);
        tr->socket = 0;
    }
}

int tracker_connect(struct Tracker *tr, _Atomic int *cancel_flag) {
    tr->status = TRACKER_CONNECTING;

    log_info("connecting to tracker :: %s on port %i", tr->host, tr->port);

    struct addrinfo *remote_addrinfo = NULL;
    struct addrinfo *result_addrinfo = NULL;
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
    memset(&connect_receive, 0x00, sizeof(struct TRACKER_UDP_CONNECT_RECEIVE));

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
            tracker_message_succeded(tr);
            tr->status = TRACKER_CONNECTED;
        } else {
            tracker_message_failed(tr);
            goto error;
            // throw("incorrect transaction_id from tracker :: %s on port %i", tr->host, tr->port);
        }
    } else {
        tracker_message_failed(tr);
        goto error;
        // throw("incorrect action from tracker :: %s on port %i", tr->host, tr->port);
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
    if (tr->status == TRACKER_IDLE && tr->announce_deadline < now() && tr->message_attempts < 5) {
        return 1;
    }
    return 0;
}

int tracker_announce(struct Tracker *tr, _Atomic int *cancel_flag, _Atomic int_fast64_t downloaded, _Atomic int_fast64_t left, _Atomic int_fast64_t uploaded, uint16_t port, uint8_t info_hash_hex[20], struct Queue * peer_queue) {
    if(tr->status != TRACKER_CONNECTED) {
        return EXIT_FAILURE;
    }

    tr->status = TRACKER_ANNOUNCING;

    log_info("announcing tracker :: %s on port %i", tr->host, tr->port);

    struct ifaddrs *local_addr;
    getifaddrs(&local_addr);
    struct sockaddr_in * sa = (struct sockaddr_in *) local_addr->ifa_addr;


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
            .ip=sa->sin_addr.s_addr,
            .key=net_utils.htonl(1),
            .num_want=net_utils.htonl(-1),
            .port=net_utils.htons(port),
            .extensions=net_utils.htons(0)
    };
    memcpy(&announce_send.info_hash, info_hash_hex, sizeof(int8_t[20]));

    // prepare response
    int8_t raw_response[65507]; // 65,507 bytes, practical udp datagram size limit
    // (https://en.wikipedia.org/wiki/User_Datagram_Protocol)
    memset(&raw_response, 0x00, sizeof(raw_response));
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
                        &raw_response[position];
                current_peer->ip = net_utils.ntohl(current_peer->ip);
                current_peer->port = net_utils.ntohs(current_peer->port);

                if(current_peer->ip == 0){
                    break;
                }

                struct Peer * p = peer_new(current_peer->ip, current_peer->port);
                if(queue_push(peer_queue, (void *) p) == EXIT_FAILURE) {
                    throw("unable to return peer to torrent :: %s on port %i", tr->host, tr->port);
                }

                position += peer_size;
            }

            tracker_message_succeded(tr);
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

int tracker_should_scrape(struct Tracker *tr) {
    if (tr->status == TRACKER_IDLE && tr->scrape_deadline < now() && tr->message_attempts < 5) {
        return 1;
    }
    return 0;
}

int tracker_scrape(struct Tracker *tr, _Atomic int *cancel_flag, uint8_t info_hash_hex[20]) {
    if(tr->status != TRACKER_CONNECTED) {
        return EXIT_FAILURE;
    }
    tr->status = TRACKER_SCRAPING;
    struct TRACKER_UDP_SCRAPE_SEND * scrape_send = NULL;

    log_info("scraping tracker :: %s on port %i", tr->host, tr->port);

    // prepare request
    int32_t transaction_id = random();
    size_t send_size = sizeof(struct TRACKER_UDP_SCRAPE_SEND) + sizeof(struct TRACKER_UDP_SCRAPE_SEND_INFO_HASH);

    scrape_send = malloc(send_size);
    scrape_send->connection_id = net_utils.htonll(tr->connection_id);
    scrape_send->action = net_utils.htonl(2);
    scrape_send->transaction_id = net_utils.htonl(transaction_id);
    struct TRACKER_UDP_SCRAPE_SEND_INFO_HASH info_hashes[1];
    memcpy(&info_hashes[0].info_hash, info_hash_hex, sizeof(int8_t[20]));
    memcpy(&scrape_send->info_hashes, &info_hashes, sizeof(info_hashes));

    // prepare response
    int8_t raw_response[65507]; // 65,507 bytes, practical udp datagram size limit
    // (https://en.wikipedia.org/wiki/User_Datagram_Protocol)
    memset(&raw_response, 0, sizeof(raw_response));
    struct TRACKER_UDP_SCRAPE_RECEIVE * scrape_receive = (struct TRACKER_UDP_SCRAPE_RECEIVE *) &raw_response;
    size_t response_length = 0;

    // send message and wait for response
    struct timeval scrape_timeout;
    scrape_timeout.tv_sec = tracker_get_timeout(tr);
    scrape_timeout.tv_usec = 0;
    while(scrape_timeout.tv_sec > 0) {
        // write
        if (write(tr->socket, scrape_send, send_size) != send_size) {
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
        } else if (response_length < sizeof(struct TRACKER_UDP_SCRAPE_RECEIVE)) {
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

        scrape_timeout.tv_sec -= read_timeout_length;
        if (scrape_timeout.tv_sec == 0) {
            tracker_message_failed(tr);
            throw("scrape timedout :: %s on port %i", tr->host, tr->port);
        }
    }
    scrape_receive->action = net_utils.ntohl(scrape_receive->action);
    scrape_receive->transaction_id = net_utils.ntohl(scrape_receive->transaction_id);

    if (scrape_receive->action == 2) {
        if (scrape_receive->transaction_id == transaction_id) {
            // wait 15 minutes for next scrape
            tr->scrape_deadline = now() + ((15 * 60) * 1000);

            size_t position = sizeof(struct TRACKER_UDP_SCRAPE_RECEIVE);
            size_t stats_size = sizeof(struct TRACKER_UDP_SCRAPE_RECEIVE_TORRENT_STATS);

            struct TRACKER_UDP_SCRAPE_RECEIVE_TORRENT_STATS * stats = (struct TRACKER_UDP_SCRAPE_RECEIVE_TORRENT_STATS *) &raw_response[position];
            int32_t seeders = net_utils.ntohl(stats->seeders);
            int32_t completed = net_utils.ntohl(stats->completed);
            int32_t leechers = net_utils.ntohl(stats->leechers);

            tr->seeders = seeders;
            tr->leechers = leechers;

            log_info("scraped tracker "CYAN"(%"PRId32" seeders) (%"PRId32" leechers) (%"PRId32" completed)"NO_COLOR" :: "GREEN"%s:%i"NO_COLOR, seeders, leechers, completed, tr->host, tr->port);
            tracker_message_succeded(tr);
            tr->status = TRACKER_IDLE;
        }
    }

    if(scrape_send) {
        free(scrape_send);
    }
    return EXIT_SUCCESS;

    error:
    if(scrape_send) {
        free(scrape_send);
    }
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
