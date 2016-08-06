#include <sys/mman.h>
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

int gen_qp(uint8_t *s, off_t size, int maxlinelen, int header)
{
	off_t i;
	int linelen = 0;
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
		
		if (linelen >= maxlinelen-3) {
			linelen = 0;
			prev = '\n';
			puts("=");
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

int gen_file(char *file, char *ct)
{
	int fd = open(file, O_RDONLY);
	if (!fd)
		return 0;

	struct stat st;

	fstat(fd, &st);
	uint8_t *content = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);

	if (content == MAP_FAILED)
		return -1;
	
	off_t bithigh = 0;
	off_t bitlow = 0;
	off_t linelen = 0;
	off_t maxlinelen = 0;
	off_t i;
	for (i = 0; i < st.st_size; i++) {
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

	printf("Content-Disposition: attachment; filename=\"%s\"\n",
	       basenam(file));
	if (bitlow == 0 && bithigh == 0 &&
	    maxlinelen <= 78 && content[st.st_size-1] == '\n') {
		if (!ct)
			ct = "text/plain";
		printf("Content-Type: %s\n", ct);
		printf("Content-Transfer-Encoding: 7bit\n\n");
		fwrite(content, 1, st.st_size, stdout);
		return 0;
	} else if (bitlow == 0 && bithigh == 0) {
		if (!ct)
			ct = "text/plain";
		printf("Content-Type: %s\n", ct);
		printf("Content-Transfer-Encoding: quoted-printable\n\n");
		gen_qp(content, st.st_size, 78, 0);
		return 0;
	} else if (bitlow > st.st_size/10 || bithigh > st.st_size/4) {
		if (!ct)
			ct = "application/binary";
		printf("Content-Type: %s\n", ct);
		printf("Content-Transfer-Encoding: base64\n\n");
		return gen_b64(content, st.st_size);
	} else {
		if (!ct)
			ct = "text/plain";
		printf("Content-Type: %s\n", ct);
		printf("Content-Transfer-Encoding: quoted-printable\n\n");
		gen_qp(content, st.st_size, 78, 0);
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

	int prevq = 0;

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
		// use qp for lines with high bit, for long lines, or
		// for more than two spaces
		if (highbit ||
		    e-s > 78-linelen ||
		    (prevq && s[0] == ' ' && s[1] == ' ')) {
			if (!prevq && s[0] == ' ')
				s++;

			// 13 = strlen(" =?UTF-8?Q??=")
			int w;
			while (s < e && (w = (78-linelen-13) / (highbit?3:1)) < e-s && w>0) {
				printf(" =?UTF-8?Q?");
				gen_qp((uint8_t *)s, w>e-s?e-s:w, 999, 1);
				printf("?=\n");
				s += w;
				linelen = 0;
				prevq = 1;
			}
			if (s < e) {
				if (linelen + (e-s)+13 > 78) {
					printf("\n ");
					linelen = 1;
				}
				if (highbit || s[0] == ' ') {
					printf("=?UTF-8?Q?");
					linelen += 13;
					linelen += gen_qp((uint8_t *)s, e-s, 999, 1);
					printf("?=");
					prevq = 1;
				} else {
					fwrite(s, 1, e-s, stdout);
					linelen += e-s;
					prevq = 0;
				}
			}
		} else {
			if (linelen + (e-s) > 78) {
				printf("\n");
				if (*s != ' ')
					printf(" ");
				linelen = 0;
			}
			fwrite(s, 1, e-s, stdout);
			linelen += e-s;
			prevq = 0;
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
		int read = getline(&line, &linelen, stdin);
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
			*f = 0;
			if (strchr(line, '/')) {
				printf("\n--%s\n", sep);
				if (line[read-1] == '\n')
					line[read-1] = 0;
				gen_file(f+1, (char *)line+1);
				intext = 0;
				continue;
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
