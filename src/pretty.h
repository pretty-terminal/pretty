#ifndef LIB_H
    #define LIB_H

    #include <stdio.h>
    #include <stdlib.h>
    #include <stdarg.h>

    #include "log.h"
    #include "terminal.h"
    #include "stdint.h"

static inline __attribute__((format(printf, 1, 2)))
void die(const char *errstr, ...)
{
    va_list ap;
    va_start(ap, errstr);

    char buff[BUFSIZ - 64];
    vsnprintf(buff, sizeof(buff), errstr, ap);
    va_end(ap);

    pretty_log(PRETTY_ERROR, "%s", buff);

    exit(EXIT_FAILURE);
}

typedef struct {
    term *pretty;
    uint64_t notify_interval;
} thread_args;

char *file_read(char const *filepath);

#endif
