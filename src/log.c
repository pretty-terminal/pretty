#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>

#include "log.h"

void pretty_log(enum log_level level, const char *text, ...)
{
    static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    pthread_mutex_lock(&log_mutex);

    char message[BUFSIZ - 64];
    char timebuf[16];
    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);

    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &local);

    if (text == NULL) {
        fprintf(stderr, "[%s] PRETTY_ERROR: Null message passed to logger\n", timebuf);
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    va_list args;
    va_start(args, text);
    vsnprintf(message, sizeof(message), text, args);
    va_end(args);

    const char *logstr;

    switch (level) {
    case PRETTY_ERROR:
        logstr = "PRETTY_ERROR";
        break;
    case PRETTY_WARN:
        logstr = "PRETTY_WARN";
        break;
    case PRETTY_INFO:
        logstr = "PRETTY_INFO";
        break;
    case PRETTY_DEBUG:
        logstr = "PRETTY_DEBUG";
        break;
    default:
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    fprintf(stderr, "[%s | %s]: %s\n", timebuf, logstr, message);
    pthread_mutex_unlock(&log_mutex);
}
