/*
 * ============================================================================
 *
 *       Filename:  main.c
 *
 *    Description:  Main execution of the tests using cmocka
 *
 *        Created:  04/28/2016 19:21:37
 *       Compiler:  gcc
 *
 *         Author:  Gustavo Pantuza
 *   Organization:  Software Community
 *
 * ============================================================================
 */


#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "../src/torrent.h"


/* include here your files that contain test functions */




/* test that basic magnet_uri parsing works */
static void test_magnet_uri_parse_success(void **state) {
    /**
     * If you want to know how to use cmocka, please refer to:
     * https://api.cmocka.org/group__cmocka__asserts.html
     */

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

    char * magnet_uri = "this_is_not_a_magnet_uri";
    char * path = "/tmp";

    struct Torrent *t = NULL;
    t = torrent_new(magnet_uri, path);
    assert_null(t);
    torrent_free(t);
}

/**
 * Test runner function
 */
int
main(void) {

    /**
     * Insert here your test functions
     */
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_magnet_uri_parse_success),
            cmocka_unit_test(test_invalid_magnet_uri)
    };


    /* Run the tests */
    return cmocka_run_group_tests(tests, NULL, NULL);
}
