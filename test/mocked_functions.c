#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "macros.h"

#ifndef UVGTORRENT_C_MOCKED_FUNCTIONS
#define UVGTORRENT_C_MOCKED_FUNCTIONS

// set USE_REAL_MALLOC to 0 to return mocked value
int USE_REAL_MALLOC = 1;
void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size) {
    if (USE_REAL_MALLOC == 1) {
        return __real_malloc(size);
    } else {
        return (void *) mock();
    }
}

// set USE_REAL_STRNDUP to 0 to return mocked value
int USE_REAL_STRNDUP = 1;
char *__real_strndup(const char *s, size_t n);
void *__wrap_strndup(const char *s, size_t n) {
    if (USE_REAL_STRNDUP == 1) {
        return __real_strndup(s, n);
    } else {
        return (char *) mock();
    }
}

// connect
int __real_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int __wrap_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return 0;
}

// read
void * READ_VALUE;
size_t READ_COUNT = -1;
ssize_t __real_read(int fd, void *buf, size_t count);
ssize_t __wrap_read(int fd, void * buf, size_t count) {
    memcpy(buf, READ_VALUE, count);

    if (READ_COUNT == -1) {
        return count;
    }
    return READ_COUNT;
}

// write
void * WRITE_VALUE;
size_t WRITE_COUNT = -1;
ssize_t __real_write(int fd, const void *buf, size_t count);
ssize_t __wrap_write(int fd, const void *buf, size_t count) {
    WRITE_VALUE = (void *) buf;
    if (WRITE_COUNT == -1) {
        return count;
    };
    return WRITE_COUNT;
}

// random
long int RANDOM_VALUE;
long int __real_random(void);
long int __wrap_random(void) {
    return RANDOM_VALUE;
}

// poll
int __real_poll(struct pollfd *fds, nfds_t nfds, int timeout);
int __wrap_poll(struct pollfd *fds, nfds_t nfds, int timeout){
    fds[0].revents = POLLIN;
}


void reset_mocks() {
    USE_REAL_MALLOC = 1;
    USE_REAL_STRNDUP = 1;

    READ_VALUE = NULL;
    READ_COUNT = -1;
    WRITE_VALUE = NULL;
    WRITE_COUNT = -1;
    RANDOM_VALUE = 0;
}
#endif // UVGTORRENT_C_MOCKED_FUNCTIONS
