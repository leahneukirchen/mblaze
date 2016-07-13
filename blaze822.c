// memmem
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "blaze822.h"
#include "blaze822_priv.h"

#define bufsiz 4096

static long
parse_posint(char **s, size_t minn, size_t maxn)
{
        long n;
        char *end;

        errno = 0;
        n = strtol(*s, &end, 10);
        if (errno) {
//                perror("strtol");
		return -1;
        }
        if (n < (long)minn || n > (long)maxn) {
//                fprintf(stderr, "number outside %zd <= n < %zd\n", minn, maxn);
		return -1;
        }
        *s = end;
        return n;
}

time_t
blaze822_date(char *s) {
	struct tm tm;
	int c;

#if 0
#define i4(m) (s[0] && (s[0]|0x20) == m[0] && \
               s[1] && (s[1]|0x20) == m[1] && \
               s[2] && (s[2]|0x20) == m[2] && \
               s[3] && (s[3]|0x20) == m[3] && (s = s+4) )

#define i3(m) (s[0] && (s[0]|0x20) == m[0] && \
               s[1] && (s[1]|0x20) == m[1] && \
               s[2] && (s[2]|0x20) == m[2] && (s = s+3) )
#endif

#define i4(m) (((uint32_t) m[0]<<24 | m[1]<<16 | m[2]<<8 | m[3]) == \
	       ((uint32_t) s[0]<<24 | s[1]<<16 | s[2]<<8 | s[3] | 0x20202020) \
	       && (s += 4))

#define i3(m) (((uint32_t) m[0]<<24 | m[1]<<16 | m[2]<<8) == \
	       ((uint32_t) s[0]<<24 | s[1]<<16 | s[2]<<8 | 0x20202000) \
	       && (s += 3))

	while (iswsp(*s))
		s++;
	if (i4("mon,") || i4("tue,") || i4("wed,") || i4("thu,") ||
	    i4("fri,") || i4("sat,") || i4("sun,"))
		while (iswsp(*s))
			s++;

	if ((c = parse_posint(&s, 1, 31)) < 0) goto fail;
	tm.tm_mday = c;

	while (iswsp(*s))
		s++;
	
	if      (i3("jan")) tm.tm_mon = 0;
	else if (i3("feb")) tm.tm_mon = 1;
	else if (i3("mar")) tm.tm_mon = 2;
	else if (i3("apr")) tm.tm_mon = 3;
	else if (i3("may")) tm.tm_mon = 4;
	else if (i3("jul")) tm.tm_mon = 5;
	else if (i3("jun")) tm.tm_mon = 6;
	else if (i3("aug")) tm.tm_mon = 7;
	else if (i3("sep")) tm.tm_mon = 8;
	else if (i3("oct")) tm.tm_mon = 9;
	else if (i3("nov")) tm.tm_mon = 10;
	else if (i3("dec")) tm.tm_mon = 11;
	else goto fail;

	while (iswsp(*s))
		s++;
	
	if ((c = parse_posint(&s, 1000, 9999)) > 0) {
		tm.tm_year = c - 1900;
	} else if ((c = parse_posint(&s, 0, 49)) > 0) {
		tm.tm_year = c + 100;
	} else if ((c = parse_posint(&s, 50, 99)) > 0) {
		tm.tm_year = c;
	} else goto fail;

	while (iswsp(*s))
		s++;

	if ((c = parse_posint(&s, 0, 24)) < 0) goto fail;
	tm.tm_hour = c;
	if (*s++ != ':') goto fail;
	if ((c = parse_posint(&s, 0, 59)) < 0) goto fail;
	tm.tm_min = c;
	if (*s++ == ':') {
		if ((c = parse_posint(&s, 0, 61)) < 0) goto fail;
		tm.tm_sec = c;
	} else {
		tm.tm_sec = 0;
	}

	while (iswsp(*s))
		s++;

	if (*s == '+' || *s == '-') {
		int neg = (*s == '-');
		s++;
		if ((c = parse_posint(&s, 0, 10000)) < 0) goto fail;
		if (neg) {
                        tm.tm_hour += c / 100;
                        tm.tm_min  += c % 100;
		} else {
                        tm.tm_hour -= c / 100;
                        tm.tm_min  -= c % 100;
                }
	}

	tm.tm_isdst = -1;

	time_t r = mktime(&tm);
	return r;

fail:
	return -1;
}

char *
blaze822_addr(char *s, char **dispo, char **addro)
{
	static char disp[1024];
	static char addr[1024];
//	char *disp = disp+sizeof disp;
//	char *addr = addr+sizeof addr;
	char *c, *e;

//	printf("RAW : |%s|\n", s);
	
	while (iswsp(*s))
		s++;
	
	c = disp;
	e = disp + sizeof disp;

	*disp = 0;
	*addr = 0;

	while (*s) {
		if (*s == '<') {
			char *c = addr;
			char *e = addr + sizeof addr;

			s++;
			while (*s && c < e && *s != '>')
				*c++ = *s++;
			if (*s == '>')
				s++;
			*c = 0;
		} else if (*s == '"') {
			s++;
			while (*s && c < e && *s != '"')
				*c++ = *s++;
			if (*s == '"')
				s++;
		} else if (*s == '(') {
			s++;

			if (!*addr) {   // assume: user@host (name)
				*c-- = 0;
				while (c > disp && iswsp(*c))
					*c-- = 0;
				strcpy(addr, disp);
				c = disp;
				*c = 0;
			}

			while (*s && c < e && *s != ')')
				*c++ = *s++;
			if (*s == ')')
				s++;
		} else if (*s == ',') {
			s++;
			break;
		} else {
			*c++ = *s++;
		}
	}

	*c-- = 0;
	// strip trailing ws
	while (c > disp && iswsp(*c))
		*c-- = 0;

	if (*disp && !*addr && strchr(disp, '@')) {
		// just mail address was given
		strcpy(addr, disp);
		*disp = 0;
	}

//	printf("DISP :: |%s|\n", disp);
//	printf("ADDR :: |%s|\n", addr);

	if (dispo) *dispo = disp;
	if (addro) *addro = addr;

	return s;
}

