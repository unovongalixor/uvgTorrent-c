//
// Created by vongalixor on 2019-12-10.
//

#include "torrent.h"
#include "../tracker/tracker.h"
#include "../yuarel/yuarel.h"
#include "../macros.h"
#include "../thread_pool/thread_pool.h"
#include "../hash_map/hash_map.h"
#include "../bitfield/bitfield.h"
#include "../peer/peer.h"
#include "../net_utils/net_utils.h"
#include "../bencode/bencode.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdatomic.h>

#define POLL_ERR         (-1)
#define POLL_EXPIRE      (0)

/* private functions */
static int torrent_parse_magnet_uri(struct Torrent *t) {
    struct yuarel url;

    char prefix[] = "http://blank.com";
    char ignore[] = "magnet:";

    if (strncmp(ignore, t->magnet_uri, strlen(ignore)) != 0) {
        return EXIT_FAILURE;
    }

    int prefix_size = (sizeof(prefix) / sizeof(prefix[0]) - 1);
    int ignore_size = (sizeof(ignore) / sizeof(ignore[0]) - 1);

    char url_string[sizeof(prefix) + strlen(t->magnet_uri) + 1];

    strncpy(url_string, prefix, sizeof(prefix));
    strncpy(url_string + (prefix_size), t->magnet_uri + (ignore_size), strlen(t->magnet_uri) + 1);

    if (-1 == yuarel_parse(&url, url_string)) {
        return EXIT_FAILURE;
    }

    {
        int p;
        struct yuarel_param params[10];

        p = yuarel_parse_query(url.query, '&', params, 10);
        while (p-- > 0) {

            if (strcmp(params[p].key, "dn") == 0) {
                t->name = strndup(params[p].val, strlen(params[p].val));
                if (!t->name) {
                    throw("failed to set torrent name");
                }

            } else if (strcmp(params[p].key, "xt") == 0) {
                t->info_hash = strndup(params[p].val, strlen(params[p].val));
                if (!t->info_hash) {
                    throw("failed to set torrent info_hash");
                }

                char * trimmed_info_hash = strrchr(t->info_hash, ':') + 1;
                int pos = 0;
                /* hex string to int8_t array */
                for(int count = 0; count < sizeof(t->info_hash_hex); count++) {
                    sscanf(trimmed_info_hash + pos, "%2hhx", &t->info_hash_hex[count]);
                    pos += 2 * sizeof(char);
                }

            } else if (strcmp(params[p].key, "tr") == 0) {
                if (torrent_add_tracker(t, params[p].val) == EXIT_FAILURE) {
                    throw("failed to add tracker");
                }
            }
        }
    }

    return EXIT_SUCCESS;
    error:
    return EXIT_FAILURE;
}

/* public functions */
struct Torrent *torrent_new(char *magnet_uri, char *path, int port) {
    struct Torrent *t = NULL;

    t = malloc(sizeof(struct Torrent));
    if (!t) {
        throw("Torrent failed to malloc")
    }
    /* zero out variables */
    t->magnet_uri = NULL;
    t->path = NULL;
    t->name = NULL;
    t->info_hash = NULL;

    t->port = port;
    t->tracker_count = 0;

    t->torrent_metadata = torrent_data_new();
    t->torrent_metadata->needed = 1;

    t->torrent_data = torrent_data_new();
    t->torrent_data->needed = 0;

    t->files = NULL;

    memset(t->trackers, 0, sizeof t->trackers);
    t->peers = NULL;
    t->peer_ips = NULL;

    /* set variables */
    t->magnet_uri = strndup(magnet_uri, strlen(magnet_uri));
    if (!t->magnet_uri) {
        throw("torrent failed to set magnet_uri");
    }

    t->path = strndup(path, strlen(path));
    if (!t->path) {
        throw("torrent failed to set path");
    }

    /* try to parse given magnet uri */
    if (torrent_parse_magnet_uri(t) == EXIT_FAILURE) {
        throw("torrent failed to parse magnet_uri");
    }

