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
extern void RESET_MOCKS();
extern void USE_WRAPPED_MALLOC();
extern void USE_WRAPPED_STRNDUP();
extern void SET_READ_VALUE(void * value);
extern void SET_READ_COUNT(size_t count);
extern void SET_WRITE_COUNT(size_t count);
extern void SET_RANDOM_VALUE(long int value);

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
