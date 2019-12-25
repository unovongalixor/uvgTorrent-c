#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "macros.h"
#include "mocked_functions.h"

int USE_REAL_MALLOC = 1;
void USE_WRAPPED_MALLOC() {
    // set USE_REAL_MALLOC to 0 to return mocked value
    USE_REAL_MALLOC = 0;
}
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
void USE_WRAPPED_STRNDUP() {
    USE_REAL_STRNDUP = 0;
}
char *__real_strndup(const char *s, size_t n);
void *__wrap_strndup(const char *s, size_t n) {
    if (USE_REAL_STRNDUP == 1) {
        return __real_strndup(s, n);
    } else {
        return (char *) mock();
    }
}

// connect_wait
int __real_connect_wait(int sockfd, const struct sockaddr *addr, socklen_t addrlen, struct timeval * timeout);
int __wrap_connect_wait(int sockfd, const struct sockaddr *addr, socklen_t addrlen, struct timeval * timeout) {
    return 0;
}

// read
ssize_t __real_read(int fd, void *buf, size_t count);
ssize_t __wrap_read(int fd, void * buf, size_t count) {
    struct READ_WRITE_MOCK_VALUED * rw = (struct READ_WRITE_MOCK_VALUED *) mock();
    void * READ_VALUE = rw->value;
    size_t READ_COUNT = rw->count;

    memcpy(buf, READ_VALUE, count);

    if (READ_COUNT == -1) {
        return count;
    }
    return READ_COUNT;
}

// write
ssize_t __real_write(int fd, const void *buf, size_t count);
ssize_t __wrap_write(int fd, const void *buf, size_t count) {
    struct READ_WRITE_MOCK_VALUED * rw = (struct READ_WRITE_MOCK_VALUED *) mock();
    rw->value = (void *) buf;
    size_t WRITE_COUNT = rw->count;
    if (WRITE_COUNT == -1) {
        return count;
    };
    return WRITE_COUNT;
}

// random
long int __real_random(void);
long int __wrap_random(void) {
    return (long int) mock();
}

// poll
int __real_poll(struct pollfd *fds, nfds_t nfds, int timeout);
int __wrap_poll(struct pollfd *fds, nfds_t nfds, int timeout){
    fds[0].revents = POLLIN;
}

// getaddrinfo
int __real_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
int __wrap_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
    struct addrinfo * response = __real_malloc(sizeof(struct addrinfo));
    response->ai_next = NULL;
    response->ai_canonname = NULL;
    *res = response;
    return 0;
}

// socket
int __real_socket(int domain, int type, int protocol);
int __wrap_socket(int domain, int type, int protocol) {
    return 0;
}


void RESET_MOCKS() {
    USE_REAL_MALLOC = 1;
    USE_REAL_STRNDUP = 1;
}