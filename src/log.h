#ifndef LOG_H
    #define LOG_H

enum log_level {
    PRETTY_ERROR,
    PRETTY_WARN,
    PRETTY_INFO,
    PRETTY_DEBUG
};

void pretty_log_full(enum log_level level, const char *text, ...)
    __attribute__((format(printf, 2, 3)));

char *pretty_log_get_time(void);

#ifdef DEBUG_MODE
    #define LOG_HEAD "[%s | %s] @ %s:%d: "
    #define LOG_HEAD_ARGS(lvl) pretty_log_get_time(), #lvl, __FILE__, __LINE__
#else
    #define LOG_HEAD "[%s | %s]: "
    #define LOG_HEAD_ARGS(lvl) pretty_log_get_time(), #lvl
#endif

#define pretty_log(lvl, msg, ...) \
    pretty_log_full(lvl, LOG_HEAD msg, LOG_HEAD_ARGS(lvl), ##__VA_ARGS__)

#endif // LOG_H
