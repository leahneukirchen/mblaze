#include <sys/time.h>
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

/*
design rationale:
- only MBOX-RD because it's the only reasonable lossless encoding, and decodes
  MBOX-O fine
- date from Date: since From lines are usually crap
- proper maildir delivery because it's not that hard
- no creation of maildirs, should be a separate tool
*/

static int cflag;
static int kflag;
static int Mflag;
static int vflag;
static char *Xflag;

char *targetdir;
long delivery;

char host[64];
void
gethost() {
	gethostname(host, sizeof host);
	// termination not posix guaranteed
	host[sizeof host - 1] = 0;
	// replace / and : with -
	char *s;
	for (s = host; *s; s++)
		if (*s == '/' || *s == ':')
			*s = '-';
}

int
deliver(FILE *infile)
{
	int outfd;
	FILE *outfile;
	char dst[PATH_MAX];
	char tmp[PATH_MAX];
	char id[128];
	struct timeval tv;

	char *line = 0;
	size_t linelen = 0;

	if (Mflag) {
		// skip to first "From " line
		while (1) {
			errno = 0;
			ssize_t rd = getdelim(&line, &linelen, '\n', infile);
			if (rd == -1) {
				if (errno == 0)
					errno = EINVAL;  // invalid mbox file
				return -1;
			}

			if (strncmp("From ", line, 5) == 0)
				break;
		}
	}

	while (!feof(infile)) {
		delivery++;
tryagain:
		gettimeofday(&tv, 0);

		snprintf(id, sizeof id, "%ld.M%06ldP%ldQ%ld.%s",
		    (long)tv.tv_sec, (long)tv.tv_usec, (long)getpid(),
		    delivery, host);

		snprintf(tmp, sizeof tmp, "%s/tmp/%s", targetdir, id);

		outfd = open(tmp, O_CREAT | O_WRONLY | O_EXCL, 0666);
		if (outfd < 0) {
			if (errno == EEXIST)
				goto tryagain;
			return -1;
		}

		outfile = fdopen(outfd, "w");

		char statusflags[5] = { 0 };

		int in_header = 1;
		int is_old = 0;
		while (1) {
			errno = 0;
			ssize_t rd = getdelim(&line, &linelen, '\n', infile);
			if (rd == -1) {
				if (errno != 0)
					return -1;
				break;
			}
			char *line_start = line;

			if (line[0] == '\n' && !line[1])
				in_header = 0;

			if (Mflag && strncmp("From ", line, 5) == 0)
				break;

			if (Mflag && in_header &&
			    (strncasecmp("status:", line, 6) == 0 ||
			     strncasecmp("x-status:", line, 8) == 0)) {
				char *v = strchr(line, ':');
				if (strchr(v, 'F')) statusflags[0] = 'F';
				if (strchr(v, 'A')) statusflags[1] = 'R';
				if (strchr(v, 'R')) statusflags[2] = 'S';
				if (strchr(v, 'D')) statusflags[3] = 'T';
				if (strchr(v, 'O')) is_old = 1;

				continue;  // drop header
			}

			if (Mflag) {
				// MBOXRD: strip first > from >>..>>From
				char *s = line;
				while (*s == '>')
					s++;
				if (strncmp("From ", s, 5) == 0) {
					line_start++;
					rd--;
				}
			}

			if (fwrite(line_start, 1, rd, outfile) != (size_t)rd)
				return -1;
		}
		if (fflush(outfile) == EOF)
			return -1;
		if (fsync(outfd) < 0)
			return -1;
		if (fclose(outfile) == EOF)
			return -1;

		// compress flags
		int i, j;
		for (i = sizeof statusflags - 1; i >= 0; i--)
			if (!statusflags[i])
				for (j = i+1; j < (int)sizeof statusflags; j++)
					statusflags[j-1] = statusflags[j];

		if (Mflag) {
			struct message *msg = blaze822_file(tmp);
			time_t date = -1;
			char *v;

			if (msg && (v = blaze822_hdr(msg, "date"))) {
				date = blaze822_date(v);
				if (date != -1) {
					const struct timeval times[2] = {
						{ tv.tv_sec, tv.tv_usec },
						{ date, 0 }
					};
					utimes(tmp, times);
				}
			}
		}

		snprintf(dst, sizeof dst, "%s/%s/%s:2,%s",
		    targetdir, (cflag || is_old) ? "cur" : "new", id,
		    Xflag ? Xflag : statusflags);
		if (rename(tmp, dst) != 0)
			return -1;

		if (vflag)
			printf("%s\n", dst);
	}
	return 0;
}

void
refile(char *file)
{
	while (*file == ' ' || *file == '\t')
		file++;

	FILE *f = fopen(file, "r");
	if (!f) {
		fprintf(stderr, "mrefile: %s: %s\n", file, strerror(errno)); 
		return;
	}

	// keep flags
	char *flags = strstr(file, ":2,");
	if (flags)
		Xflag = flags + 3;
	else
		Xflag = "";

	if (deliver(f) < 0) {
		perror("mrefile");
		return;
	}

	fclose(f);
	if (!kflag)
		unlink(file);
}

int
main(int argc, char *argv[])
{
	if (strchr(argv[0], 'f')) {
		// mrefile(1)

		cflag = 1;  // use cur/

		int c;
		while ((c = getopt(argc, argv, "kv")) != -1)
			switch (c) {
			case 'k': kflag = 1; break;
			case 'v': vflag = 1; break;
			default:
usage:
				fprintf(stderr,
				    "Usage: mrefile [-kv] [msgs...] maildir\n");
			exit(1);
		}

		if (argc == optind)
			goto usage;

		targetdir = argv[argc - 1];
		gethost();

		if (argc == optind + 1 && isatty(0))
			goto usage;
		else
			blaze822_loop(argc - 1 - optind, argv + optind, refile);

		return 0;
	}

	int c;
	while ((c = getopt(argc, argv, "cMvX:")) != -1)
		switch (c) {
		case 'c': cflag = 1; break;
		case 'M': Mflag = 1; break;
		case 'v': vflag = 1; break;
		case 'X': Xflag = optarg; break;
		default:
			fprintf(stderr,
"Usage: mdeliver [-c] [-v] [-X flags] dir < message\n"
"       mdeliver -M [-c] [-v] [-X flags] dir < mbox\n"
				);
			exit(1);
		}

	if (argc != optind+1) {
		fprintf(stderr, "usage: mdeliver DIR\n");
		return 1;
	}

	targetdir = argv[optind];

	gethost();

	if (deliver(stdin) < 0) {
		perror("mdeliver");
		return 2;
	}

	return 0;
}
