#ifndef UVGTORRENT_C_PEER_UT_METADATA_H
#define UVGTORRENT_C_PEER_UT_METADATA_H

/**
 * @brief returns 1 if the peer supports ut_metadata, 0 if not
 * @param p
 * @return
 */
extern int peer_supports_ut_metadata(struct Peer * p);
extern int peer_handle_ut_metadata_handshake(struct Peer * p, void * msg_buffer);
extern int peer_handle_ut_metadata_request(struct Peer * p, uint64_t chunk_id, struct TorrentData * torrent_metadata);
extern int peer_handle_ut_metadata_data(struct Peer * p, void * msg_buffer, struct Queue * metadata_queue);
extern int peer_handle_ut_metadata_reject(struct Peer * p);

/**
 * @brief should this peer attempt to claim and request a chunk of metadata?
 * @param p
 * @return
 */
extern int peer_should_send_ut_metadata_request(struct Peer * p, struct TorrentData * torrent_metadata);

/**
 * @brief send a piece request. the piece to be requested should have been claimed using peer_claim_resource
 *        and the piece to be requested should be stored in p->claimed_bitfield_resource_bit
 * @param piece_num
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
extern int peer_send_ut_metadata_request(struct Peer * p, struct TorrentData * torrent_metadata);

#endif //UVGTORRENT_C_PEER_UT_METADATA_H
