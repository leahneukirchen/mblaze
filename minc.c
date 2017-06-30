#include <sys/types.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blaze822.h"

static int qflag;
static int status;

void
inc(char *dir)
{
	DIR *fd;
	struct dirent *d;
	char src[PATH_MAX];
	char dst[PATH_MAX];

	snprintf(src, sizeof src, "%s/new", dir);
	fd = opendir(src);
	if (!fd) {
		fprintf(stderr, "minc: can't open maildir '%s': %s\n",
		    dir, strerror(errno));
		status = 2;
		return;
	}

	while ((d = readdir(fd))) {
#if defined(DT_REG) && defined(DT_UNKNOWN)
		if (d->d_type != DT_REG && d->d_type != DT_UNKNOWN)
			continue;
#endif
		if (d->d_name[0] == '.')
			continue;

		snprintf(src, sizeof src, "%s/new/%s",
		    dir, d->d_name);
		snprintf(dst, sizeof dst, "%s/cur/%s%s",
		    dir, d->d_name, strstr(d->d_name, ":2,") ? "" : ":2,");
		if (rename(src, dst) < 0) {
			fprintf(stderr, "minc: can't rename '%s' to '%s': %s\n",
			    src, dst, strerror(errno));
			status = 3;
			continue;
		}

		if (!qflag)
			printf("%s\n", dst);
	}

	closedir(fd);
}

int
main(int argc, char *argv[])
{
	int c, i;
	while ((c = getopt(argc, argv, "q")) != -1)
		switch(c) {
		case 'q': qflag = 1; break;
		default:
		usage:
			fprintf(stderr, "Usage: minc [-q] dirs...\n");
			exit(1);
		}

	if (optind == argc)
		goto usage;

        if (pledge("stdio rpath tty", NULL) == -1)
          err(1, "pledge");

	status = 0;
	for (i = optind; i < argc; i++)
		inc(argv[i]);

	return status;
}
