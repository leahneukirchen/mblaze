#ifdef __sun
#define __EXTENSIONS__ /* to get TIOCGWINSZ */
#endif

#include <sys/ioctl.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <termios.h>

#include "blaze822.h"
#include "xpledge.h"

int column = 0;
int maxcolumn = 80;

void
chgquote(int quotes)
{
	static int oquotes;

	if (quotes != oquotes) {
		if (column)
			putchar('\n');
		column = 0;
		oquotes = quotes;
	}
}

void
flowed(int quotes, char *line, ssize_t linelen)
{
	chgquote(quotes);
	int done = 0;

	while (!done) {
		if (column == 0) {
			for (; column < quotes; column++)
				putchar('>');
			column++;
			if (quotes && *line != ' ')
				putchar(' ');
		}

		char *eow;
		if (*line == ' ')
			eow = memchr(line + 1, ' ', linelen - 1);
		else
			eow = memchr(line, ' ', linelen);

		if (!eow) {
			eow = line + linelen;
			done = 1;
		}

		if (column + (eow - line) > maxcolumn &&
		    eow - line < maxcolumn &&
		    column - quotes > 1) {
			putchar('\n');
			column = 0;
			done = 0;
			if (*line == ' ') {
				line++;
				linelen--;
			}
		} else {
			fwrite(line, 1, eow - line, stdout);
			column += eow - line;
			linelen -= eow - line;
			line = eow;
		}
	}
}

void
fixed(int quotes, char *line, size_t linelen)
{
	flowed(quotes, line, linelen);

	putchar('\n');
	column = 0;
}

int
main(int argc, char *argv[])
{
	char *linebuf = 0;
	char *line;
	size_t linelen = 0;
	int outer_quotes = 0;
	int quotes;

	int reflow = 1;  // re-evaluated on $PIPE_CONTENTTYPE
	int force = 0;
	int delsp = 0;

	xpledge("stdio rpath tty", "");

	char *ct = getenv("PIPE_CONTENTTYPE");
	if (ct) {
		char *s, *se;
		reflow = 0;
		if (blaze822_mime_parameter(ct, "format", &s, &se) && s)
			reflow = (strncasecmp(s, "flowed", 6) == 0);
		if (blaze822_mime_parameter(ct, "delsp", &s, &se) && s)
			delsp = (strncasecmp(s, "yes", 3) == 0);
	}

	char *cols = getenv("COLUMNS");
	if (cols && isdigit(*cols)) {
		maxcolumn = atoi(cols);
	} else {
		struct winsize w;
		int fd = open("/dev/tty", O_RDONLY | O_NOCTTY);
		if (fd >= 0) {
			if (ioctl(fd, TIOCGWINSZ, &w) == 0)
				maxcolumn = w.ws_col;
			close(fd);
		}
	}

	xpledge("stdio", "");

	char *maxcols = getenv("MAXCOLUMNS");
	if (maxcols && isdigit(*maxcols)) {
		int m = atoi(maxcols);
		if (maxcolumn > m)
			maxcolumn = m;
	}

	int c;
	while ((c = getopt(argc, argv, "fqw:")) != -1)
		switch (c) {
		case 'f': force = 1; break;
		case 'q': outer_quotes++; break;
		case 'w': maxcolumn = atoi(optarg); break;
		default:
			fprintf(stderr, "Usage: mflow [-f] [-q] [-w MAXCOLUMNS]\n");
			exit(2);
		}

	while (1) {
		errno = 0;
		ssize_t rd = getdelim(&linebuf, &linelen, '\n', stdin);
		if (rd == -1) {
			if (errno == 0)
				break;
			fprintf(stderr, "mflow: error reading: %s\n",
			    strerror(errno));
			exit(1);
		}

		line = linebuf;

		if (!reflow && !force) {
			fwrite(line, 1, rd, stdout);
			continue;
		}

		if (rd > 0 && line[rd-1] == '\n')
			line[--rd] = 0;
		if (rd > 0 && line[rd-1] == '\r')
			line[--rd] = 0;

		quotes = outer_quotes;
		while (*line == '>') {  // measure quote depth
			line++;
			quotes++;
			rd--;
		}

		if (reflow && *line == ' ') {  // space stuffing
			line++;
			rd--;
		}

		if (strcmp(line, "-- ") == 0) {  // usenet signature convention
			if (column)
				fixed(quotes, "", 0);  // flush paragraph
			fixed(quotes, line, rd);
			continue;
		}

		if (reflow && rd > 0 && line[rd-1] == ' ') {  // flowed line
			if (delsp)
				line[--rd] = 0;
			flowed(quotes, line, rd);
		} else if (rd == 0) {  // empty line is fixed
			if (column > 0)
				putchar('\n');
			putchar('\n');
			column = 0;
		} else {
			if (force && rd > maxcolumn) {
				flowed(quotes, line, rd);
				fixed(quotes, "", 0);
			} else {
				fixed(quotes, line, rd);
			}
		}
	}

	if (reflow && column != 0)
		putchar('\n');
}
