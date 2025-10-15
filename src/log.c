#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>

#include "log.h"

void pretty_log(enum log_level level, const char *text, ...)
{

    char message[BUFSIZ];
    char buffer[BUFSIZ];
    char timebuf[16];
    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);

    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &local);

    if (text == NULL) {
        snprintf(buffer, sizeof(buffer), "[%s] PRETTY_ERROR: Null message passed to logger\n", timebuf);
        write(STDERR_FILENO, buffer, strlen(buffer));
        return;
    }

    va_list args;
    va_start(args, text);
    vsnprintf(message, sizeof(message), text, args);
    va_end(args);

    snprintf(buffer, sizeof(buffer), "[%s | %s]: %s\n", timebuf,
            (level == PRETTY_ERROR) ? "PRETTY_ERROR" : "PRETTY_INFO", message);
    write((level == PRETTY_ERROR) ? STDERR_FILENO : STDOUT_FILENO, buffer, strlen(buffer));
}
