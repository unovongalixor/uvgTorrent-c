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

#include "args/args.h"
#include "colors.h"
#include "macros.h"
#include "messages/messages.h"
#include "torrent/torrent.h"


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

    if (options.path[0] == '\0') {
        help();
        exit(EXIT_SUCCESS);
    }

    /* initialize and parse torrent */
    t = torrent_new(options.magnet_uri, options.path);
    if (!t) {
        throw("torrent failed to initialize");
    }

    log_info("preparing to download torrent :: %s", t->name);
    log_info("torrent hash :: %s", t->hash);
    log_info("saving torrent to path :: %s", t->path);

    torrent_free(t);

    return EXIT_SUCCESS;

error:
    torrent_free(t);
    // return success allows valgrind to test memory freeing during errors
    return EXIT_SUCCESS;
}

