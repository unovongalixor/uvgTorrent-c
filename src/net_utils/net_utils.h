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
 * @brief convert host ordered bits (little-endian) to network ordered bits (big-endian) for 64 bit ints
 * @param input
 * @return network ordered input
 */
uint64_t htonll_util(uint64_t input);

/**
 * @brief convert host ordered bits (little-endian) to network ordered bits (big-endian) for 32 bit ints
 * @param input
 * @return network ordered input
 */
uint32_t htonl_util(uint32_t input);

/**
 * @brief convert host ordered bits (little-endian) to network ordered bits (big-endian) for 16 bit ints
 * @param input
 * @return network ordered input
 */
uint16_t htons_util(uint16_t input);

/**
 * @brief convert network ordered bits (big-endian) to host ordered bits (little-endian) for 64 bit ints
 * @param input
 * @return host ordered input
 */
uint64_t ntohll_util(uint64_t input);

/**
 * @brief convert network ordered bits (big-endian) to host ordered bits (little-endian) for 32 bit ints
 * @param input
 * @return host ordered input
 */
uint32_t ntohl_util(uint32_t input);

/**
 * @brief convert network ordered bits (big-endian) to host ordered bits (little-endian) for 16 bit ints
 * @param input
 * @return host ordered input
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

/**
 * @brief provide the same interface and functionality
 * @param sockno
 * @param addr
 * @param addrlen
 * @param timeout
 * @return
 */
int connect_wait(int sockno, struct sockaddr *addr, size_t addrlen, struct timeval *timeout);

/**
 * @brief this is a read function that provides a timeout using the poll mechanism
 * @note besides the added timeout, and cancel_flag functions the same as read()
 * @param sockno
 * @param buf
 * @param buf_size
 * @param timeout
 * @param cancel_flag pointer to cancel flag to interrupt read
 * @return -1 on error, 0 on timeout, read_size on success
 */
size_t read_poll(int sockno, void * buf, size_t buf_size, struct timeval *timeout, int * cancel_flag);

static const struct {
    uint64_t (*htonll)(uint64_t input);

    uint32_t (*htonl)(uint32_t input);

    uint16_t (*htons)(uint16_t input);

    uint64_t (*ntohll)(uint64_t input);

    uint32_t (*ntohl)(uint32_t input);

    uint16_t (*ntohs)(uint16_t input);

    int (*connect)(int sockno, struct sockaddr *addr, size_t addrlen, struct timeval *timeout);

    size_t (*read)(int sockno, void * buf, size_t buf_size, struct timeval *timeout, int * cancel_flag);
} net_utils = {
        htonll_util,
        htonl_util,
        htons_util,
        ntohll_util,
        ntohl_util,
        ntohs_util,
        connect_wait,
        read_poll
};

#endif // UVGTORRENT_C_NET_UTILS_H
