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
static char default_fflag[] = "%c%m %-3n %10d %17f %t %2i%s";
static char *fflag = default_fflag;

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

static char *
fmt_date(struct message *msg, int w, int iso)
{
	static char date[32];
	char *v;

	if (!msg)
		return "";

        if ((v = blaze822_hdr(msg, "date"))) {
		time_t t = blaze822_date(v);
		if (t != -1) {
			struct tm *tm;
			tm = localtime(&t);

			if (iso && w >= 19)
				strftime(date, sizeof date, "%Y-%m-%d %H:%M:%S", tm);
			else if (iso || tm->tm_year != curyear)
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

	return date;
}

static void
print_human(intmax_t i)
{
	double d = i / 1024.0;
	const char *u = "\0\0M\0G\0T\0P\0E\0Z\0Y\0";
	while (d >= 1024) {
		u += 2;
		d /= 1024.0;
	}

	if (d < 1.0)
		printf("%4.2f", d);
	else if (!*u)
		printf("%4.0f", d);
	else if (d < 10.0)
		printf("%3.1f%s", d, u);
	else
		printf("%3.0f%s", d, u);
}

void
oneline(char *file)
{
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

	char *e = file + strlen(file) - 1;
	while (file < e && (*e == ' ' || *e == '\t'))
		*e-- = 0;
	
	struct message *msg = blaze822(file);
	if (!msg)
		goto nomsg;

	char flag1, flag2, flag3;

	char *flags = strstr(file, ":2,");
        if (!flags)
		flags = "";
	else
		flags += 3;

	if (cur && strcmp(cur, file) == 0)
		flag1 = '>';
	else if (!strchr(flags, 'S'))
		flag1 = '.';
	else if (strchr(flags, 'T'))
		flag1 = 'x';
	else
		flag1 = ' ';

	if (strchr(flags, 'F'))
		flag2 = '*';
	else if (strchr(flags, 'R'))
		flag2 = '-';
	else
		flag2 = ' ';

        char *v;

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

	if (0) {
nomsg:
		flag1 = flag2 = flag3 = ' ';
		*fromdec = 0;
		snprintf(subjdec, sizeof subjdec, "\\_ %s", file);
		lineno = 0;
	}

	int wleft = cols;

	char *f;
	for (f = fflag; *f; f++) {
		if (*f != '%') {
			putchar(*f);
			wleft--;
			continue;
		}
		f++;

		int w = 0;
		if ((*f >= '0' && *f <= '9') || *f == '-') {
			errno = 0;
			char *e;
			w = strtol(f, &e, 10);
			if (errno != 0)
				w = 0;
			else
				f = e;
		}

		if (!*f)
			break;

		switch(*f) {
		case '%':
			putchar('%');
			wleft--;
			break;
		case 'c':
			putchar(flag1);
			wleft--;
			break;
		case 'm':
			putchar(flag2);
			wleft--;
			break;
		case 'M':
			if (!w) w = -3;
			wleft -= printf("%.*s", w, flags);
			break;
		case 'n':
			if (lineno)
				printf("%*ld", w, lineno);
			else
				printf("%*s", w, "");
			wleft -= w > 0 ? w : -w;
			break;
		case 'd':
		case 'D':
			if (!w) w = 10;
			wleft -= printf("%*s", w,
			    fmt_date(msg, w, Iflag || *f == 'D'));
			break;
		case 'f':
			u8putstr(stdout, fromdec, w ? w : 16, 1);
			wleft -= w > 0 ? w : -w;
			break;
		case 't':
			putchar(flag3);
			wleft--;
			break;
		case 'i':
			{
				int z;
				if (!w)
					w = 1;

				if (indent >= 10) {
					wleft -= printf("..%2d..", indent);
					indent = 10 - 6/w;
				}

				for (z = 0; z < w*indent; z++)
					putchar(' ');
				wleft -= w*indent;
			}
			break;
		case 's':
			if (!w) w = wleft;
			u8putstr(stdout, subjdec, wleft, 0);
			wleft -= w;
			break;
		case 'b':
			{
				struct stat st;
				if (stat(file, &st) != 0)
					st.st_size = 0;
				print_human(st.st_size);
				wleft -= 5;
			}
			break;
		case 'F':
			{
				if (!w) w = 10;
				char *e = file + strlen(file);
				while (file < e && *e != '/')
					e--;
				e--;
				while (file < e && *e != '/')
					e--;
				char *b = e - 1;
				while (file < b && *b != '/')
					b--;
				if (*b == '/')
					b++;
				if (*b == '.')
					b++;
				wleft -= printf("%*.*s", w, e-b, b);
			}
			break;
		default:
			putchar('%');
			putchar(*f);
			wleft -=2;
		}
	}

	printf("\n");

	blaze822_free(msg);
}

int
main(int argc, char *argv[])
{
	int c;
	while ((c = getopt(argc, argv, "If:n")) != -1)
		switch(c) {
		case 'I': Iflag++; break;
		case 'f': fflag = optarg; break;
		case 'n': nflag = 1; break;
		default:
			fprintf(stderr, "Usage: mscan [-n] [-f format] [-I] [msgs...]\n");
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
