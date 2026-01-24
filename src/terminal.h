#ifndef TERMINAL_H
    #define TERMINAL_H

    #include "slave.h"

#define FOREACH_EVENT(EVENT) \
        EVENT(SCROLL_UP)   \
        EVENT(SCROLL_DOWN)  \

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

enum event {
    FOREACH_EVENT(GENERATE_ENUM)
};

enum term_mode {
    MODE_WRAP        = 1 << 0,
    MODE_INSERT      = 1 << 1,
    MODE_ALTSCREEN   = 1 << 2,
    MODE_CRLF        = 1 << 3,
    MODE_ECHO        = 1 << 4,
    MODE_PRINT       = 1 << 5,
    MODE_UTF8        = 1 << 6,
};

typedef struct {
    pty_session pty;

    char buff[TTY_RING_CAP]; /* TODO: think about this */
    size_t head;
    size_t last_head;
    size_t tail;
    size_t scroll_tail;

    pthread_mutex_t buffer_lock;
    bool buff_changed;
    bool overwrite_oldest;
    bool auto_scroll;
} term;

size_t ring_read_span(const term *pretty, const char **ptr);
void ring_consume(term *pretty, size_t k);
void ring_update(term *pretty, const char *src, size_t nbytes);
void calculate_scroll_internal(term *pretty, enum event dir);
void calculate_scroll(term *pretty, enum event dir);
void read_to_buff(
    term *pretty,
    char *buff,
    size_t buff_size,
    size_t *buff_pos
);

#endif // TERMINAL_H