    log_info("preparing to download torrent :: %s", t->name);
    log_info("torrent info_hash :: %s", t->info_hash);
    log_info("saving torrent to path :: %s", t->path);
    log_info("listening for peers on port :: %i", t->port);

    for (int i = 0; i < t->tracker_count; i++) {
        struct Tracker *tr = t->trackers[i];
        log_info("tracker :: %s", tr->url);
    }

    t->peers = hashmap_new(500);
    if (!t->peers) {
        throw("torrent failed to init peers hashmap");
    }

    return t;
    error:
    torrent_free(t);

    return NULL;
}

int torrent_add_tracker(struct Torrent *t, char *url) {
    if (t->tracker_count < MAX_TRACKERS) {
        struct Tracker *tr = tracker_new(url);
        if (!tr) {
            throw("tracker failed to init");
        }

        t->trackers[t->tracker_count] = tr;
        t->tracker_count++;

    } else {
        log_warn("tracker being ignored :: %s", url);
    }
    return EXIT_SUCCESS;
    error:
    return EXIT_FAILURE;
}

int torrent_run_trackers(struct Torrent *t, struct ThreadPool *tp, struct Queue * peer_queue) {
    struct Job *j = NULL;
    for (int i = 0; i < t->tracker_count; i++) {
        struct Tracker *tr = t->trackers[i];
        if(tracker_should_run(tr) == 1) {
            tr->running = 1;
            struct JobArg args[5] = {
                    {
                            .arg = (void *) tr,
                            .mutex = NULL
                    },
                    {
                            .arg = (void *) t->torrent_data,
                            .mutex = NULL
                    },
                    {
                            .arg = (void *) &t->port,
                            .mutex =  NULL
                    },
                    {
                            .arg = (void *) &t->info_hash_hex,
                            .mutex =  NULL
                    },
                    {
                            .arg = (void *) peer_queue,
                            .mutex =  NULL
                    }
            };
            j = job_new(
                    &tracker_run,
                    sizeof(args) / sizeof(struct JobArg),
                    args
            );
            if (!j) {
                throw("job failed to init");
            }

            if(thread_pool_add_job(tp, j) == EXIT_FAILURE) {
                throw("failed to add job to thread pool");
            }
        }
    }
    return EXIT_SUCCESS;

    error:
    if (j != NULL) {
        job_free(j);
    }
    return EXIT_FAILURE;
}

int torrent_run_peers(struct Torrent *t, struct ThreadPool *tp, struct Queue * metadata_queue) {
    struct PeerIp * peer_ip = t->peer_ips;
    while(peer_ip != NULL) {
        struct Peer * p = (struct Peer *) hashmap_get(t->peers, peer_ip->str_ip);
        hashmap_set(t->peers, p->str_ip, p);

        if (peer_should_run(p, (struct TorrentData **) &t->torrent_metadata)) {
            p->running = 1;
            struct JobArg args[4] = {
                    {
                            .arg = (void *) p,
                            .mutex = NULL
                    },
                    {
                            .arg = (void *) &t->info_hash_hex,
                            .mutex =  NULL
                    },
                    {
                            .arg = (void *) &t->torrent_metadata,
                            .mutex =  NULL
                    },
                    {
                            .arg = (void *) metadata_queue,
                            .mutex =  NULL
                    }
            };
            struct Job * j = job_new(
                    &peer_run,
                    sizeof(args) / sizeof(struct JobArg),
                    args
            );
            if (!j) {
                return EXIT_FAILURE;
            }
            if(thread_pool_add_job(tp, j) == EXIT_FAILURE) {
                return EXIT_FAILURE;
            }
        }
        peer_ip = peer_ip->next;
    }

    return EXIT_SUCCESS;
}

