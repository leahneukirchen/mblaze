#define _GNU_SOURCE

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <locale.h>

#include "blaze822.h"

static int cols;
static wchar_t replacement = L'?';
static char *cur;

void
u8putstr(FILE *out, char *s, size_t l, int pad)
{
	while (*s && l) {
		if (*s == '\t')
			*s = ' ';
		if (*s >= 32 && *s < 127) {
			putc(*s, out);
			s++;
			l--;
		} else {
			wchar_t wc;
			int r = mbtowc(&wc, s, 4);
			if (r < 0) {
				r = 1;
				wc = replacement;
			}
			s += r;
			fprintf(out, "%lc", wc);
			l -= wcwidth(wc);
		}
	}
	if (pad)
		while (l-- > 0)
			putc(' ', out);
}

void
oneline(char *file)
{
	int indent = 0;
	while (*file == ' ' || *file == '\t') {
		indent++;
		file++;
	}
	indent *= 2;

	char *e = file + strlen(file) - 1;
	while (file < e && (*e == ' ' || *e == '\t'))
		*e-- = 0;
	
	struct message *msg = blaze822(file);
	if (!msg) {
		int p = cols-38-3-indent;
		printf("%*.*s\\_ %*.*s\n", -38 - indent, 38 + indent, "",
		    -p, p, file);
		return;
	}

	char flag1, flag2;

	char *f = strstr(file, ":2,");
        if (!f)
		f = "";

	if (cur && strcmp(cur, file) == 0)
		flag1 = '>';
	else if (!strchr(f, 'S'))
		flag1 = '.';
	else if (strchr(f, 'T'))
		flag1 = 'x';
	else
		flag1 = ' ';

	if (strchr(f, 'F'))
		flag2 = '*';
	else if (strchr(f, 'R'))
		flag2 = '-';
	else
		flag2 = ' ';

	char date[16];
        char *v;

        if ((v = blaze822_hdr(msg, "date"))) {
		time_t t = blaze822_date(v);
		if (t != -1) {
			struct tm *tm;
			tm = localtime(&t);
			strftime(date, sizeof date, "%Y-%m-%d", tm);
		} else {
			strcpy(date, "(invalid)");
		}
	} else {
		strcpy(date, "(unknown)");
		// mtime perhaps?
	}


	char *from = "(unknown)";
        if ((v = blaze822_hdr(msg, "from"))) {
		char *disp, *addr;
		blaze822_addr(v, &disp, &addr);
		if (*disp)
			from = disp;
		else if (*addr)
			from = addr;
		else
			from = "(unknown)";
	}

	char fromdec[17];
	blaze822_decode_rfc2047(fromdec, from, sizeof fromdec - 1, "UTF-8");
	fromdec[sizeof fromdec - 1] = 0;

	char *subj = "(no subject)";
	char subjdec[100];
        if ((v = blaze822_hdr(msg, "subject"))) {
		subj = v;
	}
	blaze822_decode_rfc2047(subjdec, subj, sizeof subjdec - 1, "UTF-8");

	long lineno = blaze822_seq_find(file);
	if (lineno)
		printf("%c%c %-3ld %-10s  ", flag1, flag2, lineno, date);
	else
		printf("%c%c     %-10s  ", flag1, flag2, date);
	u8putstr(stdout, fromdec, 17, 1);
	printf("  ");
	int z;
	for (z = 0; z < indent; z++)
		printf(" ");
	u8putstr(stdout, subjdec, cols-38-indent, 0);
	printf("\n");

	blaze822_free(msg);
}

int
main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");  // for wcwidth later
	if (wcwidth(0xFFFD) > 0)
		replacement = 0xFFFD;

	struct winsize w;
	if (ioctl(1, TIOCGWINSZ, &w) == 0)
		cols = w.ws_col;
	if (getenv("COLUMNS"))
		cols = atoi(getenv("COLUMNS"));
	if (cols <= 40)
		cols = 80;

	char *seqmap = blaze822_seq_open(0);
	blaze822_seq_load(seqmap);
	cur = blaze822_seq_cur();

	int i = blaze822_loop(argc-1, argv+1, oneline);

	fprintf(stderr, "%d mails scanned\n", i);

	return 0;
}
