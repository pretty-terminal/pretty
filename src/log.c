#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>

#include "log.h"
#include "macro_utils.h"

void pretty_log(enum log_level level, const char *text, ...)
{

    char message[BUFSIZ - 64];
    char buffer[BUFSIZ];
    char timebuf[16];
    ssize_t result;
    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);

    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &local);

    if (text == NULL) {
        snprintf(buffer, sizeof(buffer), "[%s] PRETTY_ERROR: Null message passed to logger\n", timebuf);
        result = write(STDERR_FILENO, buffer, strlen(buffer));
        UNUSED(result);
        return;
    }

    va_list args;
    va_start(args, text);
    vsnprintf(message, sizeof(message), text, args);
    va_end(args);

    int fd;
    const char *logstr;

    switch (level) {
    case PRETTY_ERROR:
        fd = STDERR_FILENO;
        logstr = "PRETTY_ERROR";
        break;
    case PRETTY_WARN:
        fd = STDERR_FILENO;
        logstr = "PRETTY_WARN";
        break;
    case PRETTY_INFO:
        fd = STDOUT_FILENO;
        logstr = "PRETTY_INFO";
        break;
    case PRETTY_DEBUG:
        fd = STDOUT_FILENO;
        logstr = "PRETTY_DEBUG";
        break;
    default:
        return;
    }

    snprintf(buffer, sizeof(buffer), "[%s | %s]: %s\n", timebuf, logstr, message);

    result = write(fd, buffer, strlen(buffer));
    UNUSED(result);
}
