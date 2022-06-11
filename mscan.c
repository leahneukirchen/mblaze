#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#ifdef __sun
#define __EXTENSIONS__ /* to get TIOCGWINSZ */
#endif

#include "xpledge.h"

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <termios.h>

#include "blaze822.h"
#include "u8decode.h"

static int cols;
static wchar_t replacement = L'?';
static char *cur;

static char *aliases[32];
static int alias_idx;

static int Iflag;
static int nflag;
static int vflag;
static int curyear;
static time_t now;
static char default_fflag[] = "%c%u%r %-3n %10d %17f %t %2i%s";
static char *fflag = default_fflag;

ssize_t
u8putstr(FILE *out, char *s, ssize_t l, int pad)
{
	ssize_t ol = l;

	while (*s && l > 0) {
		if (*s == '\t')
			*s = ' ';

		if ((unsigned)*s < 32 || *s == 127) {  // C0
			fprintf(out, "%lc", (wint_t)(*s == 127 ? 0x2421 : 0x2400+*s));
			s++;
			l--;
		} else {
			uint32_t c;
			int r = u8decode(s, &c);
			if (r < 0) {
				r = 1;
				l--;
				fprintf(out, "%lc", (wint_t)replacement);
			} else {
				int w = wcwidth((wchar_t)c);
				if (w < 0)
					w = 2;   /* assume worst width */
				l -= w;
				if (l >= 0)
					fwrite(s, 1, r, out);
			}
			s += r;
		}
	}
	if (pad)
		while (l-- > 0)
			putc(' ', out);

	if (l < 0)
		l = 0;
	return ol - l;
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

	v = blaze822_hdr(msg, "date");
	if (!v)
		return "(unknown)";
	time_t t = blaze822_date(v);
	if (t == -1)
		return "(invalid)";

	struct tm *tm;
	tm = localtime(&t);

	if (iso) {
		if (w >= 19)
			strftime(date, sizeof date, "%Y-%m-%d %H:%M:%S", tm);
		else if (w >= 16)
			strftime(date, sizeof date, "%Y-%m-%d %H:%M", tm);
		else
			strftime(date, sizeof date, "%Y-%m-%d", tm);
	} else if (w >= 19) {
		// https://dotti.me/

		tm = gmtime(&t);
		strftime(date, sizeof date, "%Y-%m-%dT%H\xc2\xb7%M", tm);
		char *d = date + strlen(date);

		char *e = v + strlen(v);
		while (e > v && (*e != '+' && *e != '-'))
			e--;

		if (isdigit(e[1]) && isdigit(e[2]) && isdigit(e[3]) && isdigit(e[4])) {
			*d++ = e[0];
			*d++ = e[1];
			*d++ = e[2];
			if (e[3] != '0' || e[4] != '0') {
				*d++ = ':';
				*d++ = e[3];
				*d++ = e[4];
			}
			*d = 0;
		} else {
			*d++ = '+';
			*d++ = '0';
			*d++ = '0';
			*d = 0;
		}
	} else if (w < 10) {
		if (tm->tm_year != curyear)
			strftime(date, sizeof date, "%b%y", tm);
		else if (t > now || now - t > 86400)
			strftime(date, sizeof date, "%d%b", tm);
		else
			strftime(date, sizeof date, "%H:%M", tm);
	} else {
		if (tm->tm_year != curyear)
			strftime(date, sizeof date, "%Y-%m-%d", tm);
		else if (t > now || now - t > 86400)
			strftime(date, sizeof date, "%a %b %e", tm);
		else
			strftime(date, sizeof date, "%a  %H:%M", tm);
	}

	return date;
}

static char *
fmt_subject(struct message *msg, char *file, int strip)
{
	static char subjdec[100];
	char *subj = "(no subject)";
	char *v;

	if (!msg) {
		snprintf(subjdec, sizeof subjdec, "\\_ %s", file);
		return subjdec;
	}

	if ((v = blaze822_hdr(msg, "subject")))
		subj = v;

	blaze822_decode_rfc2047(subjdec, subj, sizeof subjdec - 1, "UTF-8");

	if (strip) {
		size_t i;
		for (i = 0; subjdec[i]; ) {
			if (subjdec[i] == ' ') {
				i++;
				continue;
			} else if (strncasecmp("re:", subjdec+i, 3) == 0 ||
				   strncasecmp("aw:", subjdec+i, 3) == 0) {
				i += 3;
				continue;
			} else if (strncasecmp("fwd:", subjdec+i, 4) == 0) {
				i += 4;
				continue;
			}
			break;
		}
		return subjdec + i;
	}

	return subjdec;
}

