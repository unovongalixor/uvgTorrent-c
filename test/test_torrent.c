#include "torrent.h"
#include <string.h>

/* MOCK FUNCTIONS */

// set USE_REAL_MALLOC to 0 to return mocked value
int USE_REAL_MALLOC = 1;
void * __real_malloc( size_t size );
void * __wrap_malloc( size_t size )
{
  if (USE_REAL_MALLOC == 1) {
    return __real_malloc( size );
  } else {
    return (void *) mock();
  }
}

// set USE_REAL_STRNDUP to 0 and provide "{MOCK_DATA}" as a string to strndup to return mocked value
int USE_REAL_STRNDUP = 1;
char* __real_strndup(const char *s, size_t n);
void* __wrap_strndup(const char *s, size_t n)
{
      // if we try to strndup a string "{MOCK_DATA}" return the value of mock(); instead
      if (USE_REAL_STRNDUP == 0 && strcmp(s, "{MOCK_DATA}") !=0 ) {
          return (char *) mock();
      } else {
         return __real_strndup(s, n);
      }
}

/* TESTS */
static void test_magnet_uri_parse_success(void **state) {
    /**
     * If you want to know how to use cmocka, please refer to:
     * https://api.cmocka.org/group__cmocka__asserts.html
     */
     (void) state;

     USE_REAL_MALLOC = 1;
     USE_REAL_STRNDUP = 1;

    char * magnet_uri = "magnet:?xt=urn:btih:3a6b29a9225a2ffb6e98ccfa1315cc254968b672&dn=Rick+and+Morty+S03E01+"
                          "720p+HDTV+HEVC+x265-iSm&tr=udp%3A%2F%2Ftracker.leechers-paradise.org%3A6969&tr=udp%3A%"
                          "2F%2Fzer0day.ch%3A1337&tr=udp%3A%2F%2Fopen.demonii.com%3A1337&tr=udp%3A%2F%2Ftracker.coppersurfer.tk%3A6969"
                          "&tr=udp%3A%2F%2Fexodus.desync.com%3A6969";
    char * path = "/tmp";

    struct Torrent *t = NULL;
    t = torrent_new(magnet_uri, path);
    assert_non_null(t);

    assert_string_equal(t->magnet_uri, magnet_uri);
    assert_string_equal(t->path, path);
    assert_string_equal(t->name, "Rick+and+Morty+S03E01+720p+HDTV+HEVC+x265-iSm");
    assert_string_equal(t->hash, "urn:btih:3a6b29a9225a2ffb6e98ccfa1315cc254968b672");

    torrent_free(t);
}

/* test that initializing torrent fails properly with an invalid magnet uri */
static void test_invalid_magnet_uri(void **state) {
    /**
     * If you want to know how to use cmocka, please refer to:
     * https://api.cmocka.org/group__cmocka__asserts.html
     */
     (void) state;

     USE_REAL_MALLOC = 1;
     USE_REAL_STRNDUP = 1;

    char * magnet_uri = "this_is_not_a_magnet_uri";
    char * path = "/tmp";

    struct Torrent *t = NULL;
    t = torrent_new(magnet_uri, path);
    assert_null(t);
    torrent_free(t);
}

static void test_torrent_strndup_failed(void **state) {
    (void) state;

    USE_REAL_MALLOC = 1;
    USE_REAL_STRNDUP = 0;

    will_return(__wrap_strndup, NULL);

    char * mock_magnet_uri = "magnet:?xt={MOCK_DATA}&dn=Rick+and+Morty+S03E01+"
                        "720p+HDTV+HEVC+x265-iSm&tr=udp%3A%2F%2Ftracker.leechers-paradise.org%3A6969&tr=udp%3A%"
                        "2F%2Fzer0day.ch%3A1337&tr=udp%3A%2F%2Fopen.demonii.com%3A1337&tr=udp%3A%2F%2Ftracker.coppersurfer.tk%3A6969"
                        "&tr=udp%3A%2F%2Fexodus.desync.com%3A6969";
    char * path = "/tmp";

    struct Torrent *t = NULL;
    t = torrent_new(mock_magnet_uri, path);
    assert_null(t);
    torrent_free(t);
}

static void test_torrent_malloc_failed(void **state) {
    (void) state;

    USE_REAL_MALLOC = 0;
    USE_REAL_STRNDUP = 1;

    will_return(__wrap_malloc, NULL);

    char * magnet_uri = "magnet:?xt=urn:btih:3a6b29a9225a2ffb6e98ccfa1315cc254968b672&dn=Rick+and+Morty+S03E01+"
                          "720p+HDTV+HEVC+x265-iSm&tr=udp%3A%2F%2Ftracker.leechers-paradise.org%3A6969&tr=udp%3A%"
                          "2F%2Fzer0day.ch%3A1337&tr=udp%3A%2F%2Fopen.demonii.com%3A1337&tr=udp%3A%2F%2Ftracker.coppersurfer.tk%3A6969"
                          "&tr=udp%3A%2F%2Fexodus.desync.com%3A6969";
    char * path = "/tmp";

    struct Torrent *t = NULL;
    t = torrent_new(magnet_uri, path);
    assert_null(t);
    torrent_free(t);
}
