#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <inttypes.h>
#include "../log.h"

int64_t now() {
    struct timeval tv;
    int rc = gettimeofday(&tv, NULL);
    if (rc != 0) {
        return -1;
    }
    int64_t current_time = ((int64_t) tv.tv_sec) * 1000 + (((int64_t) tv.tv_usec) / 1000);
    return current_time;
}
