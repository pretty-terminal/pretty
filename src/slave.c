#define _XOPEN_SOURCE 600

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "pretty.h"
#include "slave.h"
#include "macro_utils.h"
#include "pthread.h"
#include "log.h"
#include "terminal.h"

static pid_t pid;

static
void exec_sh(char *args[static 1])
{
    const struct passwd *pw;

    errno = 0;
    if ((pw = getpwuid(getuid())) == NULL)
        die("getpwuid: %s", errno != 0 ? strerror(errno) : "unknown error");

    unsetenv("COLUMNS");
    unsetenv("LINES");
    unsetenv("TERMCAP");
    setenv("LOGNAME", pw->pw_name, 1);
    setenv("USER", pw->pw_name, 1);
    // TODO: we will handle real shell shenanigans later
    // setenv("SHELL", shell, 1);
    setenv("HOME", pw->pw_dir, 1);
    setenv("TERM", "pretty", 1);

    signal(SIGCHLD, SIG_DFL);
    signal(SIGHUP, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGALRM, SIG_DFL);

    execvp(args[0], args);
    perror("execvp failed");
    exit(EXIT_FAILURE);
}

bool pty_pair(pty_session *pty)
{
    char *slave_name;

    const int PTY_OPEN_FLAGS = O_RDWR | O_NOCTTY;

    pty->master = posix_openpt(PTY_OPEN_FLAGS);

    if (pty->master < 0) die("openpty call failed: %s", strerror(errno));
    if (grantpt(pty->master) == -1) die("failed to grantpt()");
    if (unlockpt(pty->master) == -1) die("failed to unlockpt()");

    slave_name = ptsname(pty->master);
    if (slave_name == NULL) die("failed to ptsname()");

    pty->slave = open(slave_name, PTY_OPEN_FLAGS);
    if (pty->slave < 0) die("failed ot open slave");

    pretty_log(PRETTY_INFO, "Successfully opened a new pty");

    return true;
}

bool pty_init(pty_session *pty)
{

    switch (pid = fork()) {
        case -1:
            die("fork failed: %s", strerror(errno));
            break;
        case 0:
            setsid();
            dup2(pty->slave, STDIN_FILENO);
            dup2(pty->slave, STDOUT_FILENO);
            dup2(pty->slave, STDERR_FILENO);
            if (ioctl(pty->slave, TIOCSCTTY, NULL) < 0)
                die("ioctl TIOCSTTY failed: %s", strerror(errno));

            if (pty->slave > 2) close(pty->slave);

            exec_sh((char *[]){ "/bin/sh", NULL });
            return false;
        default:
            close(pty->slave);
            return true;
    }

    perror("fork");
    return false;
}

static
void pty_write_raw(pty_session *pty, const char *s, size_t n)
{
    ssize_t r;

    while (n > 0) {
        r = write(pty->master, s, n);

        if (r < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            die("write error on pty: %s", strerror(errno));
        }

        n -= r;
        s += r;
    }
}

void pty_write(pty_session *pty, const char *s, size_t n)
{
    const char *next;

    // This is similar to how the kernel handles ONLCR for ttys
    while (n > 0) {
        if (*s == '\r') {
            next = s + 1;
            pty_write_raw(pty, "\r", 1);
        } else {
            next = memchr(s, '\r', n);
            default_value(next, s + n);
            pty_write_raw(pty, s, next - s);
        }
        n -= next - s;
        s = next;
    }
}


static
bool pty_read(term *pretty)
{
    pty_session *pty = &pretty->pty;

    struct pollfd pfd = { .fd = pty->master, .events = POLLIN };
    int ret = poll(&pfd, 1, -1);

    if (ret < 0) {
        if (errno == EINTR) {
            if (pretty->pty.should_exit) return false;
            return true;
        }
        perror("poll");
        return false;
    }

    if (ret == 0) return true;

    if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
        waitpid(pid, NULL, WNOHANG);
        pretty_log(PRETTY_INFO, "PTY(%d) hangup or error", pty->master);
        return false;
    }

    if (pfd.revents & POLLIN) {
        char temp[TTY_RING_CAP];
        ssize_t n = read(pty->master, temp, sizeof temp);

        if (n > 0) {
            pthread_mutex_lock(&pretty->buffer_lock);
            ring_update(pretty, temp, (size_t)n);
            pthread_mutex_unlock(&pretty->buffer_lock);
        } else if (n < 0 && errno == EIO) perror("read");
    }

    return true;
}

void *pty_poll_loop(void *arg)
{
    term *pretty = arg;

    while (!pretty->pty.should_exit) {
        if (!pty_read(pretty)) {
            pretty->pty.child_exited = true;
            break;
        }

        pthread_mutex_lock(&pretty->buffer_lock);
        if (pretty->buff_changed) {
            notify_ui_flush();
            pretty->buff_changed = false;
        }
        pthread_mutex_unlock(&pretty->buffer_lock);
    }

    return NULL;
}
