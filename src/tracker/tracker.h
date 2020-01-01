#ifndef UVGTORRENT_C_TRACKER_H
#define UVGTORRENT_C_TRACKER_H

#include <stdint.h>
#include "../thread_pool/thread_pool.h"

enum TrackerStatus {
    TRACKER_UNCONNECTED,
    TRACKER_CONNECTING,
    TRACKER_CONNECTED,
    TRACKER_ANNOUNCING,
    TRACKER_SCRAPING,
    TRACKER_IDLE
};

struct Tracker {
    char *url;
    char *host;
    int port;

    uint64_t connection_id;
    uint32_t interval;
    uint32_t seeders;
    uint32_t leechers;

    int socket;

    enum TrackerStatus status;
    int message_attempts;
};

/**
 * @brief mallocs a new tracker struct and parses the given url
 * @param url
 * @return struct Tracker * on success, NULL on failure
 */
extern struct Tracker *tracker_new(char *url);

/**
 * @brief main tracker loop. handles running the tracker
 * @param cancel_flag
 * @param q
 * @param ...
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
extern int tracker_run(int *cancel_flag, ...);

/**
 * @brief returns 1 if this tracker is in a state to attempt a connection, 0 if not
 * @param tr
 * @return int
 */
extern int tracker_should_connect(struct Tracker *tr);

/**
 * @brief connect a given tracker
 * @param tr
 * @param cancel_flag
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
extern int tracker_connect(struct Tracker *tr, int *cancel_flag);

/**
 * @brief returns 1 if this tracker is in a state to attempt an announce, 0 if not
 * @param tr
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
extern int tracker_should_announce(struct Tracker *tr);

/**
 * @brief announce the given tracker
 * @param tr
 * @param cancel_flag
 * @param downloaded number of bytes already downloaded from this torrent
 * @param left number of bytes left to download from this torrent
 * @param uploaded number of bytes uploaded to other peers
 * @param info_hash torrent info hash
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
extern int tracker_announce(struct Tracker *tr, int *cancel_flag, int64_t downloaded, int64_t left, int64_t uploaded, char * info_hash, struct Queue * peer_queue);


/* TO BE IMPLEMENTED */
extern int tracker_should_scrape(struct Tracker *tr);
extern void tracker_scrape(struct Tracker *tr);

/**
 * @brief get the timeout for this trackers socket related activities
 * @note this conforms to the logic defined here (http://xbtt.sourceforge.net/udp_tracker_protocol.html)
 *          Set n to 0.
 *          If no response is received after 60 * 2 ^ n seconds, resend the connect request and increase n.
 *          If a response is received, reset n to 0.
 * @param tr
 * @return timeout
 */
extern int tracker_get_timeout(struct Tracker *tr);

/**
 * @brief update tracker state after a message fails
 * @note this will reset the tracker to TRACKER_UNCONNECTED so the connection will be reestablished,
 *       clear the socket if required, and increment the attempts
 *       counter in order to increase the trackers timeout
 * @param tr
 */
extern void tracker_message_failed(struct Tracker *tr);

/**
 * @brief update tracker state after a message succeeds.
 * @note this will reset the attempts counter in order to reset the trackers timeout to the initial value
 * @param tr
 */
extern void tracker_message_succeded(struct Tracker *tr);

/**
 * @brief free the given tracker
 * @param tr
 * @return tr after freeing, NULL on success
 */
extern struct Tracker *tracker_free(struct Tracker *tr);

/**
 * @brief UDP TRACKER PROTOCOL
 * @see https://www.rasterbar.com/products/libtorrent/udp_tracker_protocol.html
 */
#pragma pack(push, 1)
struct TRACKER_UDP_CONNECT_SEND {
    int64_t connection_id;  /* Must be initialized to 0x41727101980 in network byte order. This will identify the protocol. */
    int32_t action;         /* 0 for a connection request                                                                   */
    int32_t transaction_id; /* Randomized by client.                                                                        */
};

struct TRACKER_UDP_CONNECT_RECEIVE {
    int32_t action;         /* Describes the type of packet, in this case it should be 0, for connect. If 3 (for error) see errors.   */
    int32_t transaction_id; /* Must match the transaction_id sent from the client.                                                    */
    int64_t connection_id;  /* A connection id, this is used when further information is exchanged with the tracker, to identify you. */
    /* This connection id can be reused for multiple requests, but if it's cached for too long,               */
    /*  it will not be valid anymore.                                                                         */
};

