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
 * size_t count : count value to return. -1 will return whatever count is provided as an arguent
 *                i.e. success
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

#endif //UVGTORRENT_C_MOCKED_FUNCTIONS_H
