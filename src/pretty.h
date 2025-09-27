#ifndef LIB_H
    #define LIB_H

    #include <stdio.h>
    #include <stdlib.h>
    #include <stdarg.h>

static inline __attribute__((format(printf, 1, 2)))
void die(const char *errstr, ...)
{
    va_list ap;
    va_start(ap, errstr);

    vfprintf(stderr, errstr, ap);
    va_end(ap);

    exit(EXIT_FAILURE);
}

char *file_read(char const *filepath);

#endif
