#include <sys/stat.h>
#include <sys/types.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <search.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "blaze822.h"

static int cflag;
static int rflag;

int gen_b64(uint8_t *s, off_t size)
{
	static char *b64 =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	off_t i;
	int l;
	uint32_t v;
	for (i = 0, l = 0; i+2 < size; i += 3) {
		v = (s[i] << 16) | (s[i+1] << 8) | s[i+2];
		putc_unlocked(b64[(v & 0xfc0000) >> 18], stdout);
		putc_unlocked(b64[(v & 0x03f000) >> 12], stdout);
		putc_unlocked(b64[(v & 0x000fc0) >>  6], stdout);
		putc_unlocked(b64[(v & 0x3f)], stdout);
		l += 4;
		if (l > 72) {
			l = 0;
			printf("\n");
		}
	}
	if (size - i == 2) { // 2 bytes left, XXX=
		v = (s[size - 2] << 16) | (s[size - 1] << 8);
		putc_unlocked(b64[(v & 0xfc0000) >> 18], stdout);
		putc_unlocked(b64[(v & 0x03f000) >> 12], stdout);
		putc_unlocked(b64[(v & 0x000fc0) >>  6], stdout);
		putc_unlocked('=', stdout);
	} else if (size - i == 1) { // 1 byte left, XX==
		v = s[size - 1] << 16;
		putc_unlocked(b64[(v & 0xfc0000) >> 18], stdout);
		putc_unlocked(b64[(v & 0x03f000) >> 12], stdout);
		putc_unlocked('=', stdout);
		putc_unlocked('=', stdout);
	}
	printf("\n");
	return 0;
}

int gen_qp(uint8_t *s, off_t size, int maxlinelen, int linelen)
{
	off_t i;
	int header = linelen > 0;
	char prev = 0;

	for (i = 0; i < size; i++) {
		if ((s[i] > 126) ||
		    (s[i] < 32 && s[i] != '\n' && s[i] != '\t') ||
		    (s[i] == '=')) {
			printf("=%02X", s[i]);
			linelen += 3;
			prev = s[i];
		} else if (header &&
			   (s[i] == '\n' || s[i] == '\t' || s[i] == '_')) {
			printf("=%02X", s[i]);
			linelen += 3;
			prev = '_';
		} else if (header && s[i] == ' ') {
			putc_unlocked('_', stdout);
			linelen++;
			prev = '_';
		} else if (s[i] == '\n') {
			if (prev == ' ' || prev == '\t')
				puts("=");
			putc_unlocked('\n', stdout);
			linelen = 0;
			prev = 0;
		} else {
			putc_unlocked(s[i], stdout);
			linelen++;
			prev = s[i];
		}
		
		if (linelen >= maxlinelen-3-!!header) {
			linelen = 0;
			prev = '\n';
			if (header) {
				printf("?=\n =?UTF-8?Q?");
				linelen += 11;
			} else {
				puts("=");
			}
		}
	}
	if (linelen > 0 && !header)
		puts("=");
	return linelen;
}

static const char *
basenam(const char *s)
{
	char *r = strrchr(s, '/');
	return r ? r + 1 : s;
}

static void
gen_attachment(const char *filename)
{
	const char *s = filename;

	for (s = (char *) filename; *s; s++)
		if (*s <= 32 || *s >= 127 || s - filename > 35)
			goto rfc2231;

	printf("Content-Disposition: attachment; filename=\"%s\"\n", filename);
	return;

rfc2231:

	printf("Content-Disposition: attachment");
	int i = 0;
	int d = 0;

	s = filename;

	while (*s) {
		i = printf(";\n filename*%d*=", d);
		if (d++ == 0) {
			printf("UTF-8''");
			i += 7;
		}
		while (*s && i < 78 - 3) {
			if (*s <= 32 || *s > 126)
				i += printf("%%%02x", (uint8_t) *s++);
			else
				i += printf("%c", (uint8_t) *s++);
		}
	}

	printf("\n");
}

int
gen_file(char *file, char *ct)
{
	uint8_t *content;
	off_t size;

	int r = slurp(file, (char **)&content, &size);
	if (r != 0) {
		fprintf(stderr, "mmime: error attaching file '%s': %s\n",
		    file, strerror(r));
		return -1;
	}
	
	off_t bithigh = 0;
	off_t bitlow = 0;
	off_t linelen = 0;
	off_t maxlinelen = 0;
	off_t i;
	for (i = 0; i < size; i++) {
		if (content[i] == '\n') {
			if (maxlinelen < linelen)
				maxlinelen = linelen;
			linelen = 0;
		} else {
			linelen++;
		}
		if (content[i] != '\t' && content[i] != '\n' && content[i] < 32)
			bitlow++;
		if (content[i] > 127)
			bithigh++;
	}

	gen_attachment(basenam(file));

	if (bitlow == 0 && bithigh == 0 &&
	    maxlinelen <= 78 && content[size-1] == '\n') {
		if (!ct)
			ct = "text/plain";
		printf("Content-Type: %s\n", ct);
		printf("Content-Transfer-Encoding: 7bit\n\n");
		fwrite(content, 1, size, stdout);
		return 0;
	} else if (bitlow == 0 && bithigh == 0) {
		if (!ct)
			ct = "text/plain";
		printf("Content-Type: %s\n", ct);
		printf("Content-Transfer-Encoding: quoted-printable\n\n");
		gen_qp(content, size, 78, 0);
		return 0;
	} else if (bitlow > size/10 || bithigh > size/4) {
		if (!ct)
			ct = "application/binary";
		printf("Content-Type: %s\n", ct);
		printf("Content-Transfer-Encoding: base64\n\n");
		return gen_b64(content, size);
	} else {
		if (!ct)
			ct = "text/plain";
		printf("Content-Type: %s\n", ct);
		printf("Content-Transfer-Encoding: quoted-printable\n\n");
		gen_qp(content, size, 78, 0);
		return 0;
	}
}

