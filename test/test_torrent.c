#include "torrent/torrent.h"
#include "thread_pool/thread_pool.h"
#include <string.h>
#include <errno.h>

/* MOCK FUNCTIONS */
#include "mocked_functions.h"

/* TESTS */
static void test_magnet_uri_parse_success(void **state) {
    (void) state;

    RESET_MOCKS();

    char *magnet_uri = "magnet:?xt=urn:btih:3a6b29a9225a2ffb6e98ccfa1315cc254968b672&dn=Rick+and+Morty+S03E01+"
                       "720p+HDTV+HEVC+x265-iSm&tr=udp%3A%2F%2Ftracker.leechers-paradise.org%3A6969&tr=udp%3A%"
                       "2F%2Fzer0day.ch%3A1337&tr=udp%3A%2F%2Fopen.demonii.com%3A1337&tr=udp%3A%2F%2Ftracker.coppersurfer.tk%3A6969"
                       "&tr=udp%3A%2F%2Fexodus.desync.com%3A6969";
    char *path = "/tmp";

    struct Torrent *t = NULL;
    t = torrent_new(magnet_uri, path);
    assert_non_null(t);

    assert_string_equal(t->magnet_uri, magnet_uri);
    assert_string_equal(t->path, path);
    assert_string_equal(t->name, "Rick+and+Morty+S03E01+720p+HDTV+HEVC+x265-iSm");
    assert_string_equal(t->info_hash, "urn:btih:3a6b29a9225a2ffb6e98ccfa1315cc254968b672");

    torrent_free(t);
}

static void test_run_trackers_success(void **state) {
    (void) state;

    RESET_MOCKS();

    char *magnet_uri = "magnet:?xt=urn:btih:3a6b29a9225a2ffb6e98ccfa1315cc254968b672&dn=Rick+and+Morty+S03E01+"
                       "720p+HDTV+HEVC+x265-iSm&tr=udp%3A%2F%2Ftracker.leechers-paradise.org%3A6969&tr=udp%3A%"
                       "2F%2Fzer0day.ch%3A1337&tr=udp%3A%2F%2Fopen.demonii.com%3A1337&tr=udp%3A%2F%2Ftracker.coppersurfer.tk%3A6969"
                       "&tr=udp%3A%2F%2Fexodus.desync.com%3A6969";
    char *path = "/tmp";

    struct Torrent *t = NULL;
    t = torrent_new(magnet_uri, path);
    assert_non_null(t);

    struct ThreadPool *tp = thread_pool_new(1000);
    assert_non_null(tp);

    /* start running trackers in separate threads */
    torrent_run_trackers(t, tp, NULL);

    /* check that 4 tracker run jobs were added to thread pool */
    assert_int_equal(queue_get_count(tp->job_queue), 4);

    thread_pool_free(tp);
    torrent_free(t);
}

/* test that initializing torrent fails properly with an invalid magnet uri */
static void test_invalid_magnet_uri(void **state) {
    (void) state;

    RESET_MOCKS();

    char *magnet_uri = "this_is_not_a_magnet_uri";
    char *path = "/tmp";

    struct Torrent *t = NULL;
    t = torrent_new(magnet_uri, path);
    assert_null(t);
    torrent_free(t);
}

/* test that initializing torrent fails properly when allocations fail */
static void test_torrent_strndup_failed(void **state) {
    (void) state;

    RESET_MOCKS();
    USE_WRAPPED_STRNDUP();

    errno = EADDRINUSE;
    will_return(__wrap_strndup, NULL);

    char *magnet_uri = "magnet:?xt=urn:btih:3a6b29a9225a2ffb6e98ccfa1315cc254968b672&dn=Rick+and+Morty+S03E01+"
                       "720p+HDTV+HEVC+x265-iSm&tr=udp%3A%2F%2Ftracker.leechers-paradise.org%3A6969&tr=udp%3A%"
                       "2F%2Fzer0day.ch%3A1337&tr=udp%3A%2F%2Fopen.demonii.com%3A1337&tr=udp%3A%2F%2Ftracker.coppersurfer.tk%3A6969"
                       "&tr=udp%3A%2F%2Fexodus.desync.com%3A6969";

    char *path = "/tmp";

    struct Torrent *t = NULL;
    t = torrent_new(magnet_uri, path);
    assert_null(t);
    torrent_free(t);

    errno = 0;
}

/* test that initializing torrent fails properly when allocations fail */
static void test_torrent_malloc_failed(void **state) {
    (void) state;

    RESET_MOCKS();
    USE_WRAPPED_MALLOC();
    errno = EADDRNOTAVAIL;

    will_return(__wrap_malloc, NULL);

    char *magnet_uri = "magnet:?xt=urn:btih:3a6b29a9225a2ffb6e98ccfa1315cc254968b672&dn=Rick+and+Morty+S03E01+"
                       "720p+HDTV+HEVC+x265-iSm&tr=udp%3A%2F%2Ftracker.leechers-paradise.org%3A6969&tr=udp%3A%"
                       "2F%2Fzer0day.ch%3A1337&tr=udp%3A%2F%2Fopen.demonii.com%3A1337&tr=udp%3A%2F%2Ftracker.coppersurfer.tk%3A6969"
                       "&tr=udp%3A%2F%2Fexodus.desync.com%3A6969";
    char *path = "/tmp";

    struct Torrent *t = NULL;
    t = torrent_new(magnet_uri, path);
    assert_null(t);
    torrent_free(t);

    errno = 0;
}
