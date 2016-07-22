#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "blaze822.h"

static int nflag;
static int rflag;

int
main(int argc, char *argv[])
{
	int c;
	while ((c = getopt(argc, argv, "rn")) != -1)
		switch(c) {
		case 'n': nflag = 1; break;
		case 'r': rflag = 1; break;
		default:
			// XXX usage
			exit(1);
		}

	char *map = blaze822_seq_open(0);
	if (!map)
		return 1;

	int i;
	char *f;
	char *a;
	struct blaze822_seq_iter iter = { 0 };

	if (optind == argc && isatty(0)) {
		a = ":";
		i = argc;
		goto hack;
	}
	for (i = optind; i < argc; i++) {
		a = argv[i];
hack:
		if (strchr(a, '/')) {
			printf("%s\n", a);
			continue;
		}
		while ((f = blaze822_seq_next(map, a, &iter))) {
			if (nflag) {
				printf("%ld\n", iter.line-1);
			} else {
				char *s = f;
				if (rflag)
					while (*s == ' ' || *s == '\t')
						s++;
				printf("%s\n", s);
			}
			free(f);
		}
	}

	return 0;
}
