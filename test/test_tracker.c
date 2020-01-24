#include "tracker/tracker.h"
#include "peer/peer.h"
#include <string.h>
#include <errno.h>
#include <stdatomic.h>

/* MOCK FUNCTIONS */
#include "mocked_functions.h"
#include "net_utils/net_utils.h"

/* TESTS */

// test tracker initialization and url parsing
static void test_tracker_new(void **state) {
    (void) state;

    RESET_MOCKS();

    char *tracker_url = "udp://von.galixor:6969";

    struct Tracker *tr = NULL;
    tr = tracker_new(tracker_url);
    assert_non_null(tr);

    assert_string_equal(tr->url, tracker_url);
    assert_string_equal(tr->host, "von.galixor");
    assert_int_equal(tr->port, 6969);
    assert_int_equal(tr->status, TRACKER_IDLE);

    tracker_free(tr);
}

// test tracker initialization and url parsing
static void test_tracker_new_strndup_failed(void **state) {
    (void) state;

    RESET_MOCKS();
    USE_WRAPPED_STRNDUP();

    errno = EADDRINUSE;
    will_return(__wrap_strndup, NULL);

    char *tracker_url = "udp://von.galixor:6969";

    struct Tracker *tr = NULL;
    tr = tracker_new(tracker_url);
    assert_null(tr);

    tracker_free(tr);
}


// test tracker should connect
static void test_tracker_should_announce(void **state) {
    (void) state;

    RESET_MOCKS();

    char *tracker_url = "udp://von.galixor:6969";

    struct Tracker *tr = NULL;
    tr = tracker_new(tracker_url);
    assert_non_null(tr);

    // test init value
    assert_int_equal(tracker_should_announce(tr), 1);

    // test all statuses
    tr->status = TRACKER_IDLE;
    assert_int_equal(tracker_should_announce(tr), 1);
    tr->status = TRACKER_CONNECTING;
    assert_int_equal(tracker_should_announce(tr), 0);
    tr->status = TRACKER_CONNECTED;
    assert_int_equal(tracker_should_announce(tr), 0);
    tr->status = TRACKER_ANNOUNCING;
    assert_int_equal(tracker_should_announce(tr), 0);
    tr->status = TRACKER_SCRAPING;
    assert_int_equal(tracker_should_announce(tr), 0);

    tracker_free(tr);
}

// test tracker timeout scaling
static void test_tracker_timeout_scaling(void **state) {
    (void) state;

    RESET_MOCKS();

    char *tracker_url = "udp://von.galixor:6969";

    struct Tracker *tr = NULL;
    tr = tracker_new(tracker_url);
    assert_non_null(tr);

    // test init value
    assert_int_equal(tracker_get_timeout(tr), 60);

    // test timeout scaling with failures
    tracker_message_failed(tr);
    assert_int_equal(tracker_get_timeout(tr), 120);

    tracker_message_failed(tr);
    assert_int_equal(tracker_get_timeout(tr), 240);

    tracker_message_failed(tr);
    assert_int_equal(tracker_get_timeout(tr), 480);

    tracker_message_failed(tr);
    assert_int_equal(tracker_get_timeout(tr), 960);

    tracker_message_failed(tr);
    assert_int_equal(tracker_get_timeout(tr), 1920);

    // on success we should reset timeout
    tracker_message_succeded(tr);
    assert_int_equal(tracker_get_timeout(tr), 60);

    tracker_free(tr);
}

// test tracker connects successfully
static void test_tracker_connect_success(void **state) {
    (void) state;

    RESET_MOCKS();

    char *tracker_url = "udp://von.galixor:6969";

    struct Tracker *tr = NULL;
    tr = tracker_new(tracker_url);
    assert_non_null(tr);

    // set random transaction ID
    long int RANDOM_VALUE = 420;
    will_return(__wrap_random, RANDOM_VALUE);

    struct TRACKER_UDP_CONNECT_RECEIVE connect_response;
    connect_response.action = 0;
    connect_response.transaction_id = net_utils.htonl(RANDOM_VALUE);
    connect_response.connection_id = net_utils.htonll(0x41727101980);

    struct READ_WRITE_MOCK_VALUED r;
    r.value = &connect_response; // read value from here
    r.count = 0; // return provided count, success
    will_return(__wrap_read, &r);

    struct READ_WRITE_MOCK_VALUED w;
    w.value = NULL; // write value to here
    w.count = 0; // return provided count, success
    will_return(__wrap_write, &w);

    _Atomic int cancel_flag = 0;
    tracker_connect(tr, &cancel_flag);

    assert_int_equal(tracker_should_announce(tr), 0);
    assert_int_equal(tr->status, TRACKER_CONNECTED);

    tracker_free(tr);
}

