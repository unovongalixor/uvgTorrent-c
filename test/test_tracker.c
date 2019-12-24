#include "tracker/tracker.h"
#include <string.h>
#include <errno.h>

/* MOCK FUNCTIONS */
#include "mocked_functions.c"

/* TESTS */

// test tracker initialization and url parsing
static void test_tracker_new(void **state) {
    (void) state;

    reset_mocks();

    char * tracker_url = "udp://exodus.desync.com:6969";

    struct Tracker * tr = NULL;
    tr = tracker_new(tracker_url);
    assert_non_null(tr);

    assert_string_equal(tr->url, tracker_url);
    assert_string_equal(tr->host, "exodus.desync.com");
    assert_int_equal(tr->port, 6969);
    assert_int_equal(tracker_get_status(tr), TRACKER_UNCONNECTED);

    tracker_free(tr);
}

// test tracker should connect
static void test_tracker_should_connect(void **state) {
    (void) state;

    reset_mocks();

    char * tracker_url = "udp://exodus.desync.com:6969";

    struct Tracker * tr = NULL;
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

    reset_mocks();

    char * tracker_url = "udp://exodus.desync.com:6969";

    struct Tracker * tr = NULL;
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