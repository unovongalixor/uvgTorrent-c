#include "tracker/tracker.h"
#include <string.h>
#include <errno.h>

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
    assert_int_equal(tracker_get_status(tr), TRACKER_UNCONNECTED);

    tracker_free(tr);
}

// test tracker should connect
static void test_tracker_should_connect(void **state) {
    (void) state;

    RESET_MOCKS();

    char *tracker_url = "udp://von.galixor:6969";

    struct Tracker *tr = NULL;
    tr = tracker_new(tracker_url);
    assert_non_null(tr);

    // test init value
    assert_int_equal(tracker_should_connect(tr), 1);

    // test all statuses
    tracker_set_status(tr, TRACKER_UNCONNECTED);
    assert_int_equal(tracker_should_connect(tr), 1);
    tracker_set_status(tr, TRACKER_CONNECTING);
    assert_int_equal(tracker_should_connect(tr), 0);
    tracker_set_status(tr, TRACKER_CONNECTED);
    assert_int_equal(tracker_should_connect(tr), 0);
    tracker_set_status(tr, TRACKER_ANNOUNCING);
    assert_int_equal(tracker_should_connect(tr), 0);
    tracker_set_status(tr, TRACKER_SCRAPING);
    assert_int_equal(tracker_should_connect(tr), 0);

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

// test tracker should connect
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
    r.count = -1;
    will_return(__wrap_read, &r);

    struct READ_WRITE_MOCK_VALUED w;
    w.value = NULL; // write value to here
    w.count = -1;
    will_return(__wrap_write, &w);

    int cancel_flag = 0;
    tracker_connect(&cancel_flag, NULL, tr);

    assert_int_equal(tracker_should_connect(tr), 0);
    assert_int_equal(tracker_get_status(tr), TRACKER_CONNECTED);

    tracker_free(tr);
}

// test tracker should connect
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
    r.count = -1;
    will_return(__wrap_read, &r);

    struct READ_WRITE_MOCK_VALUED w;
    w.value = NULL; // write value to here
    w.count = -1;
    will_return(__wrap_write, &w);


    int cancel_flag = 0;
    tracker_connect(&cancel_flag, NULL, tr);

    assert_int_equal(tracker_should_connect(tr), 1);
    assert_int_equal(tracker_get_status(tr), TRACKER_UNCONNECTED);

    tracker_free(tr);
}

// test tracker should connect
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
    r.count = -1;
    will_return(__wrap_read, &r);

    struct READ_WRITE_MOCK_VALUED w;
    w.value = NULL; // write value to here
    w.count = -1;
    will_return(__wrap_write, &w);


    int cancel_flag = 0;
    tracker_connect(&cancel_flag, NULL, tr);

    assert_int_equal(tracker_should_connect(tr), 1);
    assert_int_equal(tracker_get_status(tr), TRACKER_UNCONNECTED);

    tracker_free(tr);
}