// test tracker fails, got mismatched transaction id
static void test_tracker_connect_fail_incorrect_transaction_id(void **state) {
    (void) state;

    RESET_MOCKS();

    char *tracker_url = "udp://von.galixor:6969";

    struct Tracker *tr = NULL;
    tr = tracker_new(tracker_url);
    assert_non_null(tr);

    // set random transaction ID
    will_return(__wrap_random, 420);
    struct TRACKER_UDP_CONNECT_RECEIVE connect_response;
    connect_response.action = 0;
    connect_response.transaction_id = net_utils.htonl(210);
    connect_response.connection_id = net_utils.htonll(0x41727101980);

    struct READ_WRITE_MOCK_VALUED r;
    r.value = &connect_response; // read value from here
    r.count = 0; // return provided count, success
    will_return(__wrap_read, &r);

    struct READ_WRITE_MOCK_VALUED w;
    w.value = NULL; // write value to here
    w.count = 0; // return provided count, success
    will_return(__wrap_write, &w);

    _Atomic int cancel_flag = 0;
    tracker_connect(tr, &cancel_flag);

    assert_int_equal(tracker_should_announce(tr), 1);
    assert_int_equal(tr->status, TRACKER_IDLE);

    tracker_free(tr);
}

// test tracker failed, got incorrect action back on connect request
static void test_tracker_connect_fail_incorrect_action(void **state) {
    (void) state;

    RESET_MOCKS();

    char *tracker_url = "udp://von.galixor:6969";

    struct Tracker *tr = NULL;
    tr = tracker_new(tracker_url);
    assert_non_null(tr);

    // set random transaction ID
    long int RANDOM_VALUE = 420;

    will_return(__wrap_random, RANDOM_VALUE);

    struct TRACKER_UDP_CONNECT_RECEIVE connect_response;
    connect_response.action = 1;
    connect_response.transaction_id = net_utils.htonl(RANDOM_VALUE);
    connect_response.connection_id = net_utils.htonll(0x41727101980);

    struct READ_WRITE_MOCK_VALUED r;
    r.value = &connect_response; // read value from here
    r.count = 0; // return provided count, success
    will_return(__wrap_read, &r);

    struct READ_WRITE_MOCK_VALUED w;
    w.value = NULL; // write value to here
    w.count = 0; // return provided count, success
    will_return(__wrap_write, &w);

    _Atomic int cancel_flag = 0;
    tracker_connect(tr, &cancel_flag);

    assert_int_equal(tracker_should_announce(tr), 1);
    assert_int_equal(tr->status, TRACKER_IDLE);

    tracker_free(tr);
}

// test tracker fails, failed read
static void test_tracker_connect_failed_read(void **state) {
    (void) state;

    RESET_MOCKS();

    char *tracker_url = "udp://von.galixor:6969";

    struct Tracker *tr = NULL;
    tr = tracker_new(tracker_url);
    assert_non_null(tr);

    // set random transaction ID
    long int RANDOM_VALUE = 420;
    will_return(__wrap_random, RANDOM_VALUE);

    struct TRACKER_UDP_CONNECT_RECEIVE connect_response;
    connect_response.action = 0;
    connect_response.transaction_id = net_utils.htonl(RANDOM_VALUE);
    connect_response.connection_id = net_utils.htonll(0x41727101980);

    struct READ_WRITE_MOCK_VALUED r;
    r.value = &connect_response; // read value from here
    r.count = -1; // incomplete read
    will_return(__wrap_read, &r);

    struct READ_WRITE_MOCK_VALUED w;
    w.value = NULL; // write value to here
    w.count = 0; // return provided count, success
    will_return(__wrap_write, &w);

    _Atomic int cancel_flag = 0;
    tracker_connect(tr, &cancel_flag);

    assert_int_equal(tracker_should_announce(tr), 1);
    assert_int_equal(tr->status, TRACKER_IDLE);

    tracker_free(tr);
}

// test tracker fails, incomplete read
static void test_tracker_connect_failed_read_incomplete(void **state) {
    (void) state;

    RESET_MOCKS();

    char *tracker_url = "udp://von.galixor:6969";

    struct Tracker *tr = NULL;
    tr = tracker_new(tracker_url);
    assert_non_null(tr);

    // set random transaction ID
    long int RANDOM_VALUE = 420;
    will_return(__wrap_random, RANDOM_VALUE);

    struct TRACKER_UDP_CONNECT_RECEIVE connect_response;
    connect_response.action = 0;
    connect_response.transaction_id = net_utils.htonl(RANDOM_VALUE);
    connect_response.connection_id = net_utils.htonll(0x41727101980);

    struct READ_WRITE_MOCK_VALUED r;
    r.value = &connect_response; // read value from here
    r.count = 1; // incomplete read
    will_return(__wrap_read, &r);

    struct READ_WRITE_MOCK_VALUED w;
    w.value = NULL; // write value to here
    w.count = 0; // return provided count, success
    will_return(__wrap_write, &w);

    _Atomic int cancel_flag = 0;
    tracker_connect(tr, &cancel_flag);

    assert_int_equal(tracker_should_announce(tr), 1);
    assert_int_equal(tr->status, TRACKER_IDLE);

    tracker_free(tr);
}

