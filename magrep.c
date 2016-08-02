#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blaze822.h"

static int aflag;
static int cflag;
static int dflag;
static int iflag;
static int qflag;
static int vflag;
static long matches;

static regex_t pattern;
static char *header;

int
match(char *file, char *s)
{
	if (vflag ^ (regexec(&pattern, s, 0, 0, 0) == 0)) {
		if (qflag)
			exit(0);
		if (!cflag)
			printf("%s\n", file);
		matches++;
		return 1;
	}

	return 0;
}

void
magrep(char *file)
{
	char *filename = file;
	while (*filename == ' ' || *filename == '\t')
                filename++;

	struct message *msg = blaze822(filename);
	if (!msg)
		return;

	char *v = blaze822_chdr(msg, header);
	if (v) {
		if (dflag) {
			char d[4096];
			blaze822_decode_rfc2047(d, v, sizeof d, "UTF-8");
			match(file, d);
		} else if (aflag) {
			char *disp, *addr;
                        while ((v = blaze822_addr(v, &disp, &addr))) {
				if (addr && match(file, addr))
					break;
			}
		} else {
			match(file, v);
		}
	}

	blaze822_free(msg);
}

int
main(int argc, char *argv[])
{
	int c;
	while ((c = getopt(argc, argv, "acdiqv")) != -1)
		switch(c) {
                case 'a': aflag = 1; break;
                case 'c': cflag = 1; break;
                case 'd': dflag = 1; break;
                case 'i': iflag = REG_ICASE; break;
                case 'q': qflag = 1; break;
                case 'v': vflag = 1; break;
		default:
		usage:
			fprintf(stderr,
"Usage: magrep [-c|-q] [-v] [-i] [-a|-d] header:regex [msgs...]\n");
			exit(2);
		}

	if (argc == optind)
		goto usage;
	header = argv[optind++];

	char *rx = strchr(header, ':');
	if (!rx)
		goto usage;

	*rx++ = 0;
	int r = regcomp(&pattern, rx, REG_EXTENDED | iflag);
	if (r != 0) {
		char buf[256];
		regerror(r, &pattern, buf, sizeof buf);
		fprintf(stderr, "magrep: regex error: %s\n", buf);
		exit(2);
	}

	if (argc == optind && isatty(0))
		blaze822_loop1(":", magrep);
	else
		blaze822_loop(argc-optind, argv+optind, magrep);
	
	if (cflag)
		printf("%ld\n", matches);

	return !matches;
}