static void
unfold_hdr(char *buf, char *end)
{
	char *s = buf;

	while (s < end && *s != ':') {
		*s = lc(*s);
		s++;
	}

	for (; s < end; s++) {
		if (*s == 0)   // sanitize nul bytes in headers
			*s = ' ';

		if (*s == '\r') {
			if (*(s+1) == '\n') {
				*s++ = '\n';
			} else {
				*s = ' ';
			}
		}

		if (iswsp(*s)) {
			// change prior \n to spaces
			int j;
			for (j = 1; s - j >= buf && *(s-j) == '\n'; j++)
				*(s-j) = ' ';
		}

		if (*s == '\n') {
			s++;
			if (iswsp(*s)) {
				*(s-1) = ' ';
			} else {
				*(s-1) = 0;
				if (s-2 > buf && *(s-2) == '\n')   // ex-crlf
					*(s-2) = 0;
				while (s < end && *s != ':') {
					*s = lc(*s);
					s++;
				}
			}
		}
	}
}

struct message *
blaze822(char *file)
{
	int fd;
	ssize_t rd;
	char *buf;
	ssize_t bufalloc;
	ssize_t used;
	char *end;

	struct message *mesg = malloc(sizeof (struct message));
	if (!mesg)
		return 0;

	fd = open(file, O_RDONLY);
	if (fd < 0) {
//		perror("open");
		return 0;
	}

	buf = 0;
	bufalloc = 0;
	used = 0;

	while (1) {
		int overlap = used > 3 ? 3 : 0;

		bufalloc += bufsiz;
		buf = realloc(buf, bufalloc);
		if (!buf) {
			free(mesg);
			close(fd);
			return 0;
		}

		rd = read(fd, buf+used, bufalloc-used);
		if (rd == 0) {
			end = buf+used;
			break;
		}
		if (rd < 0) {
			free(mesg);
			free(buf);
			close(fd);
			return 0;
		}

		if ((end = memmem(buf-overlap+used, rd+overlap, "\n\n", 2)) ||
		    (end = memmem(buf-overlap+used, rd+overlap, "\r\n\r\n", 4))) {
			used += rd;
			end++;
			break;
		}

		used += rd;
	}
	close(fd);

	*end = 0;   // dereferencing *end is safe

	unfold_hdr(buf, end);

	mesg->msg = buf;
	mesg->end = end;
	mesg->body = mesg->bodyend = mesg->bodychunk = 0;

	return mesg;
}


struct message *
blaze822_mem(char *src, size_t len)
{
	char *buf;
	char *end;

	struct message *mesg = malloc(sizeof (struct message));
	if (!mesg)
		return 0;

	end = memmem(src, len, "\n\n", 2);
	if (!end)
		end = memmem(src, len, "\r\n\r\n", 4);
	if (!end)
		end = src + len;

	size_t hlen = end - src;

	buf = malloc(hlen+1);
	if (!buf)
		return 0;
	memcpy(buf, src, hlen);

	end = buf+hlen+1;
	*end = 0;   // dereferencing *end is safe

	unfold_hdr(buf, end);

	mesg->msg = buf;
	mesg->end = end;
	mesg->body = src + hlen;
	mesg->bodyend = src + len;
	mesg->bodychunk = 0;   // src is not ours

	return mesg;
}

void
blaze822_free(struct message *mesg)
{
	free(mesg->msg);
	free(mesg->bodychunk);
	free(mesg);
}

char *
blaze822_hdr_(struct message *mesg, const char *hdr, size_t hdrlen)
{
	char *v;

	// special case: first header, no leading nul
	if (memcmp(mesg->msg, hdr+1, hdrlen-1) == 0)
		v = mesg->msg;
	else
		v = memmem(mesg->msg, mesg->end - mesg->msg, hdr, hdrlen);
	if (!v)
		return 0;
	v += hdrlen;
	while (*v && iswsp(*v))
		v++;
	return v;
}

int
blaze822_loop(int argc, char *argv[], void (*cb)(char *))
{
	char *line = 0;
	size_t linelen = 0;
	ssize_t rd;
	int i = 0;

	if (argc == 0 || (argc == 1 && strcmp(argv[0], "-") == 0)) {
		while ((rd = getdelim(&line, &linelen, '\n', stdin)) != -1) {
			if (line[rd-1] == '\n')
				line[rd-1] = 0;
			cb(line);
			i++;
		}
	} else {
		for (i = 0; i < argc; i++)
			cb(argv[i]);
	}

	return i;
}

struct message *
blaze822_file(char *file)
{
	int fd = open(file, O_RDONLY);
	if (fd < 0)
		return 0;

	struct stat st;
	if (fstat(fd, &st) < 0)
		goto error;

	size_t s = st.st_size;

	char *buf = malloc(s+1);
	if (read(fd, buf, s) < 0) {
		// XXX handle short reads?
		perror("read");
		goto error;
	}
	close(fd);

	buf[s] = 0;

	// XXX duplicate header in ram...
	struct message *mesg = blaze822_mem(buf, s);
	if (mesg)
		mesg->bodychunk = buf;
	return mesg;

error:
	close(fd);
	return 0;
}

char *
blaze822_body(struct message *mesg)
{
	return mesg->body;
}

size_t
blaze822_bodylen(struct message *mesg)
{
	if (!mesg->body || !mesg->bodyend)
		return 0;
	return mesg->bodyend - mesg->body;
}
