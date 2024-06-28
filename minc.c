#include <sys/types.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blaze822.h"
#include "blaze822_priv.h"
#include "xpledge.h"

static int qflag;
static int status;

void
inc(char *dir)
{
	DIR *fd;
	struct dirent *d;
	char src[PATH_MAX];
	char dst[PATH_MAX];

	squeeze_slash(dir);

	snprintf(src, sizeof src, "%s/new", dir);
	fd = opendir(src);
	if (!fd) {
		fprintf(stderr, "minc: can't open maildir '%s': %s\n",
		    src, strerror(errno));
		status = 2;
		return;
	}

	while ((d = readdir(fd))) {
		if (!MAIL_DT(d->d_type))
			continue;

		if (d->d_name[0] == '.')
			continue;

		snprintf(src, sizeof src, "%s/new/%s",
		    dir, d->d_name);
		snprintf(dst, sizeof dst, "%s/cur/%s%s",
		    dir, d->d_name,
		    strstr(d->d_name, MAILDIR_COLON_SPEC_VER_COMMA) ? "" : MAILDIR_COLON_SPEC_VER_COMMA);
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
		switch (c) {
		case 'q': qflag = 1; break;
		default:
usage:
			fprintf(stderr, "Usage: minc [-q] dirs...\n");
			exit(1);
		}

	xpledge("stdio rpath cpath", "");

	status = 0;
	if (optind == argc) {
		if (isatty(0))
			goto usage;
		blaze822_loop(0, 0, inc);
	} else {
		for (i = optind; i < argc; i++)
			inc(argv[i]);
	}

	return status;
}
