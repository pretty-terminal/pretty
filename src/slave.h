#ifndef SLAVE_H
    #define SLAVE_H

    #include <pthread.h>
    #include <stdbool.h>
    #include <stddef.h>
    #include <stdio.h>

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
    int pty_master_fd;
    char buff[BUFSIZ]; /* TODO: think about this */
    size_t buff_len;
    size_t buff_consumed; // how much has UI proccessed
    pthread_t thread;
    pthread_mutex_t lock;
    bool buff_changed;
    bool should_exit;
    bool overflowed;
} tty_state;

int tty_new(char *args[static 1]);
void *tty_poll_loop(void *arg);
void tty_write(tty_state *tty, const char *s, size_t n);
void tty_erase_last(tty_state *tty);

#endif // SLAVE_H
