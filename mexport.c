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
#include "xpledge.h"

static int Sflag;

static int status;

void
export(char *file)
{
	struct message *msg;

	while (*file == ' ' || *file == '\t')
		file++;

	FILE *infile = fopen(file, "r");
	if (!infile) {
		status = 1;
		fprintf(stderr, "mexport: error opening '%s': %s\n",
		    file, strerror(errno));
		return;
	}

	char from[1024] = "nobody";
	time_t date = 0;

	if (fseek(infile, 0L, SEEK_SET) && errno == ESPIPE) {
		date = time(0);
		memcpy(from, "stdin", 6);
	} else {
		msg = blaze822(file);
		if (!msg)
			return;

		char *v;
		if ((v = blaze822_hdr(msg, "return-path")) ||
		    (v = blaze822_hdr(msg, "x-envelope-from"))) {
			char *s = strchr(v, '<');
			if (s) {
				char *e = strchr(s, '>');
				if (e) {
					s++;
					snprintf(from, sizeof from, "%.*s",
					    (int)(e-s), s);
				}
			} else {  // return-path without <>
				snprintf(from, sizeof from, "%s", v);
			}
		}

		if ((v = blaze822_hdr(msg, "date"))) {
			date = blaze822_date(v);
		}
		blaze822_free(msg);
	}

	char *line = 0;
	size_t linelen = 0;

	printf("From %s %s", from, asctime(gmtime(&date)));

	int in_header = 1;
	int final_nl = 0;

	while (1) {
		errno = 0;
		ssize_t rd = getdelim(&line, &linelen, '\n', infile);
		if (rd == -1) {
			if (errno == 0)
				break;
			fprintf(stderr, "mexport: error reading '%s': %s\n",
			    file, strerror(errno));
			status = 1;
			return;
		}

		if (in_header && line[0] == '\n' && !line[1]) {
			if (Sflag) {
				char *flags = strstr(file, MAILDIR_COLON_SPEC_VER_COMMA);
				if (!flags)
					flags = "";

				fputs("Status: ", stdout);
				if (strchr(flags, 'S'))
					putchar('R');
				char *ee = strrchr(file, '/');
				if (!ee ||
				    !(ee >= file + 3 && ee[-3] == 'n' && ee[-2] == 'e' && ee[-1] == 'w'))
					putchar('O');
				putchar('\n');

				fputs("X-Status: ", stdout);
				if (strchr(flags, 'R')) putchar('A');
				if (strchr(flags, 'T')) putchar('D');
				if (strchr(flags, 'F')) putchar('F');
				putchar('\n');
			}

			in_header = 0;
		}

		// MBOXRD: add first > to >>..>>From
		char *s = line;
		while (*s == '>')
			s++;
		if (strncmp("From ", s, 5) == 0)
			putchar('>');

		fputs(line, stdout);
		final_nl = (line[rd-1] == '\n');
	}

	// ensure trailing newline
	if (!final_nl)
		putchar('\n');

	// ensure empty line at end of message
	putchar('\n');

	fclose(infile);
}

int
main(int argc, char *argv[])
{
	int c;
	while ((c = getopt(argc, argv, "S")) != -1)
		switch (c) {
		case 'S': Sflag = 1; break;
		default:
			fprintf(stderr, "Usage: mexport [-S] [msgs...]\n");
			exit(2);
		}

	status = 0;

	xpledge("stdio rpath", "");

	if (argc == optind && isatty(0))
		blaze822_loop1(":", export);
	else
		blaze822_loop(argc-optind, argv+optind, export);

	return status;
}
