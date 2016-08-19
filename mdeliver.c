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
			ssize_t rd = getline(&line, &linelen, infile);
			if (rd == -1) {
				if (errno == 0)
					// invalid mbox file
					errno = EINVAL;
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
			 tv.tv_sec, tv.tv_usec, (long)getpid(), delivery, host);
		
		snprintf(tmp, sizeof tmp, "%s/tmp/%s", targetdir, id);
	
		outfd = open(tmp, O_CREAT | O_WRONLY | O_EXCL, 0666);
		if (outfd < 0) {
			if (errno == EEXIST)
				goto tryagain;
			return -1;
		}

		outfile = fdopen(outfd, "w");

		while (1) {
			errno = 0;
			ssize_t rd = getline(&line, &linelen, infile);
			if (rd == -1) {
				if (errno != 0)
					return -1;
				break;
			}

			if (Mflag && strncmp("From ", line, 5) == 0)
				break;

			if (Mflag) {
				// MBOXRD: strip first > from >>..>>From
				char *s = line;
				while (*s == '>')
					s++;
				if (strncmp("From ", s, 5) == 0) {
					line++;
					rd--;
				}
			}

			if (fwrite(line, 1, rd, outfile) != (size_t)rd)
				return -1;
		}
		if (fflush(outfile) == EOF)
			return -1;
		if (fsync(outfd) < 0)
			return -1;
		if (fclose(outfile) == EOF)
			return -1;

		char statusflags[5] = { 0 };
		char *f = statusflags;

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
			if (msg && ((v = blaze822_hdr(msg, "status")) ||
				    (v = blaze822_hdr(msg, "x-status")))) {
				if (strchr(v, 'F')) *f++ = 'F';
				if (strchr(v, 'A')) *f++ = 'R';
				if (strchr(v, 'R') || strchr(v, 'O'))
					*f++ = 'S';
				if (strchr(v, 'D')) *f++ = 'T';
			}
		}
		*f = 0;

		snprintf(dst, sizeof dst, "%s/%s/%s:2,%s",
			 targetdir, cflag ? "cur" : "new", id,
			 Xflag ? Xflag : statusflags);
		if (rename(tmp, dst) != 0)
			return -1;

		if (vflag)
			printf("%s\n", dst);
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	int c;
	while ((c = getopt(argc, argv, "cMvX:")) != -1)
		switch(c) {
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
		perror("deliver");
		return 2;
	}

	return 0;
}
