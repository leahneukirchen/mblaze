#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blaze822.h"

static size_t l;
static char *hdr;

void
header(char *file)
{
	struct message *msg;

	msg = blaze822(file);
	if (!msg)
		return;

	char *v = blaze822_hdr_(msg, hdr, l);
	if (v)
		printf("%s\n", v);
}

static void
printhdr(char *hdr)
{
	int uc = 1;

	while (*hdr && *hdr != ':') {
		putc(uc ? toupper(*hdr) : *hdr, stdout);
		uc = (*hdr == '-');
		hdr++;
	}
	fputs(hdr, stdout);
	fputc('\n', stdout);
}

void
headerall(char *file)
{
	struct message *msg;

	msg = blaze822(file);
	if (!msg)
		return;

	char *h = 0;
	while ((h = blaze822_next_header(msg, h))) {
		char d[4096];
		blaze822_decode_rfc2047(d, h, sizeof d, "UTF-8");

		printhdr(d);
	}
}

int
main(int argc, char *argv[])
{
	void (*cb)(char *) = headerall;

	if (argc >= 2 && argv[1][0] == '-') {
		l = strlen(argv[1])+1;
		hdr = malloc(l);
		hdr[0] = 0;
		char *s = hdr+1;
		char *t = argv[1]+1;
		while (*t)
			*s++ = tolower(*t++);
		*s = ':';

		cb = header;
		argc--;
		argv++;
	}

	if (argc == 1 && isatty(0))
		blaze822_loop1(".", cb);
	else
		blaze822_loop(argc-1, argv+1, cb);
	
	return 0;
}
