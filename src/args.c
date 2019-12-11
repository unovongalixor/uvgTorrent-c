/*
 * ============================================================================
 *
 *       Filename:  args.c
 *
 *    Description:  Command line options parser using GNU getopt
 *
 *        Created:  24/03/2015 22:00:09 PM
 *       Compiler:  gcc
 *
 *         Author:  Gustavo Pantuza
 *   Organization:  Software Community
 *
 * ============================================================================
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "messages.h"
#include "args.h"
#include "colors.h"


/*
 * Sets the default options
 */
static void set_default_options(options_t* options)
{
    memset(options->magnet_uri, '\0', sizeof(options->magnet_uri));
    memset(options->path, '\0', sizeof(options->path));
}


/*
 * Finds the matching case of the current command line option
 */
void
switch_options (int arg, options_t* options)
{
    switch (arg)
    {
        case 'h':
            help();
            exit(EXIT_SUCCESS);

        case 'm':
            strncpy(options->magnet_uri, optarg, MAX_ARG_LENGTH);
            break;

        case 'p':
            strncpy(options->path, optarg, MAX_ARG_LENGTH);
            break;
    }
}


/*
 * Public function that loops until command line options were parsed
 */
void
options_parser (int argc, char* argv[], options_t* options)
{
    set_default_options(options);

    int arg; /* Current option */

    /* getopt allowed options */
    static struct option long_options[] =
    {
        {"help", no_argument, 0, 'h'},
        {"magnet_uri", required_argument, NULL, 'm'},
        {"path", required_argument, NULL, 'p'},

    };

    while (true) {

        int option_index = 0;
        arg = getopt_long(argc, argv, "hm:p:", long_options, &option_index);

        /* End of the options? */
        if (arg == -1) break;

        /* Find the matching case of the argument */
        switch_options(arg, options);
    }

}
