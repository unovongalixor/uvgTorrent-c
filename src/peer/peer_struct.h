//
// Created by vongalixor on 3/12/20.
//

#ifndef UVGTORRENT_C_PEER_STRUCT_H
#define UVGTORRENT_C_PEER_STRUCT_H

#include <inttypes.h>
#include <netinet/ip.h>
#include <stdint.h>
#include "../thread_pool/queue.h"
#include "../torrent/torrent_data.h"
#include "../buffered_socket/buffered_socket.h"

#define METADATA_PIECE_SIZE 262144
#define METADATA_CHUNK_SIZE 16384
#define UT_METADATA_ID 3

enum PeerStatus {
    PEER_UNAVAILABLE,
    PEER_UNCONNECTED,
    PEER_CONNECTING,
    PEER_CONNECTED,
    PEER_HANDSHAKE_SENT,
    PEER_HANDSHAKE_COMPLETE
};

struct Peer {
    struct sockaddr_in addr;
    char * str_ip;
    int32_t ip;
    uint16_t port;
    struct BufferedSocket * socket;
    int utmetadata;
    int metadata_size;

    int am_initiating;
    int am_choking;
    int am_interested;
    int peer_choking;
    int peer_interested;

    enum PeerStatus status;
    int running;

    /* msg reading stuff */
    // keeps the state of the current message being received so the peer can handle partial reads
    uint32_t network_ordered_msg_length;
    uint8_t msg_id;
};

#endif //UVGTORRENT_C_PEER_STRUCT_H
