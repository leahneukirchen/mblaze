#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
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
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "blaze822.h"
#include "xpledge.h"

static int cflag;
static int rflag;
static char *tflag = "multipart/mixed";

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

#define qphrasevalid(c) ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || \
                         (c >= 'a' && c <= 'z') || \
                         c == '!' || c == '*' || c == '+' || c == '-' || \
                         c == '/')

size_t
gen_qp(uint8_t *s, off_t size, size_t maxlinelen, size_t linelen)
{
	off_t i;
	int header = linelen > 0;
	char prev = 0;

	for (i = 0; i < size; i++) {
		// inspect utf8 sequence to not wrap in between multibyte
		int mb;
		if      ((s[i] & 0x80) == 0)    mb = 3;
		else if ((s[i] & 0xc0) == 0x80) mb = 3;
		else if ((s[i] & 0xe0) == 0xc0) mb = 6;
		else if ((s[i] & 0xf0) == 0xe0) mb = 9;
		else if ((s[i] & 0xf8) == 0xf0) mb = 12;
		else mb = 3;

		if (linelen >= maxlinelen-mb-!!header) {
			linelen = 0;
			prev = '\n';
			if (header) {
				printf("?=\n =?UTF-8?Q?");
				linelen += 11;
			} else {
				puts("=");
			}
		}

		if ((s[i] > 126) ||
		    (s[i] == '=') ||
		    (linelen == 0 &&
		     (strncmp((char *)s, "From ", 5) == 0 ||
	             (s[i] == '.' && i+1 < size &&
		      (s[i+1] == '\n' || s[i+1] == '\r'))))) {
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
		} else if (s[i] < 33 && s[i] != '\n') {
			if ((s[i] == ' ' || s[i] == '\t') &&
			    i+1 < size &&
			    (s[i+1] != '\n' && s[i+1] != '\r')) {
				putc_unlocked(s[i], stdout);
				linelen += 1;
				prev = s[i];
			} else {
				printf("=%02X", s[i]);
				linelen += 3;
				prev = '_';
			}
		} else if (s[i] == '\n') {
			if (prev == ' ' || prev == '\t')
				puts("=");
			putc_unlocked('\n', stdout);
			linelen = 0;
			prev = 0;
		} else if (header && !qphrasevalid(s[i])) {
			printf("=%02X", s[i]);
			linelen += 3;
			prev = '_';
		} else {
			putc_unlocked(s[i], stdout);
			linelen++;
			prev = s[i];
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
gen_attachment(const char *filename, char *content_disposition)
{
	const char *s = filename;
	int quote = 0;

	if (!*filename) {
		printf("Content-Disposition: %s\n", content_disposition);
		return;
	}

	for (s = (char *)filename; *s; s++) {
		if (*s < 32 || *s == '"' || *s >= 127 || s - filename > 35)
			goto rfc2231;
		if (strchr(" ()<>@,;:\\/[]?=", *s))
			quote = 1;
	}

	// filename SHOULD be an atom if possible
	printf("Content-Disposition: %s; filename=%s%s%s\n", content_disposition,
	    quote ? "\"" : "", filename, quote ? "\"" : "");
	return;

rfc2231:
	printf("Content-Disposition: %s", content_disposition);
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
			if (*s <= 32 || *s == '"' || *s > 126)
				i += printf("%%%02x", (uint8_t)*s++);
			else
				i += printf("%c", (uint8_t)*s++);
		}
	}

	printf("\n");
}

int
gen_file(char *file, char *ct)
{
	uint8_t *content;
	off_t size;

	char *cd = "attachment";
	char *s = strchr(ct, '#');
	if (s) {
		*s = 0;
		cd = s + 1;
	}

	const char *filename = basenam(file);
	s = strchr(file, '>');
	if (s) {
		*s = 0;
		filename = s + 1;
	}

	int r = slurp(file, (char **)&content, &size);
	if (r != 0) {
		fprintf(stderr, "mmime: error attaching file '%s': %s\n",
		    file, strerror(r));
		return -1;
	}

	if (strcmp(ct, "mblaze/raw") == 0)
		goto raw;

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

	gen_attachment(filename, cd);

	if (strcmp(ct, "message/rfc822") == 0) {
		printf("Content-Type: %s\n", ct);
		printf("Content-Transfer-Encoding: %dbit\n\n",
		    (bitlow > 0 || bithigh > 0) ? 8 : 7);
		fwrite(content, 1, size, stdout);
		return 0;
	}

	if (bitlow == 0 && bithigh == 0 &&
	    maxlinelen <= 78) {
		if (!ct)
			ct = "text/plain";
		printf("Content-Type: %s\n", ct);
		printf("Content-Transfer-Encoding: 7bit\n\n");
raw:
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

	if (l == 0)
		return;

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
	char prevqs = 0;  // was the previous word a quoted-string?

	ssize_t linelen = s - line;

	while (*s) {
		size_t highbit = 0;
		int qs = 0;
		e = s;
		while (*e && *e == ' ')
			e++;

		if (*e == '"') {  // scan quoted-string, encode at once
			s = e;
			for (e++; *e && *e != '"'; e++) {
				if (*e == '\\')
					e++;
				if ((uint8_t)*e >= 127)
					highbit++;
			}
			if (*e == '"')
				e++;
			qs = 1;
		} else {  // scan word
			while (*e && *e == ' ')
				e++;
			for (; *e && *e != ' '; e++) {
				if ((uint8_t)*e >= 127)
					highbit++;
			}
		}

		if (!highbit) {
			if (e-s >= 998)
				goto force_qp;
			if (e-s >= 78 - linelen && linelen > 0) {
				// wrap in advance before long word
				printf("\n");
				linelen = 0;
			}
			if (linelen <= 1 && s[0] == ' ' && s[1] == ' ') {
				// space at beginning of line
				goto force_qp;
			}
			if (*s != ' ' && !(prevqs && !prevq && *(s-1) != ' ')) {
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

			if (qs && *s == '"')
				s++;
			if (qs && e > s && *(e-1) == '"')
				e--;

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

			if (qs && *e == '"')
				e++;
		}
		prevqs = qs;
		s = e;
	}
	printf("\n");
}

static int
valid_content_type(char *s)
{
	int slash = 0;

	for (; *s; s++)
		if (*s == '/')
			slash++;
		else if (isalnum(*s) || *s == '-' || *s == '+' || *s == '.' ||
		    *s == ';' || *s == '=' || *s == '#')
			; /* ok */
		else
			return 0;

	return slash == 1;
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
	int emptybody = 1;
	int ret = 0;
	char *contenttype = 0;
	char *contenttransferenc = 0;

	while (1) {
		ssize_t read = getdelim(&line, &linelen, '\n', stdin);
		if (read == -1) {
			if (feof(stdin)) {
				if (!emptybody)
					break;
				line = strdup(inheader ? "\n" : "");
			} else { // errored
				exit(1);
			}
		}
		if (inheader) {
			if (line[0] == '\n') {
				inheader = 0;
				printf("MIME-Version: 1.0\n");
				if (rflag) {
					printf("Content-Type:%s", contenttype ? contenttype : " text/plain; charset=UTF-8\n");
					printf("Content-Transfer-Encoding:%s", contenttransferenc ? contenttransferenc : " quoted-printable\n");
					printf("\n");
				} else {
					printf("Content-Type: %s; boundary=\"%s\"\n", tflag, sep);
					printf("\n");
					printf("This is a multipart message in MIME format.\n");
				}
			} else {
				if (strncasecmp(line, "Content-Type:", 13) == 0) {
					free(contenttype);
					contenttype = strdup(line+13);
				} else if (strncasecmp(line, "Content-Transfer-Encoding:", 26) == 0) {
					free(contenttransferenc);
					contenttransferenc = strdup(line+26);
				} else {
					print_header(line);
				}
			}
			continue;
		}

		if (!rflag && line[0] == '#') {
			char *f = strchr(line, ' ');
			if (f) {
				char of = *f;
				*f = 0;
				if (valid_content_type(line+1)) {
					printf("\n--%s\n", sep);
					if (line[read-1] == '\n')
						line[read-1] = 0;
					if (gen_file(f+1, line+1) != 0)
						ret = 1;
					intext = 0;
					emptybody = 0;
					continue;
				}
				*f = of;
			}
		}

		if (!rflag && !intext) {
			printf("\n--%s\n", sep);
			printf("Content-Type:%s", contenttype ? contenttype : " text/plain; charset=UTF-8\n");
			printf("Content-Disposition: inline\n");
			printf("Content-Transfer-Encoding:%s", contenttransferenc ? contenttransferenc : " quoted-printable\n");
			printf("\n");

			intext = 1;
		}

		if (contenttransferenc)
			printf("%s", line);
		else
			gen_qp((uint8_t *)line, strlen(line), 78, 0);

		emptybody = 0;
	}
	if (!rflag && !inheader)
		printf("\n--%s--\n", sep);

	free(line);
	return ret;
}

int
check()
{
	off_t bithigh = 0;
	off_t bitlow = 0;
	off_t linelen = 0;
	off_t maxheadlinelen = 0;
	off_t maxbodylinelen = 0;
	off_t bodylinelenlimit = getenv("MBLAZE_RELAXED_MIME") ? 998 : 78;

	int c;
	int l = -1;

	while ((c = getchar()) != EOF) {
		if (c == '\n') {
			if (maxheadlinelen < linelen)
				maxheadlinelen = linelen;
			linelen = 0;
			if (l == '\n')
				break;
		} else {
			linelen++;
		}
		if (c != '\t' && c != '\n' && c < 32)
			bitlow++;
		if (c > 127)
			bithigh++;
		l = c;
	}

	while ((c = getchar()) != EOF) {
		if (c == '\n') {
			if (maxbodylinelen < linelen)
				maxbodylinelen = linelen;
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

	if (bitlow == 0 && bithigh == 0 &&
	    maxheadlinelen < 998 && maxbodylinelen <= bodylinelenlimit &&
	    l == '\n')
		return 0;
	else
		return 1;
}

int
main(int argc, char *argv[])
{
	srand48(time(0) ^ getpid());

	int c;
	while ((c = getopt(argc, argv, "crt:")) != -1)
		switch (c) {
		case 'r': rflag = 1; break;
		case 'c': cflag = 1; break;
		case 't': tflag = optarg; break;
		default:
usage:
			fprintf(stderr,
"Usage: mmime [-c|-r] [-t CONTENT-TYPE] < message\n");
			exit(1);
		}

	if (argc != optind)
		goto usage;

	xpledge("stdio rpath", "");

	if (cflag)
		return check();

	return gen_build();
}
