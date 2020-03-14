#include "../macros.h"
#include "peer.h"
#include "../net_utils/net_utils.h"
#include "../bencode/bencode.h"


void peer_set_socket(struct Peer *p, struct BufferedSocket * socket) {
    p->socket = socket;
    p->status = PEER_CONNECTED;
}

int peer_should_connect(struct Peer *p) {
    return (p->status == PEER_UNCONNECTED);
}

int peer_connect(struct Peer *p) {
    p->status = PEER_CONNECTING;

    p->socket = buffered_socket_new((struct sockaddr *) &p->addr);

    if(buffered_socket_connect(p->socket) == -1) {
        goto error;
    }

    p->status = PEER_CONNECTED;
    return EXIT_SUCCESS;

    error:

    p->status = PEER_UNCONNECTED;
    buffered_socket_free(p->socket);
    return EXIT_FAILURE;
}

void peer_disconnect(struct Peer *p) {
    if (p->socket != NULL) {
        buffered_socket_free(p->socket);
        p->socket = NULL;
    }

    if(p->status >= PEER_HANDSHAKE_COMPLETE) {
        log_err(RED"peer disconnected :: %s:%i"NO_COLOR, p->str_ip, p->port);
        p->status = PEER_UNCONNECTED;
    } else {
        p->status = PEER_UNCONNECTED;
    }
}

int peer_should_send_handshake(struct Peer *p) {
    return (p->status == PEER_CONNECTED) & (buffered_socket_can_write(p->socket) == 1);
}

int peer_should_handle_handshake(struct Peer *p) {
    return (p->status == PEER_HANDSHAKE_SENT) & (buffered_socket_can_read(p->socket) == 1);
}

int peer_send_handshake(struct Peer *p, int8_t info_hash_hex[20], _Atomic int *cancel_flag) {
    /* send handshake */
    struct PEER_HANDSHAKE handshake_send;
    handshake_send.pstrlen = 19;
    char *pstr = "BitTorrent protocol";
    memcpy(&handshake_send.pstr, pstr, sizeof(handshake_send.pstr));
    memset(&handshake_send.reserved, 0x00, sizeof(handshake_send.reserved));
    handshake_send.reserved[5] = 0x10; // send reserved byte to signal availability of utmetadata
    memcpy(&handshake_send.info_hash, info_hash_hex, sizeof(int8_t[20]));
    char *peer_id = "UVG11234567891234567";
    memcpy(&handshake_send.peer_id, peer_id, sizeof(handshake_send.peer_id));

    if (buffered_socket_write(p->socket, &handshake_send, sizeof(struct PEER_HANDSHAKE)) != sizeof(struct PEER_HANDSHAKE)) {
        goto error;
    }

    p->status = PEER_HANDSHAKE_SENT;

    return EXIT_SUCCESS;

    error:
    return EXIT_FAILURE;
}

int peer_handle_handshake(struct Peer *p, int8_t *info_hash_hex, struct TorrentData * torrent_metadata, _Atomic int *cancel_flag) {
    /* receive handshake */
    struct PEER_HANDSHAKE handshake_receive;
    memset(&handshake_receive, 0x00, sizeof(handshake_receive));

    if (buffered_socket_read(p->socket, &handshake_receive, sizeof(handshake_receive)) != sizeof(struct PEER_HANDSHAKE)) {
        goto error;
    }

    /* compare infohash and check for metadata support */
    for (int i = 0; i < 8; i++) {
        if (handshake_receive.info_hash[i] != (uint8_t) info_hash_hex[i]) {
            throw("mismatched infohash");
        }
    }

    /* if metadata supported, do extended handshake */
    if (handshake_receive.reserved[5] == 0x10) {
        // include metadata size here if we have that information available
        be_node_t *d = be_alloc(DICT);
        be_node_t *m = be_alloc(DICT);
        be_dict_add_num(m, "ut_metadata", UT_METADATA_ID);
        be_dict_add(d, "m", m);
        be_dict_add_num(d, "metadata_size", (int) torrent_metadata->data_size);

        char extended_handshake_message[1000] = {'\0'};
        size_t extended_handshake_message_len = be_encode(d, (char *) &extended_handshake_message, 1000);
        be_free(d);

        size_t extensions_send_size = sizeof(struct PEER_MSG_EXTENSION) + extended_handshake_message_len;
        struct PEER_MSG_EXTENSION *extension_send = malloc(extensions_send_size);
        extension_send->length = net_utils.htonl(extensions_send_size - sizeof(int32_t));
        extension_send->msg_id = 20;
        extension_send->extended_msg_id = 0; // extended handshake id
        memcpy(&extension_send->msg, &extended_handshake_message, extended_handshake_message_len);

        if (buffered_socket_write(p->socket, extension_send, extensions_send_size) != extensions_send_size) {
            free(extension_send);
            goto error;
        } else {
            free(extension_send);
        }
    }

    p->status = PEER_HANDSHAKE_COMPLETE;
    log_info(GREEN"peer handshaked :: %s:%i"NO_COLOR, p->str_ip, p->port);

    return EXIT_SUCCESS;
    error:
    return EXIT_FAILURE;
}
