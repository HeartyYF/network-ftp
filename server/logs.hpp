#ifndef __FTP_LOGS__
#define __FTP_LOGS__

#include <time.h>
#include <stdio.h>
#include <stdarg.h>

#include "types.hpp"
#include "config.hpp"

char* logbuf = NULL;
time_t timet;

void logs(enum LOG_TYPE type, const char* format, ...) {
    if (logbuf == NULL) {
        logbuf = (char*)malloc(size);
    }
    time(&timet);
    // this pointer points to a static buffer
    // so no need to free result
    char* result = ctime(&timet);
    char* n_format;
    result[strlen(result) - 1] = ' ';
    printf("%s", result);
    switch (type) {
        case LOG_TYPE_ERRO:
            printf("[ERRO]");
            break;
        case LOG_TYPE_WARN:
            printf("[WARN]");
            break;
        case LOG_TYPE_INFO:
            printf("[INFO]");
            break;
        case LOG_TYPE_DBUG:
            printf("[DBUG]");
            break;
        case LOG_TYPE_TRCE:
            printf("[TRCE]");
            break;
        case LOG_TYPE_RECV:
            printf("[RECV]");
            // remove trailing CRLF
            n_format = (char*)malloc(strlen(format) + 1);
            strcpy(n_format, format);
            n_format[strlen(n_format) - 2] = '\0';
            break;
        default:
            printf("[NONE]");
            break;
    }
    va_list args;
    va_start(args, format);
    if (type == LOG_TYPE_RECV) {
        vsnprintf(logbuf, size, n_format, args);
    } else {
        vsnprintf(logbuf, size, format, args);
    }
    printf(" %s", logbuf);
    if (logbuf[strlen(logbuf) - 1] != '\n') {
        printf("\n");
    }
    if (type == LOG_TYPE_RECV) {
        free(n_format);
    }
}

#endif