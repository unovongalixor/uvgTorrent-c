#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "macros.h"

#ifndef UVGTORRENT_C_MOCKED_FUNCTIONS_H
#define UVGTORRENT_C_MOCKED_FUNCTIONS_H

/* functions for manipulating mocks. use these in your tests */
/**
 * struct READ_WRITE_MOCK_VALUED;
 *
 * void * value : during a read call this value is copied to the provided buffer
 *                during a write call whatever buffer is provided to write will be placed here
 * size_t count : count value to return
 *
 * NOTES   : this struct should be used when mocking system read or write calls
 *           this struct provides you with a mechanism to set the count returned by read and write calls
 *           during a read call, value will be dumped in the provided buffer
 *           during a write call, whatever value is written will be copied into value
 * RETURN  : size_t
 */
struct READ_WRITE_MOCK_VALUED {
    void * value;
    size_t count;
};

/**
 *  extern void RESET_MOCKS();
 *
 * NOTES   : reset malloc and strndup to use the real functions
 *           should be called at the start of every test
 * RETURN  :
 */
extern void RESET_MOCKS();

/**
 *  extern void USE_WRAPPED_MALLOC();
 *
 * NOTES   : use the wrapped malloc function to help simulate memory allocation errors
 *           will return the mock result defined by a will_return call
 * RETURN  :
 */
extern void USE_WRAPPED_MALLOC();

/**
 *  extern void USE_WRAPPED_STRNDUP();
 *
 * NOTES   : use the wrapped strndup function to help simulate memory allocation errors
 *           will return the mock result defined by a will_return call
 * RETURN  :
 */
extern void USE_WRAPPED_STRNDUP();

/* mock implementations. you shouldn't need to play with these directly */
void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size);

char *__real_strndup(const char *s, size_t n);
void *__wrap_strndup(const char *s, size_t n);

// read
ssize_t __real_read(int fd, void *buf, size_t count);
ssize_t __wrap_read(int fd, void * buf, size_t count);

// write
ssize_t __real_write(int fd, const void *buf, size_t count);
ssize_t __wrap_write(int fd, const void *buf, size_t count);


// random
long int __real_random(void);
long int __wrap_random(void);

// connect_wait
int __real_connect_wait(int sockfd, const struct sockaddr *addr, socklen_t addrlen, struct timeval * timeout);
int __wrap_connect_wait(int sockfd, const struct sockaddr *addr, socklen_t addrlen, struct timeval * timeout);

// poll
int __real_poll(struct pollfd *fds, nfds_t nfds, int timeout);
int __wrap_poll(struct pollfd *fds, nfds_t nfds, int timeout);

// getaddrinfo
int __real_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
int __wrap_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);

// socket
int __real_socket(int domain, int type, int protocol);
int __wrap_socket(int domain, int type, int protocol);


#endif //UVGTORRENT_C_MOCKED_FUNCTIONS_H
