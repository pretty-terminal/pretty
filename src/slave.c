#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pty.h>
#include <pwd.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "pretty.h"
#include "slave.h"
#include "macro_utils.h"
#include "pthread.h"
#include "log.h"

static pid_t pid;

static
void sigchld(int a)
{
	int stat;
	pid_t p = waitpid(pid, &stat, WNOHANG);

	if (p < 0)
		die("waiting for pid %hd failed: %s", pid, strerror(errno));
	if (pid != p)
		return;

	if (WIFEXITED(stat) && WEXITSTATUS(stat))
		die("child exited with status %d", WEXITSTATUS(stat));
	else if (WIFSIGNALED(stat))
		die("child terminated due to signal %d", WTERMSIG(stat));
}

static
void exec_sh(char *args[static 1])
{
    const struct passwd *pw;

    errno = 0;
    if ((pw = getpwuid(getuid())) == NULL) {
        die("getpwuid: %s", errno != 0 ? strerror(errno) : "unknown error");
    }

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

int tty_new(char *args[static 1])
{
    int cmdfd = -1;
    int master;
    int slave;

    if (openpty(&master, &slave, NULL, NULL, NULL) < 0)
        die("openpty call failed: %s", strerror(errno));

    pretty_log(PRETTY_INFO, "Successfully opened a new tty");

    switch (pid = fork()) {
    case -1:
        die("fork failed: %s", strerror(errno));
        break;
    case 0:
        setsid();
        dup2(slave, STDIN_FILENO);
        dup2(slave, STDOUT_FILENO);
        dup2(slave, STDERR_FILENO);
        if (ioctl(slave, TIOCSCTTY, NULL) < 0)
            die("ioctl TIOCSTTY failed: %s", strerror(errno));

        if (slave > 2)
            close(slave);

        exec_sh(args);
        break;
    default:
        close(slave);
        cmdfd = master;
        signal(SIGCHLD, sigchld);
        break;
    }

    return cmdfd;
}

static
void tty_write_raw(tty_state *tty, const char *s, size_t n)
{
    ssize_t r;

    while (n > 0) {
        r = write(tty->pty_master_fd, s, n);

        if (r < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            die("write error on tty: %s", strerror(errno));
        }

        n -= r;
        s += r;
    }
}

void tty_write(tty_state *tty, const char *s, size_t n)
{
    const char *next;

	// This is similar to how the kernel handles ONLCR for ttys
    while (n > 0) {
        if (*s == '\r') {
            next = s + 1;
            tty_write_raw(tty, "\r", 2);
        } else {
            next = memchr(s, '\r', n);
            default_value(next, s + n);
            tty_write_raw(tty, s, next - s);
        }
        n -= next - s;
        s = next;
    }
}

static
bool tty_update(tty_state *tty)
{
    struct pollfd pfd = { .fd = tty->pty_master_fd, .events = POLL_IN };
    int ret = poll(&pfd, 1, -1);

    if (ret < 0)
        return perror("poll"), false;

    if (ret == 0)
        return true;

    if (pfd.revents & POLLIN) {
        char temp[BUFSIZ];
        ssize_t n = read(tty->pty_master_fd, temp, sizeof temp);

        if (n > 0) {
            pthread_mutex_lock(&tty->lock);
            if (tty->buff_len + n < sizeof(tty->buff)) {
                pretty_log(PRETTY_INFO, "Received %zd chars, appending to buffer (current len: %zu)", 
                    n, tty->buff_len);

                memcpy(tty->buff + tty->buff_len, temp, n);
                tty->buff_len += n;

                if (!tty->buff_changed) {
                    tty->buff_changed = true;
                    notify_ui_flush();
                }
            } else {
                pretty_log(PRETTY_INFO, "WARNING: Buffer full, dropping %zd chars", n);
            }

            pthread_mutex_unlock(&tty->lock);
        } else if (n == 0 || errno == EIO) {
            return true;
        } else {
            perror("read");
            return true;
        }
    }

    return true;
}

void *tty_poll_loop(void *arg)
{
    tty_state *tty = arg;

    while (true) {
        tty_update(tty);
        pthread_mutex_lock(&tty->lock);
        if (tty->buff_changed && tty->buff_consumed < tty->buff_len) {
            notify_ui_flush();
            tty->buff_changed = false;
        }
        pthread_mutex_unlock(&tty->lock);
    }
    __builtin_unreachable();
}
