#ifndef UVGTORRENT_C_NET_UTILS_H
#define UVGTORRENT_C_NET_UTILS_H

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>

/**
 * uint64_t htonll_util (uint64_t input)
 *
 * uint64_t    input;
 *
 * NOTES   : convert host ordered bits (little-endian) to
 *           network ordered bits (big-endian) for 64 bit ints
 * RETURN  : result
 */
uint64_t htonll_util(uint64_t input);

/**
 * uint32_t htonl_util (uint32_t input)
 *
 * uint32_t    input;
 *
 * NOTES   : convert host ordered bits (little-endian) to
 *           network ordered bits (big-endian) for 32 bit ints
 * RETURN  : result
 */
uint32_t htonl_util(uint32_t input);

/**
 * uint16_t htons_util (uint16_t input)
 *
 * uint16_t    input;
 *
 * NOTES   : convert host ordered bits (little-endian) to
 *           network ordered bits (big-endian) for 16 bit ints
 * RETURN  : result
 */
uint16_t htons_util(uint16_t input);

/**
 * uint64_t ntohll_util (uint64_t input)
 *
 * uint64_t    input;
 *
 * NOTES   : convert network ordered bits (big-endian) to
 *           host ordered bits (little-endian) for 64 bit ints
 * RETURN  : result
 */
uint64_t ntohll_util(uint64_t input);

/**
 * uint32_t ntohl_util (uint32_t input)
 *
 * uint32_t    input;
 *
 * NOTES   : convert network ordered bits (big-endian) to
 *           host ordered bits (little-endian) for 32 bit ints
 * RETURN  : result
 */
uint32_t ntohl_util(uint32_t input);

/**
 * uint16_t ntohs_util (uint16_t input)
 *
 * uint16_t    input;
 *
 * NOTES   : convert network ordered bits (big-endian) to
 *           host ordered bits (little-endian) for 16 bit ints
 * RETURN  : result
 */
uint16_t ntohs_util(uint16_t input);

/**
 * int connect_wait (int sockno, struct sockaddr * addr, size_t addrlen, struct timeval * timeout)
 *
 * struct timeval *    timeout;
 *
 * NOTES   : wrap connect with a select based timeout mechanism
 *         : manipulates socket flags then restors flags after connect
 * RETURN  : success
 */
int connect_wait(int sockno, struct sockaddr *addr, size_t addrlen, struct timeval *timeout);

static const struct {
    uint64_t (*htonll)(uint64_t input);

    uint32_t (*htonl)(uint32_t input);

    uint16_t (*htons)(uint16_t input);

    uint64_t (*ntohll)(uint64_t input);

    uint32_t (*ntohl)(uint32_t input);

    uint16_t (*ntohs)(uint16_t input);

    int (*connect)(int sockno, struct sockaddr *addr, size_t addrlen, struct timeval *timeout);
} net_utils = {
        htonll_util,
        htonl_util,
        htons_util,
        ntohll_util,
        ntohl_util,
        ntohs_util,
        connect_wait
};

#endif // UVGTORRENT_C_NET_UTILS_H
