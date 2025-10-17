#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>

#include "log.h"

char *pretty_log_get_time(void)
{
    static char timebuf[16];
    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);

    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &local);

    return timebuf;
}

void pretty_log_full(enum log_level level, const char *text, ...)
{
    static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&log_mutex);

    char message[BUFSIZ - 64];

    va_list args;
    va_start(args, text);
    vsnprintf(message, sizeof(message), text, args);
    va_end(args);

    fprintf(stderr, "%s\n", message);
    pthread_mutex_unlock(&log_mutex);
}
