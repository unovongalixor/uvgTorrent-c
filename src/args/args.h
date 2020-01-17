/*
 * ============================================================================
 *
 *       Filename:  args.h
 *
 *    Description:  Header file of the command line options parser
 *
 *        Created:  24/03/2016 21:30:39 PM
 *       Compiler:  gcc
 *
 *         Author:  Gustavo Pantuza
 *   Organization:  Software Community
 *
 * ============================================================================
 */



#ifndef UVGTORRENT_C_ARGS_H
#define UVGTORRENT_C_ARGS_H


#include <stdbool.h>
#include <getopt.h>


/* Max size of a file name */
#define MAX_ARG_LENGTH 512


/* Defines the command line allowed options struct */
struct options {
    char magnet_uri[MAX_ARG_LENGTH];
    char path[MAX_ARG_LENGTH];
    int port;
};


/* Exports options as a global type */
typedef struct options options_t;


/* Public functions section */
void options_parser(int argc, char *argv[], options_t *options);


#endif // UVGTORRENT_C_ARGS_H
