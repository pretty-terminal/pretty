#ifndef LIB_H
    #define LIB_H

    #include <stdio.h>
    #include <stdlib.h>
    #include <stdarg.h>

    #include "log.h"

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

char *file_read(char const *filepath);
void notify_ui_flush(void);


#endif
