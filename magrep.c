#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <err.h>
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
static int oflag;
static int pflag;
static int qflag;
static int vflag;
static long mflag;
static long matches;

static regex_t pattern;
static char *header;
static char *curfile;

int
match(char *file, char *hdr, char *s)
{
	if (oflag && !cflag && !qflag && !vflag) {
		regmatch_t pmatch;
		int len, matched;
		matched = 0;
		while (*s && regexec(&pattern, s, 1, &pmatch, 0) == 0) {
			s += pmatch.rm_so;
			if (!(len = pmatch.rm_eo-pmatch.rm_so)) {
				s += 1;
				continue;
			}
			if (pflag)
				printf("%s: %s: ", file, hdr);
			printf("%.*s\n", len, s);
			s += len;
			matched++;
		}
		return (matched && matches++);
	} else if (vflag ^ (regexec(&pattern, s, 0, 0, 0) == 0)) {
		if (qflag)
			exit(0);
		matches++;
		if (!cflag) {
			printf("%s", file);
			if (pflag && !vflag)
				printf(": %s: %s", hdr, s);
			putchar('\n');
		}
		if (mflag && matches >= mflag)
			exit(0);
		return 1;
	}

	return 0;
}

blaze822_mime_action
match_part(int depth, struct message *msg, char *body, size_t bodylen)
{
	(void)depth;

	char *ct = blaze822_hdr(msg, "content-type");

	blaze822_mime_action r = MIME_CONTINUE;

	if (!ct || strncmp(ct, "text/plain", 10) == 0) {
		char *charset = 0, *cs, *cse;
		if (blaze822_mime_parameter(ct, "charset", &cs, &cse))
			charset = strndup(cs, cse-cs);
		if (!charset ||
		    strcasecmp(charset, "utf-8") == 0 ||
		    strcasecmp(charset, "utf8") == 0 ||
		    strcasecmp(charset, "us-ascii") == 0) {
			if (pflag && !cflag && !oflag && !vflag) {
				char *s, *p;
				for (p = s = body; p < body+bodylen+1; p++) {
					if (*p == '\r' || *p == '\n') {
						*p = 0;
						if (p-s > 1)
							match(curfile, "/", s);
						s = p+1;
					}
				}
			} else if (match(curfile, "/", body)) {
				r = MIME_STOP;
			}
		} else {
			/* XXX decode here */
		}
		free(charset);
	}

	return r;
}

void
match_body(char *file)
{
	curfile = file;
	while (*curfile == ' ' || *curfile == '\t')
		curfile++;

	struct message *msg = blaze822_file(curfile);
	if (!msg)
		return;

	blaze822_walk_mime(msg, 0, match_part);
}

void
magrep(char *file)
{
	if (!*header) {
		char *flags = strstr(file, ":2,");
		if (flags)
			match(file, "flags", flags+3);
		return;
	} else if (strcmp(header, "/") == 0) {
		match_body(file);
		return;
	}

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
			match(file, header, d);
		} else if (aflag) {
			char *disp, *addr;
			while ((v = blaze822_addr(v, &disp, &addr))) {
				if (addr && match(file, header, addr))
					break;
			}
		} else {
			match(file, header, v);
		}
	}

	blaze822_free(msg);
}

int
main(int argc, char *argv[])
{
	int c;
	while ((c = getopt(argc, argv, "acdim:opqv")) != -1)
		switch (c) {
		case 'a': aflag = 1; break;
		case 'c': cflag = 1; break;
		case 'd': dflag = 1; break;
		case 'i': iflag = REG_ICASE; break;
		case 'm': mflag = atol(optarg); break;
		case 'o': oflag = 1; break;
		case 'p': pflag = 1; break;
		case 'q': qflag = 1; break;
		case 'v': vflag = 1; break;
		default:
usage:
			fprintf(stderr,
"Usage: magrep [-c|-o|-p|-q|-m max] [-v] [-i] [-a|-d] header:regex [msgs...]\n");
			exit(2);
		}

	if (argc == optind)
		goto usage;
	header = argv[optind++];

	char *rx = strchr(header, ':');
	if (!rx)
		goto usage;

        if (pledge("stdio rpath tty", NULL) == -1)
          err(1, "pledge");

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

	if (cflag && !qflag && !mflag)
		printf("%ld\n", matches);

	return !matches;
}