static char *
fmt_from(struct message *msg)
{
	static char fromdec[256];
	char *from = "(unknown)";
	char *v, *w;

	if (!msg)
		return "";

	if ((v = blaze822_hdr(msg, "from"))) {
		blaze822_decode_rfc2047(fromdec, v, sizeof fromdec - 1, "UTF-8");
		fromdec[sizeof fromdec - 1] = 0;
		from = fromdec;

		if (itsme(fromdec) && ((w = blaze822_hdr(msg, "to")))) {
			memcpy(fromdec, "TO:", 4);
			blaze822_decode_rfc2047(fromdec + 3, w,
			    sizeof fromdec - 1 - 3, "UTF-8");
			from = fromdec;
		} else {
			char *disp, *addr;
			blaze822_addr(fromdec, &disp, &addr);
			if (disp)
				from = disp;
			else if (addr)
				from = addr;
		}
	}

	return from;
}

static char *
fmt_to_flag(struct message *msg)
{
	char *v;

	if (!msg || !alias_idx)
		return " ";

	if ((v = blaze822_hdr(msg, "to")) && itsme(v))
		return ">";
	else if ((v = blaze822_hdr(msg, "cc")) && itsme(v))
		return "+";
	else if ((v = blaze822_hdr(msg, "resent-to")) && itsme(v))
		return ":";
	else if ((v = blaze822_hdr(msg, "from")) && itsme(v))
		return "<";
	else
		return " ";
}

static ssize_t
print_human(intmax_t i, int w)
{
	double d = i / 1024.0;
	const char *u = "\0\0M\0G\0T\0P\0E\0Z\0Y\0";
	while (d >= 1024) {
		u += 2;
		d /= 1024.0;
	}

	if (d < 1.0)
		return printf("%*.2f", w, d);
	else if (!*u)
		return printf("%*.0f", w, d);
	else if (d < 10.0)
		return printf("%*.1f%s", w-1, d, u);
	else
		return printf("%*.0f%s", w-1, d, u);
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

	struct message *msg = blaze822(file);
	char *flags = msg ? strstr(file, ":2,") : 0;
	if (!flags)
		flags = "";
	else
		flags += 3;

	int wleft = cols;

	char *f;
	for (f = fflag; *f; f++) {
		if (*f == '\\') {
			f++;
			switch (*f) {
			case 'n': putchar('\n'); wleft = cols; break;
			case 't': putchar('\t'); wleft -= (8 - wleft % 8); break;
			default:
				putchar('\\'); wleft--;
				putchar(*f); wleft--;
			}
			continue;
		}
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

		switch (*f) {
		case '%':
			putchar('%');
			wleft--;
			break;
		case 'c':
			if (cur && strcmp(cur, file) == 0)
				putchar('>');
			else
				putchar(' ');
			wleft--;
			break;
		case 'u':  // unseen
			if (strchr(flags, 'T'))
				putchar('x');
			else if (strchr(flags, 'F'))
				putchar('*');
			else if (msg && !strchr(flags, 'S'))
				putchar('.');
			else
				putchar(' ');
			wleft--;
			break;
		case 'r':  // replied
			if (strchr(flags, 'R'))
				putchar('-');
			else if (strchr(flags, 'P'))
				putchar(':');
			else
				putchar(' ');
			wleft--;
			break;
		case 't':  // to-flag
			wleft -= printf("%s", fmt_to_flag(msg));
			break;
		case 'M':  // raw Maildir flags
			if (!w) w = -3;
			wleft -= printf("%*s", w, flags);
			break;
		case 'n':
			{
				long lineno = msg ? blaze822_seq_find(file) : 0;

				if (lineno)
					wleft -= printf("%*ld", w, lineno);
				else
					wleft -= printf("%*s", w, "");
			}
			break;
		case 'd':
		case 'D':
			if (!w) w = 10;
			wleft -= printf("%*s", w,
			    fmt_date(msg, w, Iflag || *f == 'D'));
			break;
		case 'f':
			if (w < 0)
				w += wleft;
			if (w)
				wleft -= u8putstr(stdout,
				    fmt_from(msg), w, 1);
			else
				wleft -= u8putstr(stdout,
				    fmt_from(msg), wleft, 0);
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
		case 'S':
			if (w < 0)
				w += wleft;
			if (w)
				wleft -= u8putstr(stdout,
				    fmt_subject(msg, file, *f == 'S'), w, 1);
			else
				wleft -= u8putstr(stdout,
				    fmt_subject(msg, file, *f == 'S'), wleft, 0);
			break;
		case 'b':
			{
				struct stat st;
				if (msg) {
					if (stat(file, &st) != 0)
						st.st_size = 0;
					wleft -= print_human(st.st_size, w);
				} else {
					wleft -= printf("%.*s", w, "");
				}
			}
			break;
		case 'F':
			{
				char *e = file + strlen(file);

				if (!msg)
					goto empty;

				while (file < e && *e != '/')
					e--;
				if (file == e)
					goto empty;
				e--;
				while (file < e && *e != '/')
					e--;
				while (file < e && *e == '/')
					e--;
				if (file == e)
					goto empty;
				char *b = e;
				e++;
				while (file < b && *b != '/')
					b--;
				if (*b == '/')
					b++;
				if (*b == '.')
					b++;

				if (0) {
empty:
					b = e = "";
				}
				if (w) {
					if (w < 0)
						w = -w;
					wleft -= printf("%*.*s",
					    -w, (int)(e-b < w ? e-b : w), b);
				} else {
					wleft -= printf("%.*s", (int)(e-b), b);
				}
			}
			break;
		case 'R':
			if (w)
				wleft -= printf("%*.*s", w, w, file);
			else
				wleft -= printf("%s", file);
			break;
		case 'I':
			{
				char *m = msg ? blaze822_hdr(msg, "message-id") : 0;
				if (!m)
					m = "(unknown)";
				if (w)
					wleft -= printf("%*.*s", w, w, m);
				else
					wleft -= printf("%s", m);
			}
			break;
		default:
			putchar('%');
			putchar(*f);
			wleft -= 2;
		}
	}

	printf("\n");

	blaze822_free(msg);
}

