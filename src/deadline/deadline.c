#include <stdint.h>
#include <sys/time.h>
#include <time.h>

int64_t now() {
    struct timeval tv;
    int rc = gettimeofday(&tv, NULL);
    if (rc == 0) {
        return -1;
    }
    return ((int64_t)tv.tv_sec) * 1000 + (((int64_t)tv.tv_usec) / 1000);
}
