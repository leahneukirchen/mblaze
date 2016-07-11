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
header(char *hdr, size_t l, char *file)
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
	char *line = 0;
	size_t linelen = 0;
	int read;

	int i = 0;

	size_t l = strlen(argv[1])+2;
        char *hdr = malloc(l);
	hdr[0] = 0;
	char *s = hdr+1;
	char *t = argv[1];
	while (*t)
		*s++ = tolower(*t++);
	*s = ':';

	if (argc == 2 || (argc == 3 && strcmp(argv[1], "-") == 0)) {
		while ((read = getdelim(&line, &linelen, '\n', stdin)) != -1) {
			if (line[read-1] == '\n') line[read-1] = 0;
			header(hdr, l, line);
			i++;
		}
	} else {
		for (i = 2; i < argc; i++) {
			header(hdr, l, argv[i]);
		}
		i--;
	}

	printf("%d mails scanned\n", i);
	
	return 0;
}
