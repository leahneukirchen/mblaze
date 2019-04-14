#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blaze822.h"

static int aflag;
static char defaulthflags[] = "from:sender:reply-to:to:cc:bcc:"
    "resent-from:resent-sender:resent-to:resent-cc:resent-bcc:";
static char *hflag = defaulthflags;

void
print_quoted(char *s)
{
	char *t;

	for (t = s; *t; t++)
                if ((unsigned char)*t < 32 || strchr("()<>[]:;@\\,.\"", *t))
			goto quote;

	printf("%s", s);
	return;

quote:
	putchar('"');
	for (t = s; *t; t++) {
		if (*t == '"' || *t == '\\')
			putchar('\\');
		putchar(*t);
	}
	putchar('"');

}

void
addr(char *file)
{
	while (*file == ' ' || *file == '\t')
		file++;

	struct message *msg = blaze822(file);
	if (!msg)
		return;

	char *h = hflag;
	char *v;
	while (*h) {
		char *n = strchr(h, ':');
		if (n)
			*n = 0;
		v = blaze822_chdr(msg, h);
		if (v) {
			char *disp, *addr;
			char vdec[16384];
			blaze822_decode_rfc2047(vdec, v, sizeof vdec - 1, "UTF-8");
			vdec[sizeof vdec - 1] = 0;
			v = vdec;

			while ((v = blaze822_addr(v, &disp, &addr))) {
				if (disp && addr && strcmp(disp, addr) == 0)
					disp = 0;
				if (disp && addr) {
					if (aflag) {
						printf("%s\n", addr);
					} else {
						print_quoted(disp);
						printf(" <%s>\n", addr);
					}
				} else if (addr) {
					printf("%s\n", addr);
				}
			}
		}
		if (n) {
			*n = ':';
			h = n + 1;
		} else {
			break;
		}
	}
	blaze822_free(msg);
}

int
main(int argc, char *argv[])
{
	int c;
	while ((c = getopt(argc, argv, "ah:")) != -1)
		switch (c) {
		case 'a': aflag = 1; break;
		case 'h': hflag = optarg; break;
		default:
			fprintf(stderr,
			    "Usage: maddr [-a] [-h headers] [msgs...]\n");
			exit(1);
		}

	if (argc == optind && isatty(0))
		blaze822_loop1(":", addr);
	else
		blaze822_loop(argc-optind, argv+optind, addr);

	return 0;
}
