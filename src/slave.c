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

#include "macro_utils.h"
#include "pretty.h"
#include "slave.h"
#include "sys/ioctl.h"

static int iofd = 1;
int cmdfd;
static pid_t pid;

char *scroll;
char *utmp;

void
sigchld(int a)
{
	int stat;
	pid_t p;

	if ((p = waitpid(pid, &stat, WNOHANG)) < 0)
		die("waiting for pid %hd failed: %s\n", pid, strerror(errno));

	if (pid != p)
		return;

	if (WIFEXITED(stat) && WEXITSTATUS(stat))
		die("child exited with status %d\n", WEXITSTATUS(stat));
	else if (WIFSIGNALED(stat))
		die("child terminated due to signal %d\n", WTERMSIG(stat));
}

static
void exec_sh(char *cmd, char **args)
{
    char *shell, *prog, *arg;
    const struct passwd *pw;

    errno = 0;
    if ((pw = getpwuid(getuid())) == NULL) {
        die("getpwuid: %s\n", errno != 0 ? strerror(errno) : "unknown error");
    }

    if ((shell = getenv("SHELL")) == NULL)
        shell = (pw->pw_shell[0]) ? pw->pw_shell : cmd;

    if (args) {
        prog = args[0];
        arg = NULL;
    } else if (scroll) {
        prog = scroll;
        arg = utmp ? utmp : shell;
    } else if (utmp) {
        prog = utmp;
        arg = NULL;
    } else {
        prog = shell;
        arg = NULL;
    }

    default_value(args, ((char *[]) {prog, arg, NULL}));

	unsetenv("COLUMNS");
	unsetenv("LINES");
	unsetenv("TERMCAP");
	setenv("LOGNAME", pw->pw_name, 1);
	setenv("USER", pw->pw_name, 1);
	setenv("SHELL", shell, 1);
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
    _exit(1);
}

int tty_new(const char *line, char *cmd, const char *output, char **args)
{
    int master, slave;

    if (output != NULL) {
        iofd = (!strcmp(output, "-")) ?
            1 : open(output, O_WRONLY | O_CREAT, 0666);

        if (iofd < 0) {
            fprintf(stderr, "Error opening %s%s\n",
                output, strerror(errno));
        }
    }

    // TODO: implement stty function
	// if (line) {
	// 	if ((cmdfd = open(line, O_RDWR)) < 0)
	// 		die("open line '%s' failed: %s\n",
	// 		    line, strerror(errno));
	// 	dup2(cmdfd, 0);
	// 	stty(args);
	// 	return cmdfd;
	// }

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

        exec_sh(cmd, args);
        fprintf(stderr, "Successfully executed SH\n");
        break;
    default:
        close(slave);
        cmdfd = master;
        signal(SIGCHLD, sigchld);
        break;
    }

    return cmdfd;
}
