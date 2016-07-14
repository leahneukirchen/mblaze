#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <wchar.h>

#include "blaze822.h"

void
printhdr(char *hdr)
{
	int uc = 1;

	while (*hdr) {
		putc(uc ? toupper(*hdr) : *hdr, stdout);
		uc = (*hdr == '-');
		hdr++;
	}
	putc(' ', stdout);
}

void
show(char *file)
{
	struct message *msg;

	msg = blaze822_file(file);
	if (!msg)
		return;

	char fields[] = "\0from:\0subject:\0to:\0cc:\0date:\0\0";

	// XXX custom field formatting

	char *f, *v;
	for (f = fields; f[1]; f += strlen(f+1)+1) {
		v = blaze822_hdr_(msg, f, strlen(f+1)+1);
		if (v) {
			printhdr(f+1);
			printf("%s\n", v);
		}
	}

	printf("\n");

	fwrite(blaze822_body(msg), blaze822_bodylen(msg), 1, stdout);
	blaze822_free(msg);
}

int
main(int argc, char *argv[])
{
	blaze822_loop(argc-1, argv+1, show);

	return 0;
}
