#ifndef LOGGING_H
#define LOGGING_H
#include <string.h>

#include "stdint.h"
#define __FILENAME__ \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define LOG_INFO(...) _LOG_INFO(__FILENAME__, __func__, __VA_ARGS__)

#define PRINT_U8ARR(a, b) _PRINT_U8ARR(__FILENAME__, __func__, a, b)
#define LOG_ERR(...) _LOG_ERR(__FILENAME__, __func__, __VA_ARGS__)
// log format message into stdin if verbose
// if verbose on, return 1
// else return 0
int _LOG_INFO(const char *, const char *, const char *, ...);

// log u8 array into stdin if verbose
// if verbose on, return 1
// else return 0
int _PRINT_U8ARR(const char *, const char *, const uint8_t *, uint32_t);

// log format message into stderr despite verbose or not
void _LOG_ERR(const char *, const char *, const char *, ...);

#endif