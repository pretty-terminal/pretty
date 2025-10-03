#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pty.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "pretty.h"
#include "slave.h"
#include "sys/ioctl.h"

static pid_t pid;

void
sigchld(int a)
{
	int stat;
	pid_t p = waitpid(pid, &stat, WNOHANG);

	if (p < 0)
		die("waiting for pid %hd failed: %s\n", pid, strerror(errno));
	if (pid != p)
		return;

	if (WIFEXITED(stat) && WEXITSTATUS(stat))
		die("child exited with status %d\n", WEXITSTATUS(stat));
	else if (WIFSIGNALED(stat))
		die("child terminated due to signal %d\n", WTERMSIG(stat));
}

static
void exec_sh(char *args[static 1])
{
    const struct passwd *pw;

    errno = 0;
    if ((pw = getpwuid(getuid())) == NULL) {
        die("getpwuid: %s\n", errno != 0 ? strerror(errno) : "unknown error");
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
        die("openpty call failed: %s\n", strerror(errno));

    fprintf(stderr, "Successfully opened a new tty\n");

    switch (pid = fork()) {
    case -1:
        die("fork failed: %s\n", strerror(errno));
        break;
    case 0:
        setsid();
        dup2(slave, STDIN_FILENO);
        dup2(slave, STDOUT_FILENO);
        dup2(slave, STDERR_FILENO);
        if (ioctl(slave, TIOCSCTTY, NULL) < 0)
            die("ioctl TIOCSTTY failed: %s\n", strerror(errno));

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
