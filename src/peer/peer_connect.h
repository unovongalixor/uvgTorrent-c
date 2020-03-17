#ifndef UVGTORRENT_C_PEER_CONNECT_H
#define UVGTORRENT_C_PEER_CONNECT_H

/**
 * @brief PEER WIRE PROTOCOL
 * @see https://wiki.theory.org/index.php/BitTorrentSpecification#Peer_wire_protocol_.28TCP.29
 * @see peer_messages.h for the rest of the protocol
 */
#pragma pack(push, 1)
struct PEER_HANDSHAKE {
    uint8_t pstrlen;
    uint8_t pstr[19];
    uint8_t reserved[8];
    uint8_t info_hash[20];
    uint8_t peer_id[20];
};
#pragma pack(pop)

/**
 * @brief set this peers socket and set status to connected
 * @param p
 * @param socket
 */
extern void peer_set_socket(struct Peer * p, struct BufferedSocket * socket);

/**
 * @brief return true false, this peer is unconnected and ready to establish a connection
 * @param p
 * @return
 */
extern int peer_should_connect(struct Peer * p);

/**
 * @brief set up and connect a socket to this peer
 * @param p
 * @return
 */
extern int peer_connect(struct Peer * p);

/**
 * @brief close the peer socket and set status to unconnected
 * @param p
 * @return
 */
extern void peer_disconnect(struct Peer * p, char * file, int line);

/**
 * @brief return true false, this peer is connected and ready to perform an extended handshake
 * @param p
 * @return
 */
extern int peer_should_send_handshake(struct Peer * p);
extern int peer_should_handle_handshake(struct Peer * p);

/**
 * @brief perform handshake with peer.
 * @note if the peer supports utmetadata extended handshake, this function will also send the extended handshake
 * @param p
 * @param info_hash_hex
 * @return
 */
extern int peer_send_handshake(struct Peer * p, int8_t info_hash_hex[20], _Atomic int * cancel_flag);
extern int peer_handle_handshake(struct Peer * p, int8_t info_hash_hex[20], struct TorrentData * torrent_metadata, _Atomic int * cancel_flag);


#endif //UVGTORRENT_C_PEER_CONNECT_H
