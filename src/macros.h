//
// Created by vongalixor on 2019-12-10.
//

#ifndef UVGTORRENT_C_MACROS_H
#define UVGTORRENT_C_MACROS_H

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "colors.h"

#define clean_errno() (errno == 0 ? "None" : strerror(errno))

#define log_err(M, ...) fprintf(stderr, RED "[ERROR] %s:%d: errno: %s: " NO_COLOR M "\n", __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__)

#define log_warn(M, ...) fprintf(stderr, YELLOW "[WARN]  %s:%d: errno: %s: " NO_COLOR M "\n", __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__)

#define log_info(M, ...) fprintf(stdout, BLUE "[INFO]  %s:%d: " NO_COLOR M "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define throw(M, ...)  { log_err(M, ##__VA_ARGS__); errno=0; goto error; }

#endif //UVGTORRENT_C_MACROS_H
