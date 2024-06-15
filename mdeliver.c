#include <sys/stat.h>
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
#include <strings.h>
#include <unistd.h>

#include "blaze822.h"
#include "xpledge.h"

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
int preserve_mtime;
int try_rename;

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
deliver(char *infilename)
{
	int outfd;
	FILE *infile;
	FILE *outfile;
	char dst[PATH_MAX];
	char tmp[PATH_MAX];
	char id[128];
	struct timeval tv;

	char *line = 0;
	size_t linelen = 0;

	if (infilename) {
		if (try_rename) {
			infile = 0;
		} else {
			infile = fopen(infilename, "r");
			if (!infile) {
				fprintf(stderr, "mrefile: %s: %s\n",
				    infilename, strerror(errno));
				return -1;
			}
		}
	} else {
		// mdeliver
		infile = stdin;
	}

	if (Mflag) {
		// skip to first "From " line
		while (1) {
			errno = 0;
			ssize_t rd = getdelim(&line, &linelen, '\n', infile);
			if (rd == -1) {
				if (errno == 0)
					errno = EINVAL;  // invalid mbox file
				goto fail;
			}

			if (strncmp("From ", line, 5) == 0)
				break;
		}
	}

	do {
		delivery++;
try_again:
		gettimeofday(&tv, 0);

		snprintf(id, sizeof id, "%ld.M%06ldP%ldQ%ld.%s",
		    (long)tv.tv_sec, (long)tv.tv_usec, (long)getpid(),
		    delivery, host);

		snprintf(tmp, sizeof tmp, "%s/tmp/%s", targetdir, id);

		if (try_rename) {
			snprintf(dst, sizeof dst, "%s/%s/%s"MAILDIR_COLON_SPEC_VER_COMMA"%s",
			    targetdir, cflag ? "cur" : "new", id, Xflag);
			if (rename(infilename, dst) == 0)
				goto success;

			/* rename failed, open file and try copying */

			infile = fopen(infilename, "r");
			if (!infile) {
				fprintf(stderr, "mrefile: %s: %s\n",
				    infilename, strerror(errno));
				return -1;
			}
		}

		struct stat st;
		if (fstat(fileno(infile), &st) < 0)
			st.st_mode = 0600;
		if (S_ISFIFO(st.st_mode))
			st.st_mode = 0600;
		outfd = open(tmp, O_CREAT | O_WRONLY | O_EXCL,
		    st.st_mode & 07777);
		if (outfd < 0) {
			if (errno == EEXIST)
				goto try_again;
			if (errno == ENOENT)
				fprintf(stderr, "mrefile: %s/tmp: %s\n",
				    targetdir, strerror(errno));
			else
				fprintf(stderr, "mrefile: %s: %s\n",
				    tmp, strerror(errno));
			goto fail;
		}

		outfile = fdopen(outfd, "w");

		char statusflags[5] = { 0 };

		int in_header = 1;
		int is_old = 0;
		int prev_line_empty = 0;
		int this_line_empty = 0;  // only for mbox parsing
		while (1) {
			errno = 0;
			ssize_t rd = getdelim(&line, &linelen, '\n', infile);
			if (rd == -1) {
				if (errno != 0)
					goto fail;
				break;
			}
			char *line_start = line;

			if (line[0] == '\n' && (!line[1] ||
			                        (line[1] == '\r' && !line[2]))) {
				this_line_empty = Mflag ? 1 : 0;
				in_header = 0;
			} else {
				this_line_empty = 0;
			}

			if (Mflag && strncmp("From ", line, 5) == 0)
				break;

			if (Mflag && in_header &&
			    (strncasecmp("status:", line, 7) == 0 ||
			     strncasecmp("x-status:", line, 9) == 0)) {
				char *v = strchr(line, ':');
				if (v) {
					if (strchr(v, 'F')) statusflags[0] = 'F';
					if (strchr(v, 'A')) statusflags[1] = 'R';
					if (strchr(v, 'R')) statusflags[2] = 'S';
					if (strchr(v, 'D')) statusflags[3] = 'T';
					if (strchr(v, 'O')) is_old = 1;
				}
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

			// print delayed empty line
			if (prev_line_empty)
				if (fputc('\n', outfile) == EOF)
					goto fail;
			if (!this_line_empty)
				if (fwrite(line_start, 1, rd, outfile) != (size_t)rd)
					goto fail;

			prev_line_empty = this_line_empty;
		}
		if (fflush(outfile) == EOF)
			goto fail;
		if (fsync(outfd) < 0)
			goto fail;
		if (fclose(outfile) == EOF)
			goto fail;

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
			blaze822_free(msg);
		}

		if (preserve_mtime) {
#if defined(AT_FDCWD) && defined(UTIME_NOW) && defined(UTIME_OMIT)
			const struct timespec times[2] = {
				{ tv.tv_sec, tv.tv_usec * 1000L },
#if (defined(__APPLE__) && defined(__MACH__))
			        st.st_mtimespec
#else /* POSIX.1-2008 */
				st.st_mtim
#endif
			};
			utimensat(AT_FDCWD, tmp, times, 0);
#else
			const struct timeval times[2] = {
				tv,
				{ st.st_mtime, 0 }
			};
			utimes(tmp, times);
#endif
		}

		snprintf(dst, sizeof dst, "%s/%s/%s"MAILDIR_COLON_SPEC_VER_COMMA"%s",
		    targetdir, (cflag || is_old) ? "cur" : "new", id,
		    Xflag ? Xflag : statusflags);
		if (rename(tmp, dst) != 0)
			goto fail;

success:
		if (vflag)
			printf("%s\n", dst);
	} while (Mflag && !feof(infile));

	if (infile)
		fclose(infile);
	return 0;

fail:
	if (infile)
		fclose(infile);
	return -1;
}

void
refile(char *file)
{
	while (*file == ' ' || *file == '\t')
		file++;

	// keep flags
	char *flags = strstr(file, MAILDIR_COLON_SPEC_VER_COMMA);
	if (flags)
		Xflag = flags + 3;
	else
		Xflag = "";

	if (deliver(file) < 0) {
		perror("mrefile");
		return;
	}

	if (!kflag && !try_rename)
		unlink(file);
}

int
main(int argc, char *argv[])
{
	if (strchr(argv[0], 'f')) {
		// mrefile(1)

		cflag = 1;  // use cur/
		preserve_mtime = 1;
		try_rename = 1;

		int c;
		while ((c = getopt(argc, argv, "kv")) != -1)
			switch (c) {
			case 'k': kflag = 1; try_rename = 0; break;
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
usage2:
			fprintf(stderr,
"Usage: mdeliver [-c] [-v] [-X flags] dir < message\n"
"       mdeliver -M [-c] [-v] [-X flags] dir < mbox\n"
				);
			exit(1);
		}

	if (argc != optind+1)
		goto usage2;

	xpledge("stdio rpath wpath cpath fattr", "");
	if (!preserve_mtime && !Mflag) {
		/* drop fattr */
		xpledge("stdio rpath wpath cpath", "");
	}

	targetdir = argv[optind];

	gethost();

	if (deliver(0) < 0) {
		perror("mdeliver");
		return 2;
	}

	return 0;
}
