#ifndef LOG_H
    #define LOG_H

enum log_level {
    PRETTY_ERROR,
    PRETTY_INFO
};

void pretty_log(enum log_level level, const char *text, ...)
    __attribute__((format(printf, 2, 3)));

#endif // LOG_H
