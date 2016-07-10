#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#undef getc
#define getc getc_unlocked

#ifndef __GLIBC__
#include "/home/chris/src/musl/src/internal/stdio_impl.h"
#endif

FILE *input;

enum {
	EOL = -2,
	EOH = -3
};


//          WSP            =  SP / HTAB
#define iswsp(c)  (((c) == ' ' || (c) == '\t'))

// CRLF is \n, LF is \n, just CR is ' ',
// end of file is EOF, end of line is EOL, end of header is EOH,
// CRLFCR is EOH, CRLFCRLF is EOH, CRCR is EOH
int
mgetch()
{
	int c, d;

	switch(c = getc(input)) {
	case EOF:
		return EOF;
	case '\r':
		d = getc(input);
		if (d != '\n') {
			// stray CR
			ungetc(d, input);
			return ' ';
		}
		/* fallthru */
	case '\n':
		d = getc(input);
		if (d == '\r') {
			d = getc(input);
			if (d != '\n' && d != '\r')
				return d;
		}
		if (d == '\n' || d == '\r') {
			return EOH;
		} else if (iswsp(d)) {
			// line continuation
			return ' ';
		} else {
			// end of line
			ungetc(d, input);
			return EOL;
		}
	default:
		return c;
	}
}

char *
strip(char *t)
{
	char *b, *e;
	for (b = t; *b && iswsp(*b); b++);
	for (e = b; *e && !iswsp(*e); e++);
	*e = 0;
	return b;
}

void skipws() {
	int c;
	do {
		c = getc(input);
	} while (c != EOF && iswsp(c));
	ungetc(c, input);
}

char *
parse_header()
{
	static char hdr[1024];
	static const char *hdrend = hdr + sizeof hdr;
	int c;

	char *s = hdr;
	*s = 0;
	while ((c = mgetch()) > 0 && s < hdrend) {
		if (c < 0)
			return 0;
		if (c == ':') {
			*s = 0;
			skipws();
			return strip(hdr);
		}
		*s++ = tolower(c);
	}
	return 0;
}

char *
parse_value()
{
	static char value[1024];
	static const char *valend = value + sizeof value;

	int c;
	char *s = value;

	*s = 0;

	while ((c = mgetch()) != EOF && s < valend) {
		if (c == EOH) {
			ungetc('\n', input);
			return value;
		}
		if (c == EOL) {
			*s = 0;
			return value;
		}
		*s++ = c;
	}
	return value;
}

int
parse_tok(char **t)
{
	static char tok[16];
	static const char *tokend = tok + sizeof tok;

	int c;

	char *s = tok;
	*s = 0;
	*t = 0;

	c = mgetch();
	while (iswsp(c))
		c = mgetch();
	while (c > 0 && s < tokend) {
//		printf("%c/%d ", c, c);
		if (iswsp(c)) {
			skipws();
			*s = 0;
			*t = tok;
			break;
		}
		else
			*s++ = c;
		c = mgetch();
	}
	if (c == EOL) {
		*s = 0;
		*t = tok;
	}
	if (t)
		*t = strip(tok);

//	printf("TOK |%s|", tok);
	if (c < 0)
		return c;
	else {
		return 1;
	}
}

static long
parse_posint(char *s, size_t minn, size_t maxn)
{
        long n;
        char *end;

        errno = 0;
        n = strtol(s, &end, 10);
        if (errno || *end || n < (long)minn || n > (long)maxn)
		return -1;
        return n;
}

