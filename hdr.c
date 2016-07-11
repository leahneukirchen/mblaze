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

int
main(int argc, char *argv[])
{
	l = strlen(argv[1])+2;
        hdr = malloc(l);
	hdr[0] = 0;
	char *s = hdr+1;
	char *t = argv[1];
	while (*t)
		*s++ = tolower(*t++);
	*s = ':';

	int i = blaze822_loop(argc-2, argv+2, header);
	
	printf("%d mails scanned\n", i);
	
	return 0;
}
