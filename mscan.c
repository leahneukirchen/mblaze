#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

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

static char *aliases[32];
static int alias_idx;

static int Iflag;
static int nflag;
static int curyear;
static int curyday;

void
u8putstr(FILE *out, char *s, ssize_t l, int pad)
{
	while (*s && l > 0) {
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
			l -= wcwidth(wc);
			if (l >= 0)
				fprintf(out, "%lc", wc);
		}
	}
	if (pad)
		while (l-- > 0)
			putc(' ', out);
}

int
itsme(char *v)
{
	int i;

	char *disp, *addr;
	while ((v = blaze822_addr(v, &disp, &addr)))
		for (i = 0; addr && i < alias_idx; i++)
			if (strcmp(aliases[i], addr) == 0)
				return 1;
	return 0;
}

static int init;

void
numline(char *file)
{
	if (!init) {
		// delay loading of the seq until we need to scan the first
		// file, in case someone in the pipe updated the map before
		char *seq = blaze822_seq_open(0);
		blaze822_seq_load(seq);
		cur = blaze822_seq_cur();
		init = 1;
	}

	while (*file == ' ' || *file == '\t')
		file++;

	long lineno = blaze822_seq_find(file);
	if (lineno)
		printf("%ld\n", lineno);
	else
		printf("%s\n", file);
}

void
oneline(char *file)
{
	int metawidth = 38;

	if (!init) {
		// delay loading of the seq until we need to scan the first
		// file, in case someone in the pipe updated the map before
		char *seq = blaze822_seq_open(0);
		blaze822_seq_load(seq);
		cur = blaze822_seq_cur();
		init = 1;
	}

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
		int p = cols-metawidth-3-indent;
		printf("%*.*s\\_ %*.*s\n", -metawidth - indent, metawidth + indent, "",
		    -p, p, file);
		return;
	}

	char flag1, flag2, flag3;

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

	char date[32];
        char *v;

        if ((v = blaze822_hdr(msg, "date"))) {
		time_t t = blaze822_date(v);
		if (t != -1) {
			struct tm *tm;
			tm = localtime(&t);

			if (Iflag > 1) {
				strftime(date, sizeof date,
					 "%Y-%m-%d %H:%M:%S", tm);
				metawidth += 9;
			} else if (Iflag || tm->tm_year != curyear)
				strftime(date, sizeof date, "%Y-%m-%d", tm);
			else if (tm->tm_yday != curyday)
				strftime(date, sizeof date, "%a %b %e", tm);
			else
				strftime(date, sizeof date, "%a  %H:%M", tm);
		} else {
			strcpy(date, "(invalid)");
		}
	} else {
		strcpy(date, "(unknown)");
		// mtime perhaps?
	}

	flag3 = ' ';
	if (alias_idx) {
		if ((v = blaze822_hdr(msg, "to")) && itsme(v))
			flag3 = '>';
		else if ((v = blaze822_hdr(msg, "cc")) && itsme(v))
			flag3 = '+';
		else if ((v = blaze822_hdr(msg, "resent-to")) && itsme(v))
			flag3 = ':';
	}

	char *from = "(unknown)";
	char to[256];

        if ((v = blaze822_hdr(msg, "from"))) {
		if (itsme(v)) {
			snprintf(to, sizeof to, "TO:%s", v);
			from = to;
			flag3 = '<';
		} else {
			char *disp, *addr;
			blaze822_addr(v, &disp, &addr);
			if (disp)
				from = disp;
			else if (addr)
				from = addr;
		}
	}

	char fromdec[64];
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
	u8putstr(stdout, fromdec, 16, 1);
	printf(" %c ", flag3);
	int z;
	if (indent > 18) {
		printf("..%2d..              ", indent/2);
		indent = 20;
	} else {
		for (z = 0; z < indent; z++)
			printf(" ");
	}
	u8putstr(stdout, subjdec, cols-metawidth-indent, 0);
	printf("\n");

	blaze822_free(msg);
}

int
main(int argc, char *argv[])
{
	int c;
	while ((c = getopt(argc, argv, "In")) != -1)
		switch(c) {
		case 'I': Iflag++; break;
		case 'n': nflag = 1; break;
		default:
			fprintf(stderr, "Usage: mscan [-n] [-I] [msgs...]\n");
			exit(1);
		}

	if (nflag) {
		if (argc == optind && isatty(0))
			blaze822_loop1(":", numline);
		else
			blaze822_loop(argc-optind, argv+optind, numline);
		return 0;
	}

	time_t now = time(0);
	struct tm *tm = localtime(&now);
	curyear = tm->tm_year;
	curyday = tm->tm_yday;

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

	char *f = blaze822_home_file(".mblaze/profile");
	struct message *config = blaze822(f);

	if (config) {
		char *v, *d, *a;
		if ((v = blaze822_hdr(config, "local-mailbox")))
			while (alias_idx < (int)(sizeof aliases / sizeof aliases[0]) &&
			       (v = blaze822_addr(v, &d, &a)))
				if (a)
					aliases[alias_idx++] = strdup(a);
		if ((v = blaze822_hdr(config, "alternate-mailboxes")))
			while (alias_idx < (int)(sizeof aliases / sizeof aliases[0]) &&
			       (v = blaze822_addr(v, &d, &a)))
				if (a)
					aliases[alias_idx++] = strdup(a);
	}

	long i;
	if (argc == optind && isatty(0))
		i = blaze822_loop1(":", oneline);
	else
		i = blaze822_loop(argc-optind, argv+optind, oneline);
	fprintf(stderr, "%ld mails scanned\n", i);

	return 0;
}
