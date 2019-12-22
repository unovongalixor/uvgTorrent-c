#ifndef UVGTORRENT_C_DEADLINE_H
#define UVGTORRENT_C_DEADLINE_H

#include <stdint.h>
#include <sys/time.h>
#include <time.h>

/**
* extern int64_t now()
*
* NOTES   : returns current time as milliseconds. useful for checking deadlines
* RETURN  : int64_t
*/
extern int64_t now();

#endif // UVGTORRENT_C_DEADLINE_H