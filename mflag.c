#include <sys/types.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <search.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blaze822.h"
#include "blaze822_priv.h"
#include "xpledge.h"

static int8_t flags[255];
static int vflag = 0;

char **args;
ssize_t argsalloc = 256;
int idx = 0;
char *curfile;

void
add(char *file)
{
	if (idx >= argsalloc) {
		argsalloc *= 2;
		if (argsalloc < 0)
			exit(-1);
		args = realloc(args, sizeof (char *) * argsalloc);
	}
	if (!args)
		exit(-1);
	while (*file == ' ' || *file == '\t')
		file++;
	args[idx] = strdup(file);
	idx++;
}

void
flag(char *file)
{
	int indent = 0;
	while (file[indent] == ' ' || file[indent] == '\t')
		indent++;

	char *f = strstr(file, MAILDIR_COLON_SPEC_VER_COMMA);
	if (!f)
		goto skip;

	if (args) {
		int i;
		for (i = 0; i < idx; i++)
			if (strcmp(file+indent, args[i]) == 0)
				goto doit;
		goto skip;
	}

doit:
	;
	int8_t myflags[255] = { 0 };
	char *s;
	for (s = f+3; *s; s++)
		myflags[(unsigned int)*s] = 1;

	int changed = 0;
	unsigned int i;
	for (i = 0; i < sizeof myflags; i++) {
		int z = myflags[i];
		myflags[i] += flags[i];
		if ((z <= 0 && myflags[i] > 0) ||
		    (z > 0 && myflags[i] <= 0))
			changed = 1;
	}
	if (changed) {
		char dst[PATH_MAX];
		char *s = file;
		char *t = dst;
		while (s < f+3 && t < dst + sizeof dst - 1)
			*t++ = *s++;
		for (i = 0; i < sizeof myflags && t < dst + sizeof dst - 1; i++)
			if (myflags[i] > 0)
				*t++ = i;
		*t = 0;

		if (rename(file+indent, dst+indent) < 0) {
			fprintf(stderr, "mflag: can't rename '%s' to '%s': %s\n",
			    file+indent, dst+indent, strerror(errno));
			goto skip;
		}

		if (curfile && strcmp(file+indent, curfile) == 0)
			blaze822_seq_setcur(dst+indent);

		printf("%s\n", dst);

		return;
	}

skip:
	if (vflag)
		printf("%s\n", file);
}

int
main(int argc, char *argv[])
{
	int c;
	while ((c = getopt(argc, argv, "PRSTDFprstdfX:x:v")) != -1)
		switch (c) {
		case 'P': case 'R': case 'S': case 'T': case 'D': case 'F':
			flags[(unsigned int)c] = 1;
			break;
		case 'p': case 'r': case 's': case 't': case 'd': case 'f':
			flags[(unsigned int)uc(c)] = -1;
			break;
		case 'X':
			while (*optarg)
				flags[(unsigned int)*optarg++] = 1;
			break;
		case 'x':
			while (*optarg)
				flags[(unsigned int)*optarg++] = -1;
			break;
		case 'v': vflag = 1; break;
		default:
			fprintf(stderr,
			    "Usage: mflag [-DFPRST] [-X str]\n"
			    "		  [-dfprst] [-x str]\n"
			    "		  [-v] [msgs...]\n"
			);
			exit(1);
		}

	xpledge("stdio rpath cpath", "");

	curfile = blaze822_seq_cur();

	if (vflag) {
		if (argc == optind && !isatty(0)) {
			return blaze822_loop(0, 0, flag);  // read from stdin
		}

		args = calloc(argsalloc, sizeof (char *));
		if (!args)
			exit(-1);

		int status;
		if (argc == optind)
			status = blaze822_loop1(".", add);
		else
			status = blaze822_loop(argc-optind, argv+optind, add);

		if (isatty(0))
			status |= blaze822_loop1(":", flag);
		else
			status |= blaze822_loop(0, 0, flag);

		return status;
	}

	if (argc == optind && isatty(0))
		return blaze822_loop1(".", flag);
	else
		return blaze822_loop(argc-optind, argv+optind, flag);
}