// test tracker announced successfully
static void test_tracker_announce_success(void **state) {
    (void) state;

    RESET_MOCKS();

    char *tracker_url = "udp://von.galixor:6969";

    struct Tracker *tr = NULL;
    tr = tracker_new(tracker_url);
    tr->status = TRACKER_CONNECTED;
    assert_non_null(tr);

    // set random transaction ID
    long int RANDOM_VALUE = 420;
    will_return(__wrap_random, RANDOM_VALUE);

    struct TRACKER_UDP_ANNOUNCE_RECEIVE * announce_response = malloc(sizeof(struct TRACKER_UDP_ANNOUNCE_RECEIVE) + sizeof(struct TRACKER_UDP_ANNOUNCE_RECEIVE_PEER));
    announce_response->action = net_utils.htonl(1);
    announce_response->transaction_id = net_utils.htonl(RANDOM_VALUE);
    announce_response->interval = net_utils.htonl(2000);
    announce_response->leechers = net_utils.htonl(10);
    announce_response->seeders = net_utils.htonl(20);
    struct TRACKER_UDP_ANNOUNCE_RECEIVE_PEER peer = {
            .ip = net_utils.htonl(0xFFFF),
            .port = net_utils.htons(1000)
    };
    memcpy(&announce_response->peers, &peer, sizeof(struct TRACKER_UDP_ANNOUNCE_RECEIVE_PEER));

    struct READ_WRITE_MOCK_VALUED r;
    r.value = announce_response; // read value from here
    r.count = sizeof(struct TRACKER_UDP_ANNOUNCE_RECEIVE) + sizeof(struct TRACKER_UDP_ANNOUNCE_RECEIVE_PEER); // return provided count, success
    will_return(__wrap_read, &r);

    struct READ_WRITE_MOCK_VALUED w;
    w.value = NULL; // write value to here
    w.count = 0; // return provided count, success
    will_return(__wrap_write, &w);

    struct Queue * peer_queue = queue_new();

    _Atomic int cancel_flag = 0;
    int8_t info_hash_hex[20];
    memset(&info_hash_hex, 0, sizeof(info_hash_hex));
    tracker_announce(tr, &cancel_flag, 0, 0, 0, 4900, info_hash_hex, peer_queue);

    assert_int_equal(tracker_should_announce(tr), 0);
    assert_int_equal(tr->status, TRACKER_IDLE);
    assert_int_equal(queue_get_count(peer_queue), 1);

    while(queue_get_count(peer_queue) > 0) {
        struct Peer * p = (struct Peer *) queue_pop(peer_queue);
        peer_free(p);
    }

    queue_free(peer_queue);

    free(announce_response);
    tracker_free(tr);
}


// test tracker announced successfully
static void test_tracker_scrape_success(void **state) {
    (void) state;

    RESET_MOCKS();

    char *tracker_url = "udp://von.galixor:6969";

    struct Tracker *tr = NULL;
    tr = tracker_new(tracker_url);
    tr->status = TRACKER_CONNECTED;
    assert_non_null(tr);

    // set random transaction ID
    long int RANDOM_VALUE = 420;
    will_return(__wrap_random, RANDOM_VALUE);

    struct TRACKER_UDP_SCRAPE_RECEIVE * scrape_response = malloc(sizeof(struct TRACKER_UDP_SCRAPE_RECEIVE) + sizeof(struct TRACKER_UDP_SCRAPE_RECEIVE_TORRENT_STATS));
    scrape_response->action = net_utils.htonl(2);
    scrape_response->transaction_id = net_utils.htonl(RANDOM_VALUE);
    struct TRACKER_UDP_SCRAPE_RECEIVE_TORRENT_STATS stats = {
            .seeders=net_utils.htonl(333),
            .completed=net_utils.htonl(444),
            .leechers=net_utils.htonl(555)
    };
    memcpy(&scrape_response->torrent_stats, &stats, sizeof(struct TRACKER_UDP_SCRAPE_RECEIVE_TORRENT_STATS));

    struct READ_WRITE_MOCK_VALUED r;
    r.value = scrape_response; // read value from here
    r.count = sizeof(struct TRACKER_UDP_SCRAPE_RECEIVE) + sizeof(struct TRACKER_UDP_SCRAPE_RECEIVE_TORRENT_STATS);
    will_return(__wrap_read, &r);

    struct READ_WRITE_MOCK_VALUED w;
    w.value = NULL; // write value to here
    w.count = 0; // return provided count, success
    will_return(__wrap_write, &w);

    _Atomic int cancel_flag = 0;
    int8_t info_hash_hex[20];
    memset(&info_hash_hex, 0, sizeof(info_hash_hex));
    tracker_scrape(tr, &cancel_flag, info_hash_hex);

    assert_int_equal(tracker_should_scrape(tr), 0);
    assert_int_equal(tr->status, TRACKER_IDLE);
    assert_int_equal(tr->seeders, 333);
    assert_int_equal(tr->leechers, 555);

    free(scrape_response);
    tracker_free(tr);
}