void
print_header(char *line) {
	char *s, *e;
	size_t l = strlen(line);

	if (line[l-1] == '\n')
		line[l-1] = 0;

	/* iterate word-wise, encode words when needed. */

	s = line;

	if (!(*s == ' ' || *s == '\t')) {
		// raw header name
		while (*s && *s != ':')
			putc_unlocked(*s++, stdout);
		if (*s == ':')
			putc_unlocked(*s++, stdout);
	}

	int prevq = 0;  // was the previous word encoded as qp?

	int linelen = s - line;

	while (*s) {
		size_t highbit = 0;
		e = s;
		while (*e && *e == ' ')
			e++;
		for (; *e && *e != ' '; e++) {
			if ((uint8_t) *e >= 127)
				highbit++;
		}

		if (!highbit) {
			if (e-s >= 78)
				goto force_qp;
			if (e-s >= 78 - linelen) {
				// wrap in advance before long word
				printf("\n");
				linelen = 0;
			}
			if (linelen <= 1 && s[0] == ' ' && s[1] == ' ') {
				// space at beginning of line
				goto force_qp;
			}
			if (*s != ' ') {
				printf(" ");
				linelen++;
			}
			fwrite(s, 1, e-s, stdout);
			linelen += e-s;
			prevq = 0;
		} else {
force_qp:
			if (!prevq && *s == ' ')
				s++;
			if (linelen >= 78 - 13 - 4 ||
			    (e-s < (78 - 13)/3 &&
			     e-s >= (78 - linelen - 13)/3)) {
				// wrap in advance
				printf("\n");
				linelen = 0;
			}
			printf(" =?UTF-8?Q?");
			linelen += 11;
			linelen = gen_qp((uint8_t *)s, e-s, 78, linelen);
			printf("?=");
			linelen += 2;
			prevq = 1;
		}
		s = e;
	}
	printf("\n");
}

int
gen_build()
{
	char sep[100];
	snprintf(sep, sizeof sep, "----_=_%08lx%08lx%08lx_=_",
	    lrand48(), lrand48(), lrand48());

	char *line = 0;
	size_t linelen = 0;
	int inheader = 1;
	int intext = 0;

	while (1) {
		int read = getdelim(&line, &linelen, '\n', stdin);
		if (read == -1) {
			if (feof(stdin))
				break;
			else
				exit(1);
		}
		if (inheader) {
			if (line[0] == '\n') {
				inheader = 0;
				printf("MIME-Version: 1.0\n");
				if (rflag) {
					printf("Content-Type: text/plain; charset=UTF-8\n");
					printf("Content-Transfer-Encoding: quoted-printable\n\n");

				} else {
					printf("Content-Type: multipart/mixed; boundary=\"%s\"\n", sep);
					printf("\n");
					printf("This is a multipart message in MIME format.\n");
				}
			} else {
				print_header(line);
			}
			continue;
		}

		if (!rflag && line[0] == '#') {
			char *f = strchr(line, ' ');
			if (f) {
				char of = *f;
				*f = 0;
				if (strchr(line, '/')) {
					printf("\n--%s\n", sep);
					if (line[read-1] == '\n')
						line[read-1] = 0;
					gen_file(f+1, (char *)line+1);
					intext = 0;
					continue;
				}
				*f = of;
			}
		}

		if (!rflag && !intext) {
			printf("\n--%s\n", sep);
			printf("Content-Type: text/plain; charset=UTF-8\n");
			printf("Content-Disposition: inline\n");
			printf("Content-Transfer-Encoding: quoted-printable\n\n");
			
			intext = 1;
		}

		gen_qp((uint8_t *)line, strlen(line), 78, 0);
	}
	if (!rflag && !inheader)
		printf("\n--%s--\n", sep);

	free(line);
	return 0;
}

int
check()
{
	off_t bithigh = 0;
	off_t bitlow = 0;
	off_t linelen = 0;
	off_t maxlinelen = 0;

	int c;
	int l = -1;

	while ((c = getchar()) != EOF) {
		if (c == '\n') {
			if (maxlinelen < linelen)
				maxlinelen = linelen;
			linelen = 0;
		} else {
			linelen++;
		}
		if (c != '\t' && c != '\n' && c < 32)
			bitlow++;
		if (c > 127)
			bithigh++;
		l = c;
	}

	if (bitlow == 0 && bithigh == 0 && maxlinelen <= 78 && l == '\n')
		return 0;
	else
		return 1;
}

int
main(int argc, char *argv[])
{
	srand48(time(0) ^ getpid());

	int c;
	while ((c = getopt(argc, argv, "cr")) != -1)
		switch(c) {
		case 'r': rflag = 1; break;
		case 'c': cflag = 1; break;
		default:
		usage:
			fprintf(stderr, "Usage: mmime [-c|-r] < message\n");
			exit(1);
		}

	if (argc != optind)
		goto usage;

	if (cflag)
		return check();

	return gen_build();
}