int
main(int argc, char *argv[])
{
	pid_t pager_pid = -1;

	int c;
	while ((c = getopt(argc, argv, "If:nv")) != -1)
		switch (c) {
		case 'I': Iflag++; break;
		case 'f': fflag = optarg; break;
		case 'n': nflag = 1; break;
		case 'v': vflag = 1; break;
		default:
			fprintf(stderr, "Usage: mscan [-Inv] [-f format] [msgs...]\n");
			exit(1);
		}

	xpledge("stdio rpath tty proc exec", NULL);

	if (nflag) {
		if (argc == optind && isatty(0))
			blaze822_loop1(":", numline);
		else
			blaze822_loop(argc-optind, argv+optind, numline);
		return 0;
	}

	now = time(0);
	struct tm *tm = localtime(&now);
	curyear = tm->tm_year;

	setlocale(LC_ALL, "");  // for wcwidth later
	if (wcwidth(0xfffd) > 0)
		replacement = 0xfffd;

	struct winsize w;
	int ttyfd = open("/dev/tty", O_RDONLY | O_NOCTTY);
	if (ttyfd >= 0 && ioctl(ttyfd, TIOCGWINSZ, &w) == 0) {
		cols = w.ws_col;

		char *pg;
		pg = getenv("MBLAZE_PAGER");
		if (!pg)
			pg = getenv("PAGER");
		if (pg && *pg && strcmp(pg, "cat") != 0) {
			pager_pid = pipeto(pg);
			if (pager_pid < 0)
				fprintf(stderr,
				    "mscan: spawning pager '%s': %s\n",
				    pg, strerror(errno));
		}
	}
	if (ttyfd >= 0)
		close(ttyfd);

	xpledge("stdio rpath", "");

	if (getenv("COLUMNS"))
		cols = atoi(getenv("COLUMNS"));
	if (cols <= 40)
		cols = 80;

	char *f = blaze822_home_file("profile");
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
		if ((v = blaze822_hdr(config, "scan-format")))
			if (fflag == default_fflag)  // NB. ==
				fflag = v;
	}

	long i;
	if (argc == optind && isatty(0))
		i = blaze822_loop1(":", oneline);
	else
		i = blaze822_loop(argc-optind, argv+optind, oneline);

	if (pager_pid > 0)
		pipeclose(pager_pid);

	if (vflag)
		fprintf(stderr, "%ld mails scanned\n", i);

	return 0;
}