int torrent_add_peer(struct Torrent *t, struct ThreadPool *tp, struct Peer * p) {
    if (hashmap_has_key(t->peers, p->str_ip) == 0) {
        hashmap_set(t->peers, p->str_ip, p);

        // store the peers key in a linked list
        struct PeerIp ** peer_ip = &t->peer_ips;
        while(*peer_ip != NULL) {
            peer_ip = &(*peer_ip)->next;
        }

        *peer_ip = malloc(sizeof(struct PeerIp));
        if (!*peer_ip) {
            throw("failed to add peer_ip to linked list");
        }
        (*peer_ip)->str_ip = p->str_ip;
        (*peer_ip)->next = NULL;

        return EXIT_SUCCESS;
    } else {
        peer_free(p);
    }

    return EXIT_SUCCESS;
    error:

    return EXIT_FAILURE;
}

int torrent_listen_for_peers(_Atomic int * cancel_flag, ...) {
    va_list args;
    va_start(args, cancel_flag);

    struct JobArg t_job_arg = va_arg(args, struct JobArg);
    struct Torrent *t = (struct Torrent *) t_job_arg.arg;

    struct JobArg peer_queue_job_arg = va_arg(args, struct JobArg);
    struct Queue * peer_queue = (struct Queue *) peer_queue_job_arg.arg;

    // listen on t->port and dump peer objects in peer_queue
    int sockfd;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1) {
        throw("unable to listen for peers, socket failed to create");
    }

    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        throw("unable to listen for peers, SO_REUSEADDR failed");
    }

    struct sockaddr_in servaddr = {
            .sin_family = AF_INET,
            .sin_addr.s_addr = net_utils.htonl(INADDR_ANY),
            .sin_port = net_utils.htons(t->port)
    };
    if ((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) {
        throw("unable to listen for peers, socket failed to bind socket");
    }

    if ((listen(sockfd, 8)) != 0) {
        throw("unable to listen for peers, failed to listen");
    }

    int num_fd = 1;
    struct pollfd poll_set[1];
    memset(poll_set, '\0', sizeof(poll_set));
    poll_set[0].fd = sockfd;
    poll_set[0].events = POLLIN;

    while (*cancel_flag != 1) {
        int result = poll(poll_set, 1, 1);
        switch (result) {
            case POLL_EXPIRE:
                break;
            case POLL_ERR:
                throw("unable to listen for peers, error on poll");
            default: {
                socklen_t len = sizeof(struct sockaddr_in);
                struct sockaddr_in addr;
                int peer_socket = accept(sockfd, (struct sockaddr *)&addr, &len);

                struct Peer * p = peer_new((int32_t) addr.sin_addr.s_addr, (uint16_t) addr.sin_port);
                struct BufferedSocket * socket = buffered_socket_new((struct sockaddr *) &p->addr);
                buffered_socket_set_socket_fd(socket, peer_socket);

                peer_set_socket(p, socket);

                queue_push(peer_queue, (void *) p);
                break;
            }
        }
    }

    if (sockfd > 0) {
        close(sockfd);
    }
    return EXIT_SUCCESS;
error:
    if (sockfd > 0) {
        close(sockfd);
    }
    return EXIT_FAILURE;
}

