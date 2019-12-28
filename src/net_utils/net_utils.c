#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include "../macros.h"

/**
* int little_endianess ()
*
* PURPOSE: determine if our host bit ordering
*          is little endian.
*/
int little_endianess() {
    /*
    *  if the first bit in memory is 0x67 (last bit in the word)
    *  then we're on a little endian system, and
    *  we'll need to flip bits for network requests
    *  https://en.wikipedia.org/wiki/Endianness#Example
    */
    volatile uint32_t i = 0x01234567;
    return (*((uint8_t * )(&i))) == 0x67;
}

/**
* uint64_t byteswap64 (uint64_t input)
*
* uint64_t    input;
*
* NOTES   : reverse byte order between big-endian and little-endian
* RETURN  : result
*/
uint64_t byteswap64(uint64_t input) {
    uint64_t v1 = ntohl(input & 0x00000000ffffffffllu);
    uint64_t v2 = ntohl(input >> 32);
    return (v1 << 32) | v2;
}

/**
* uint64_t htonll_util (uint64_t input)
*
* uint64_t    input;
*
* NOTES   : convert host ordered bits (little-endian) to
*           network ordered bits (big-endian) for 64 bit ints
* RETURN  : result
*/
uint64_t htonll_util(uint64_t input) {
    if (little_endianess()) {
        return byteswap64(input);
    } else {
        return input;
    }
}

uint32_t htonl_util(uint32_t input) {
    if (little_endianess()) {
        return htonl(input);
    } else {
        return input;
    }
}

uint16_t htons_util(uint16_t input) {
    if (little_endianess()) {
        return htons(input);
    } else {
        return input;
    }
}

uint64_t ntohll_util(uint64_t input) {
    if (little_endianess()) {
        return byteswap64(input);
    } else {
        return input;
    }
}

uint32_t ntohl_util(uint32_t input) {
    if (little_endianess()) {
        return ntohl(input);
    } else {
        return input;
    }
}

uint16_t ntohs_util(uint16_t input) {
    if (little_endianess()) {
        return ntohs(input);
    } else {
        return input;
    }
}

int connect_wait(
        int sockno,
        struct sockaddr *addr,
        size_t addrlen,
        struct timeval *timeout) {
    int res, opt;

    // get socket flags
    if ((opt = fcntl(sockno, F_GETFL, NULL)) < 0) {
        return -1;
    }

    // set socket non-blocking
    if (fcntl(sockno, F_SETFL, opt | O_NONBLOCK) < 0) {
        return -1;
    }

    // try to connect
    if ((res = connect(sockno, addr, addrlen)) < 0) {
        if (errno == EINPROGRESS) {
            fd_set wait_set;

            // make file descriptor set with socket
            FD_ZERO(&wait_set);
            FD_SET(sockno, &wait_set);

            // wait for socket to be writable; return after given timeout
            res = select(sockno + 1, NULL, &wait_set, NULL, timeout);
        }
    }
        // connection was successful immediately
    else {
        res = 1;
    }

    // reset socket flags
    if (fcntl(sockno, F_SETFL, opt) < 0) {
        return -1;
    }

    // an error occured in connect or select
    if (res < 0) {
        return -1;
    }
        // select timed out
    else if (res == 0) {
        errno = ETIMEDOUT;
        return 1;
    }
        // almost finished...
    else {
        socklen_t len = sizeof(opt);

        // check for errors in socket layer
        if (getsockopt(sockno, SOL_SOCKET, SO_ERROR, &opt, &len) < 0) {
            return -1;
        }

        // there was an error
        if (opt) {
            errno = opt;
            return -1;
        }
    }

    return 0;
}

size_t read_poll(int sockno, void * buf, size_t buf_size, struct timeval * timeout, int * cancel_flag) {

    struct pollfd fds[1];

    fds[0].fd = sockno;
    fds[0].events = POLLIN;

    size_t read_count = -1;

    while (1) {
        int ret;

        ret = poll(fds, 1, 1000);

        if (*cancel_flag == 1) {
            return read_count;
        }

        if (ret == -1) {
            return read_count;
        } else if (ret == 0) {
            timeout->tv_sec--;
            if (timeout->tv_sec == 0) {
                return ret;
            }
        } else if (fds[0].revents & POLLIN) {
            read_count = read(sockno, buf, buf_size);
            break;
        } else {
            return read_count;
        }
    }
    return read_count;
}
