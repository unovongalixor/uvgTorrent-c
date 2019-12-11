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
#include "test_torrent.c"

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
            cmocka_unit_test(test_invalid_magnet_uri),
            cmocka_unit_test(test_torrent_new_allocation_failed)
    };


    /* Run the tests */
    return cmocka_run_group_tests(tests, NULL, NULL);
}
