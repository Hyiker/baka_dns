#include "utils/logging.h"

#include <stdarg.h>

#include "stdio.h"
#include "time.h"
#include "utils/conf.h"
int _LOG_INFO(const char* filename, const char* func, const char* fmt, ...) {
    if (!conf.verbose) {
        return 0;
    }

    va_list args;
    va_start(args, fmt);
    char timebuf[100];
    time_t tm;
    tm = time(NULL);
    strftime(timebuf, 100, "%H:%M:%S", localtime(&tm));
    printf("[%s::%s %s] ", filename, func, timebuf);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
    return 1;
}

int _PRINT_U8ARR(const char* filename, const char* func, const uint8_t* u8array,
                 uint32_t len) {
    if (!conf.verbose) {
        return 0;
    }
    char timebuf[100];
    time_t tm;
    tm = time(NULL);
    strftime(timebuf, 100, "%H:%M:%S", localtime(&tm));
    printf("[%s::%s %s] ", filename, func, timebuf);
    for (size_t i = 0; i < len; i++) {
        printf("%.2x ", u8array[i]);
    }
    printf("\n");
    return 1;
}

void _LOG_ERR(const char* filename, const char* func, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char timebuf[100];
    time_t tm;
    tm = time(NULL);
    strftime(timebuf, 100, "%H:%M:%S", localtime(&tm));
    fprintf(stderr, "[%s::%s %s] ", filename, func, timebuf);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fflush(stderr);
}