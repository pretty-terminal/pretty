#ifndef SLAVE_H
    #define SLAVE_H

    #include <pthread.h>
    #include <stdbool.h>
    #include <stddef.h>

enum { TTY_RING_CAP = 64 * 1024 };

typedef struct {
    int master;
    int slave;

    pthread_t thread;
    pthread_mutex_t io_lock;

    bool child_exited;
    bool should_exit;
} pty_session;

bool pty_pair(pty_session *pty);
bool pty_init(pty_session *pty);
void *pty_poll_loop(void *arg);
void pty_write(pty_session *pty, const char *s, size_t n);

#endif // SLAVE_H