int torrent_process_metadata_piece(struct Torrent * t, struct PEER_EXTENSION * metadata_msg) {
    uint32_t msg_length;
    size_t buffer_size;
    get_msg_length((void *)metadata_msg, (uint32_t * ) & msg_length);
    get_msg_buffer_size((void *)metadata_msg, (size_t * ) & buffer_size);
    size_t extenstion_msg_len = (buffer_size) - sizeof(struct PEER_EXTENSION);
    size_t msg_size = 0;

    be_node_t * msg = be_decode((char *) &metadata_msg->msg, extenstion_msg_len, &msg_size);
    if (msg == NULL) {
        log_err("failed to decode metadata msg");
        be_free(msg);
        return EXIT_FAILURE;
    }
    uint64_t piece = (uint64_t) be_dict_lookup_num(msg, "piece");
    be_free(msg);

    if(torrent_data_write_chunk(t->torrent_metadata, piece, &metadata_msg->msg[msg_size], extenstion_msg_len - msg_size) == EXIT_SUCCESS) {
        int valid = 1;
        for(int i = 0; i < t->torrent_metadata->completed->bytes_count; i++) {
            if(t->torrent_metadata->completed->bytes[i] != 0xFF) {
                valid = 0;
                break;
            }
        }
        if (valid == 1) {
            size_t metadata_read_size = 0;
            be_node_t * info = be_decode((char *) t->torrent_metadata->data, t->torrent_metadata->data_size, &metadata_read_size);
            if (info == NULL) {
                be_free(info);
                // clear completed and claimed bitfields to try downloading again
                throw("failed to decode metadata");
            }

            char * name = be_dict_lookup_cstr(info, "name");
            uint64_t piece_length = be_dict_lookup_num(info, "piece length");
            uint64_t torrent_length = 0;

            be_node_t * files = be_dict_lookup(info, "files", NULL);
            if(files == NULL) {
                // single file torrent
                log_info("got single file torrent");
                log_info("name :: %s", name);
                log_info("piece length :: %"PRId64, piece_length);
                log_info("file path :: %s", name);
                torrent_length = be_dict_lookup_num(info, "length");
                log_info("torrent length :: %"PRId64, torrent_length);
            } else {
                // multiple files torrent
                log_info("got a multiple file torrent");
                log_info("name :: %s", name);
                log_info("piece length :: %"PRId64, piece_length);

                list_t *l, *tmp;
                list_for_each_safe(l, tmp, &files->x.list_head) {
                    be_node_t *file = list_entry(l, be_node_t, link);

                    char file_path[4096]; // 4096 unix max path size
                    size_t remaining_file_path_buffer = sizeof(file_path);
                    memset(&file_path, 0x00, sizeof(file_path));
                    be_node_t * path = be_dict_lookup(file, "path", NULL);
                    list_t *path_l, *path_tmp;
                    list_for_each_safe(path_l, path_tmp, &path->x.list_head) {
                        be_node_t * path = list_entry(path_l, be_node_t, link);
                        if(remaining_file_path_buffer < path->x.str.len) {
                            be_free(path);
                            throw("failed to parse filename, too long");
                        }
                        strncat((char *) &file_path, path->x.str.buf, path->x.str.len);
                        remaining_file_path_buffer -= path->x.str.len;
                        be_free(path);
                    }

                    uint64_t file_length = be_dict_lookup_num(file, "length");
                    log_info("file path :: %s", (char *) &file_path);
                    // log_info("file length :: %"PRId64, file_length);
                    torrent_length += file_length;

                    be_free(file);
                }

                log_info("torrent length :: %"PRId64, torrent_length);
            }

            be_free(info);
        }
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
    error:
    return EXIT_FAILURE;
}

struct Torrent *torrent_free(struct Torrent *t) {
    if (t) {
        if (t->magnet_uri) {
            free(t->magnet_uri);
            t->magnet_uri = NULL;
        }
        if (t->path) {
            free(t->path);
            t->path = NULL;
        }
        if (t->name) {
            free(t->name);
            t->name = NULL;
        }
        if (t->info_hash) {
            free(t->info_hash);
            t->info_hash = NULL;
        }

        for (int i = 0; i < t->tracker_count; i++) {
            struct Tracker *tr = t->trackers[i];
            if (tr) {
                tracker_free(tr);
                tr = NULL;
            }
        }

        if (t->peers != NULL) {
            struct Peer * p = hashmap_empty(t->peers);
            while (p != NULL) {
                peer_free(p);
                p = hashmap_empty(t->peers);
            }

            hashmap_free(t->peers);
        }

        if(t->peer_ips != NULL) {
            struct PeerIp * peer_ip = t->peer_ips;
            struct PeerIp * next_peer_ip = NULL;
            while(peer_ip != NULL) {
                next_peer_ip = peer_ip->next;
                free(peer_ip);
                peer_ip = next_peer_ip;
            }
        }

        if (t->torrent_metadata != NULL) {
            t->torrent_metadata = torrent_data_free(t->torrent_metadata);
        }

        if (t->torrent_data != NULL) {
            t->torrent_data = torrent_data_free(t->torrent_data);
        }

        if(t->files != NULL) {
            free(t->files);
            t->files = NULL;
        }

        free(t);
        t = NULL;
    }

    return t;
}
