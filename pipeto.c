#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

pid_t
pipeto(const char *cmdline)
{
	int pipe0[2];  // stdout -> stdin
	int pipe1[2];  // child errno -> parent
	pid_t pid;

	if (pipe(pipe0) < 0)
		return -1;
	if (pipe(pipe1) < 0)
		return -1;

	pid = fork();
	if (pid < 0) {
		return -1;
	} else if (pid == 0) {  // in child
		close(pipe1[0]);
		// close errno pipe on successful exec
		fcntl(pipe1[1], F_SETFD, FD_CLOEXEC);

		if (dup2(pipe0[0], 0) < 0)
			exit(111);

		close(pipe0[0]);
		close(pipe0[1]);

		// split cmdline, just on spaces
		char *argv[16];
		int argc = 0;
		char *cp = strdup(cmdline);
		if (!cp)
			exit(111);
		while (argc < 16 && *cp) {
			argv[argc++] = cp;
			cp = strchr(cp, ' ');
			if (!cp)
				break;
			*cp++ = 0;
			while (*cp == ' ')
				cp++;
		}
		argv[argc] = 0;

		if (argv[0])
			execvp(argv[0], argv);
		else
			errno = EINVAL;

		// execvp failed, write errno to parent
		int e = errno;
		(void)! write(pipe1[1], &e, sizeof e);
		exit(111);
	} else {  // in parent
		close(pipe1[1]);

		int e;
		ssize_t n = read(pipe1[0], &e, sizeof e);
		if (n < 0)
			e = errno;
		close(pipe1[0]);

		if (n == 0) {
			// child executed successfully, redirect stdout to it
			if (dup2(pipe0[1], 1) < 0)
				return -1;

			close(pipe0[0]);
			close(pipe0[1]);

			return pid;
		} else {
			errno = e;
			return -1;
		}
	}

//	return pid;
}

int
pipeclose(pid_t pid)
{
	int s;

	fflush(0);
	close(1);
	waitpid(pid, &s, 0);

	return s;
}