int
parse_date(time_t *r)
{
	char *t;
	int c;

	struct tm tm;
	
	if ((c = parse_tok(&t)) < 0) goto fail;
	if (strlen(t) == 4 && t[3] == ',')  // ignore day of week
		if ((c = parse_tok(&t)) < 0) goto fail;
	
	if ((c = parse_posint(t, 1, 31)) < 0) goto fail;
	tm.tm_mday = c;

	if ((c = parse_tok(&t)) < 0) goto fail;
	// convert to switch
	if      (strcmp("Jan", t) == 0) tm.tm_mon = 0;
	else if (strcmp("Feb", t) == 0) tm.tm_mon = 1;
	else if (strcmp("Mar", t) == 0) tm.tm_mon = 2;
	else if (strcmp("Apr", t) == 0) tm.tm_mon = 3;
	else if (strcmp("May", t) == 0) tm.tm_mon = 4;
	else if (strcmp("Jun", t) == 0) tm.tm_mon = 5;
	else if (strcmp("Jul", t) == 0) tm.tm_mon = 6;
	else if (strcmp("Aug", t) == 0) tm.tm_mon = 7;
	else if (strcmp("Sep", t) == 0) tm.tm_mon = 8;
	else if (strcmp("Oct", t) == 0) tm.tm_mon = 9;
	else if (strcmp("Nov", t) == 0) tm.tm_mon = 10;
	else if (strcmp("Dec", t) == 0) tm.tm_mon = 11;
	else goto fail;

	if ((c = parse_tok(&t)) < 0) goto fail;
	if ((c = parse_posint(t, 1000, 9999)) > 0) {
		tm.tm_year = c - 1900;
	} else if ((c = parse_posint(t, 0, 49)) > 0) {
		tm.tm_year = c + 100;
	} else if ((c = parse_posint(t, 50, 99)) > 0) {
		tm.tm_year = c;
	} else goto fail;

	if ((c = parse_tok(&t)) < 0)
		if (!t) goto fail;
	if (strlen(t) == 8 && t[2] == ':' && t[5] == ':') {
		t[2] = t[5] = 0;
		if ((c = parse_posint(t, 0, 24)) < 0) goto fail;
		tm.tm_hour = c;
		if ((c = parse_posint(t+3, 0, 59)) < 0) goto fail;
		tm.tm_min = c;
		if ((c = parse_posint(t+6, 0, 61)) < 0) goto fail;
		tm.tm_sec = c;
	} else if (strlen(t) == 5 && t[2] == ':') {
		t[2] = 0;
		if ((c = parse_posint(t, 0, 24)) < 0) goto fail;
		tm.tm_hour = c;
		if ((c = parse_posint(t+3, 0, 59)) < 0) goto fail;
		tm.tm_min = c;
		tm.tm_sec = 0;
	} else {
		goto fail;
	}

//	if ((c = parse_tok(&t)) > 0) goto fail;   // expect EOL, EOH, EOF
	c = parse_tok(&t);
	if (t && strlen(t) == 5 && (t[0] == '+' || t[0] == '-')) {
		if ((c = parse_posint(t+1, 0, 10000)) < 0) goto fail;
                if (t[0] == '+') {
                        tm.tm_hour -= c / 100;
                        tm.tm_min  -= c % 100;
                } else {
                        tm.tm_hour += c / 100;
                        tm.tm_min  += c % 100;
                }
	}
	// TODO: parse obs-zone?

	tm.tm_isdst = -1;

	*r = timegm(&tm);
	return 0;

fail:
	*r = -1;
	/* slurp rest of line and leave it */
        while ((c = mgetch()) > 0)
		;
	return c;
}

int decode_rfc2047 (char *str, char *dst, size_t dstlen);


int catmain() {
	int c;

	input = stdin;
	while ((c = mgetch())) {
		if (c > 0)
			putchar(c);
		else
			printf("[%d]\n", c);
	}

	return 0;
}

int tmain() {
	char *s;

	input = stdin;
	while ((s = parse_header())) {
		if (strcmp(s, "date") == 0) {
			time_t t;
			if (parse_date(&t) >= 0)
				printf("%s :: (%ld) %s", s, t, ctime(&t));
			else
				printf("date : OOPS\n");
		} else if (strcmp(s, "subject") == 0) {
			char *v = parse_value();
			char buf[2048];
			decode_rfc2047(v, buf, sizeof buf);
			printf("%s : %s\n", s, v);
			printf("%s :: %s\n", s, buf);
		} else {
			char *v = parse_value();
			printf("%s : %s\n", s, v);
		}
	}

	return 0;
}

int main() {
	char *s;
	
	char *line = 0;
	size_t linelen = 0;
	int read;

	int i = 0;
	
	static char buf[50000]; /* buf must survive until stdout is closed */

	while ((read = getdelim(&line, &linelen, '\n', stdin)) != -1) {
		if (line[read-1] == '\n') line[read-1] = 0;

		printf("%s\n", line);

		FILE *f = fopen(line, "r");
		if (!f) {
			perror("fopen");
			continue;
		}
		setvbuf ( f , buf , _IOFBF , sizeof(buf) );
		i++;

		input = f;
		while ((s = parse_header())) {
			if (strcmp(s, "date") == 0) {
				time_t t;
				if (parse_date(&t) >= 0)
					printf("%s :: (%ld) %s", s, t, ctime(&t));
				else
					printf("date : OOPS\n");
			} else if (strcmp(s, "subject") == 0 ||
			    strcmp(s, "from") == 0 ||
			    strcmp(s, "to") == 0) {
				char *v = parse_value();
				char buf[2048];
				if (decode_rfc2047(v, buf, sizeof buf))
					printf("%s :: %s\n", s, buf);
				else
					printf("%s : %s\n", s, v);
			} else
				parse_value();  // and ignore it
		}
		fclose(f);
	}

	printf("%d mails scanned\n", i);

	return 0;
}
