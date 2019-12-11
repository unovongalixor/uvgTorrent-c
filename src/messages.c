/*
 * ============================================================================
 *
 *       Filename:  messages.c
 *
 *    Description:  Program messages implementation
 *
 *        Created:  24/03/2016 22:48:39
 *       Compiler:  gcc
 *
 *         Author:  Gustavo Pantuza
 *   Organization:  Software Community
 *
 * ============================================================================
 */


#include <stdio.h>
#include <stdlib.h>


#include "colors.h"
#include "messages.h"



/*
 * Help message
 */
void
help ()
{
    fprintf(stdout, BLUE __PROGRAM_NAME__ "\n\n" NO_COLOR);
    options();
}

/*
 * Options message
 */
void
options ()
{
    fprintf(stdout, BROWN "Options:\n\n" NO_COLOR);
    fprintf(stdout, GRAY "\t-h|--help\n" NO_COLOR
                    "\t\tPrints this help message\n\n");
    fprintf(stdout, GRAY "\t-m|--magnet_uri\n" NO_COLOR
                    "\t\tmagnet_uri to download\n\n");
    fprintf(stdout, GRAY "\t-p|--path\n" NO_COLOR
                    "\t\tfolder to save the torrent to\n\n");

}
