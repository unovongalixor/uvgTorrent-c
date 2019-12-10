/*
 * ============================================================================
 *
 *       Filename:  main.c
 *
 *    Description:  Main file of the project
 *
 *        Created:  03/24/2016 19:40:56
 *
 *         Author:  Gustavo Pantuza
 *   Organization:  Software Community
 *
 * ============================================================================
 */


#include <stdio.h>
#include <stdlib.h>

#include "args.h"
#include "colors.h"
#include "macros.h"
#include "messages.h"
#include "torrent.h"


int
main (int argc, char* argv[])
{
    struct Torrent *t = NULL;

    /* Read command line options */
    options_t options;
    options_parser(argc, argv, &options);

    if (options.magnet_uri[0] == '\0') {
        help();
        exit(EXIT_SUCCESS);
    }

    /* initialize and parse torrent */
    t = new_torrent(options.magnet_uri);

    log_info("%s", t->magnet_uri);

    free(t);
    t = NULL;

    return EXIT_SUCCESS;

error:
    if (t) { free(t); t=NULL; };
    return EXIT_FAILURE;
}

