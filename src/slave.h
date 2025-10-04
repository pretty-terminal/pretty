#ifndef SLAVE_H
    #define SLAVE_H

    #include <pthread.h>
    #include <stdbool.h>
    #include <stddef.h>

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
    char buff[4096]; /* TODO: think about this */
    size_t buff_len;
    pthread_t thread;
    pthread_mutex_t lock;
    bool buff_changed;
} tty_state;

void notify_ui_flush(void);

int tty_new(char *args[static 1]);
void *tty_poll_loop(void *arg);

#endif // SLAVE_H
