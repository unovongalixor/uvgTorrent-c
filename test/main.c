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

/* include here your files that contain test functions */
#include "mocked_functions.c"
#include "test_torrent.c"
#include "test_tracker.c"
#include "test_hash_map.c"
#include "test_bitfield.c"

/**
 * Test runner function
 */
int
main(void) {

    /**
     * Insert here your test functions
     */
    const struct CMUnitTest tests[] = {
            /* Torrent */
            cmocka_unit_test(test_magnet_uri_parse_success),
            cmocka_unit_test(test_run_trackers_success),
            cmocka_unit_test(test_invalid_magnet_uri),
            cmocka_unit_test(test_torrent_strndup_failed),
            cmocka_unit_test(test_torrent_malloc_failed),

            /* Tracker */
            cmocka_unit_test(test_tracker_new),
            cmocka_unit_test(test_tracker_new_strndup_failed),
            cmocka_unit_test(test_tracker_should_announce),
            cmocka_unit_test(test_tracker_timeout_scaling),
            cmocka_unit_test(test_tracker_connect_success),
            cmocka_unit_test(test_tracker_connect_fail_incorrect_transaction_id),
            cmocka_unit_test(test_tracker_connect_fail_incorrect_action),
            cmocka_unit_test(test_tracker_connect_failed_read),
            cmocka_unit_test(test_tracker_connect_failed_read_incomplete),
            cmocka_unit_test(test_tracker_announce_success),
            cmocka_unit_test(test_tracker_scrape_success),

            /* HashMap */
            cmocka_unit_test(test_hashmap_get_and_set),
            cmocka_unit_test(test_hashmap_get_and_set_collision),
            cmocka_unit_test(test_hashmap_empty_collision),
            cmocka_unit_test(test_hashmap_empty_malloc),

            /* Bitfield */
            cmocka_unit_test(test_bitfield_get_and_set),
    };


    /* Run the tests */
    return cmocka_run_group_tests(tests, NULL, NULL);
}