struct TRACKER_UDP_ANNOUNCE_SEND {
    int64_t connection_id;  /* The connection id acquired from establishing the connection.                             */
    int32_t action;         /*	Action. in this case, 1 for announce. See actions.                                      */
    int32_t transaction_id; /*	Randomized by client.                                                                   */
    int8_t info_hash[20];  /*	The info-hash of the torrent you want announce yourself in.                             */
    int8_t peer_id[20];    /*	Your peer id.                                                                           */
    int64_t downloaded;     /*	The number of byte you've downloaded in this session.                                   */
    int64_t left;           /*	The number of bytes you have left to download until you're finished.                    */
    int64_t uploaded;       /*	The number of bytes you have uploaded in this session.                                  */
    int32_t event;          /* The event, one of:                                                                       */
    /* none = 0                                                                                 */
    /* completed = 1                                                                            */
    /* started = 2                                                                              */
    /* stopped = 3                                                                              */
    uint32_t ip;             /*	Your ip address. Set to 0 if you want the tracker to use the sender of this UDP packet. */
    uint32_t key;              /* A unique key that is randomized by the client.                                           */
    int32_t num_want;       /*	The maximum number of peers you want in the reply. Use -1 for default.                  */
    uint16_t port;              /* The port you're listening on.                                                            */
    uint16_t extensions;     /* see extensions                                                                           */
};

struct TRACKER_UDP_ANNOUNCE_RECEIVE_PEER {
    int32_t ip;        /* The ip of a peer in the swarm. */
    uint16_t port;      /* The peer's listen port.        */
};
struct TRACKER_UDP_ANNOUNCE_RECEIVE {
    int32_t action;         /*	The action this is a reply to. Should in this case be 1 for announce. If 3 (for error) see errors. See actions.   */
    int32_t transaction_id;    /*  Must match the transaction_id sent in the announce request.                                                       */
    int32_t interval;          /*  the number of seconds you should wait until re-announcing yourself.                                               */
    int32_t leechers;       /*  The number of peers in the swarm that has not finished downloading.                                               */
    int32_t seeders;        /* The number of peers in the swarm that has finished downloading and are seeding.                                    */
    struct TRACKER_UDP_ANNOUNCE_RECEIVE_PEER peers[]; /* peers with this torrent                                                                  */
};

struct TRACKER_UDP_SCRAPE_SEND_INFO_HASH {
    int8_t info_hash[20]; /*	info_hash	The info hash that is to be scraped. */
};
struct TRACKER_UDP_SCRAPE_SEND {
    int64_t connection_id;    /*	The connection id retrieved from the establishing of the connection.  */
    int32_t action;           /*	The action, in this case, 2 for scrape. See actions.                  */
    int32_t transaction_id;    /*  Randomized by client.                                                 */
    struct TRACKER_UDP_SCRAPE_SEND_INFO_HASH info_hashes[];  /* info hashes of torrent to scrape        */
};

struct TRACKER_UDP_SCRAPE_RECEIVE_TORRENT_STATS {
    int32_t complete;       /* The current number of connected seeds.                 */
    int32_t downloaded;     /* The number of times this torrent has been downloaded.  */
    int32_t incomplete;     /* The current number of connected leechers.              */
};
struct TRACKER_UDP_SCRAPE_RECEIVE {
    int32_t action;         /*	The action, should in this case be 2 for scrape. If 3 (for error) see errors.  */
    int32_t transaction_id; /*	Must match the sent transaction id.                                            */
    struct TRACKER_UDP_SCRAPE_RECEIVE_TORRENT_STATS torrent_stats[]; /* status for torrents we scraped         */
};

struct TRACKER_UDP_ERROR {
    int32_t action;         /*	The action, in this case 3, for error. See actions.      */
    int32_t transaction_id; /*	Must match the transaction_id sent from the client.      */
    int8_t error_string[];   /*	The rest of the packet is a string describing the error. */
};
#pragma pack(pop)

#endif //UVGTORRENT_C_TRACKER_H
