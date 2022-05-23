#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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
	if (errno)
		return -1;
	if (n < (long)minn || n > (long)maxn) {
		errno = ERANGE;
		return -1;
	}
	*s = end;
	return n;
}

time_t
blaze822_date(char *s) {
	struct tm tm;
	int c;

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
	else if (i3("jun")) tm.tm_mon = 5;
	else if (i3("jul")) tm.tm_mon = 6;
	else if (i3("aug")) tm.tm_mon = 7;
	else if (i3("sep")) tm.tm_mon = 8;
	else if (i3("oct")) tm.tm_mon = 9;
	else if (i3("nov")) tm.tm_mon = 10;
	else if (i3("dec")) tm.tm_mon = 11;
	else goto fail;

#undef i3
#undef i4

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

	time_t r = mytimegm(&tm);
	return r;

fail:
	return -1;
}

static char *
skip_comment(char *s)
{
	if (*s != '(')
		return s;

	long d = 0;
	do {
		if (!*s)
			break;
		else if (*s == '(')
			d++;
		else if (*s == ')')
			d--;
		s++;
	} while (d > 0);

	return s;
}

// always 0 terminates
// never writes more than dstmax to dst
// returns how many bytes were appended
static size_t
safe_append(char *dst, size_t dstmax, char *srcbeg, char *srcend)
{
	size_t srclen = srcend - srcbeg;
	size_t dstlen = strnlen(dst, dstmax);

	if (dstlen == dstmax)
		return 0;

	if (dstmax - dstlen < srclen + 1)
		srclen = dstmax - dstlen - 1;
	memcpy(dst + dstlen, srcbeg, srclen);
	dst[dstlen + srclen] = 0;

	return srclen;
}

static size_t
safe_append_space(char *dst, size_t dstmax)
{
	char sp[] = " ";
	char *se = sp + 1;

	return safe_append(dst, dstmax, sp, se);
}

char *
blaze822_addr(char *s, char **dispo, char **addro)
{
	static char disp[1024];
	static char addr[1024];

	memset(addr, 0, sizeof addr);
	memset(disp, 0, sizeof disp);

	char ttok[1024] = { 0 };
	char *tc = ttok;
	char *te = ttok + sizeof ttok;

	int not_addr = 0;

	while (1) {
		if (!*s || iswsp(*s) || *s == ',' || *s == ';') {
			if (tc != ttok) {
				if (!*addr && !not_addr && memchr(ttok, '@', tc - ttok)) {
					safe_append(addr, sizeof addr, ttok, tc);
				} else {
					if (*disp)
						safe_append_space(disp, sizeof disp);
					safe_append(disp, sizeof disp,
					    ttok, tc);
				}
				tc = ttok;
				*ttok = 0;
				not_addr = 0;
			}
			if (!*s) {
				if (!*addr && !*disp) {
					if (dispo) *dispo = 0;
					if (addro) *addro = 0;
					return 0;
				}
				break;
			}
			if (*s == ',' || *s == ';') {
				s++;
				if (*addr || *disp)
					break;
			} else {
				s++;
			}
		} else if (*s == '<') {
			char tok[1024] = { 0 };
			char *c = tok;
			char *e = tok + sizeof tok;
			s++;
			while (*s && c < e && *s != '>') {
				s = skip_comment(s);
				if (!*s) {
					break;
				} else if (*s == '"') {
					// local part may be quoted, allow all
					s++;
					while (*s && c < e && *s != '"') {
						if (*s == '\\')
							s++;
						*c++ = *s++;
					}
					if (*s == '"')
						s++;
				} else if (*s == '<' || *s == ':') {
					c = tok;
					s++;
				} else {
					if (iswsp(*s))
						s++;
					else
						*c++ = *s++;
				}
			}
			if (*s == '>')
				s++;
			if (*addr) {
				if (*disp)
					safe_append_space(disp, sizeof disp);
				safe_append(disp, sizeof disp,
				    addr, addr + strlen(addr));
			}
			*addr = 0;
			safe_append(addr, sizeof addr, tok, c);
		} else if (*s == '"') {
			char tok[1024] = { 0 };
			char *c = tok;
			char *e = tok + sizeof tok;
			s++;
			while (*s && c < e && *s != '"') {
				if (*s == '\\' && *(s+1))
					s++;
				*c++ = *s++;
			}
			if (*s == '"')
				s++;

			if (memchr(tok, '@', c - tok))
				not_addr = 1;  // @ inside "" is never an addr

			*tc = 0;
			tc += safe_append(ttok, sizeof ttok, tok, c);
		} else if (*s == '(') {
			char *z = skip_comment(s);
			if (!*disp && *addr)  // user@host (name)
				safe_append(disp, sizeof disp, s + 1,
				    *z ? z-1 : (*(z-1) == ')' ? z-1 : z));
			else if (*disp) {  // copy comment
				safe_append_space(disp, sizeof disp);
				safe_append(disp, sizeof disp, s, z);
			}
			s = z;
		} else if (*s == ':') {
			if (memchr(ttok, '[', tc - ttok)) {
				// in ipv6 address
				if (tc < te)
					*tc++ = *s++;
				else
					s++;
			} else {  // ignore group name and start over
				s++;
				tc = ttok;
				memset(addr, 0, sizeof addr);
				memset(disp, 0, sizeof disp);
				*ttok = 0;
				not_addr = 0;
			}
		} else {
			if (*s == '\\' && *(s+1))
				s++;
			if (tc < te)
				*tc++ = *s++;
			else
				s++;
		}
	}

	char *host = strrchr(addr, '@');
	char *u;
	if (host && (u = strpbrk(addr, "()<>[]:;@\\,\" \t")) && u < host) {
		// need to "-quote local-part

		ssize_t hlen = strlen(host);
		char addr2[sizeof addr];
		char *e = addr2 + sizeof addr2 - 1;
		char *t;

		u = addr;
		t = addr2;
		*t++ = '"';
		while (u < host && e - t > 2) {
			if (*u == '"' || *u == '\\')
				*t++ = '\\';
			*t++ = *u++;
		}
		*t++ = '"';
		if (e - t > hlen + 1) {
			memcpy(t, host, hlen);
			*(t + hlen) = 0;
			memcpy(addr, addr2, sizeof addr);
		}
	}

	if (dispo) *dispo = *disp ? disp : 0;
	if (addro) *addro = *addr ? addr : 0;

	return s;
}

static void
compress_hdr(char *s, char *end)
{
	char *t, *h;

	if ((t = h = strchr(s, '\n'))) {
		while (h < end && *h) {
			if (*h == '\n') {
				*t++ = ' ';
				while (*h && isfws(*h) && *(h+1))
					h++;
			}
			*t++ = *h++;
		}
		// remove trailing whitespace
		while (s < t && isfws(t[-1]))
			*--t = 0;
		// zero fill gap
		while (t < h)
			*t++ = 0;
	}
}


static void
unfold_hdr(char *buf, char *end)
{
	char *s, *l;
	*end = 0;

	// sanitize all nul in message headers, srsly
	if (memchr(buf, 0, end-buf))
		for (s = buf; s < end; s++)
			if (*s == 0)
				*s = ' ';

	// normalize crlf
	if (memchr(buf, '\r', end-buf))
		for (s = buf; s < end; s++)
			if (*s == '\r') {
				if (*(s+1) == '\n')
					*s = '\n';
				else
					*s = ' ';
			}

	l = buf;
	s = buf;

	while (s < end && *s != ':' && *s != '\n') {
		*s = lc(*s);
		s++;
	}

	while (s < end) {
		s = memchr(s+1, '\n', end-s);
		if (!s)
			break;

		while (s < end && *s == '\n')
			s++;
		if (!iswsp(*s)) {
			*(s-1) = 0;
			compress_hdr(l, s-1);
			l = s;
			while (s < end && *s != ':' && *s != '\n') {
				*s = lc(*s);
				s++;
			}
		}
	}
	compress_hdr(l, end);
}

static int
is_crlf(char *s, size_t len)
{
	char *firsteol = memchr(s, '\n', len);

	return firsteol && firsteol > s && firsteol[-1] == '\r';
}

struct message *
blaze822(char *file)
{
	int fd;
	int crlf;
	ssize_t rd;
	char *buf;
	ssize_t bufalloc;
	ssize_t used;
	char *end;

	struct message *mesg = malloc(sizeof (struct message));
	if (!mesg)
		return 0;

	if (strcmp(file, "/dev/stdin") == 0)
		fd = dup(0);
	else
		fd = open(file, O_RDONLY);
	if (fd < 0) {
		free(mesg);
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
		if (used == 0) {
			crlf = is_crlf(buf, rd);
		}

		if (crlf) {
			if ((end = mymemmem(buf-overlap+used, rd+overlap, "\r\n\r\n", 4))) {
				end++;
				end++;
				break;
			}
		} else {
			if ((end = mymemmem(buf-overlap+used, rd+overlap, "\n\n", 2))) {
				end++;
				break;
			}
		}

		used += rd;
	}
	close(fd);

	*end = 0;   // dereferencing *end is safe

	unfold_hdr(buf, end);

	mesg->msg = buf;
	mesg->end = end;
	mesg->body = mesg->bodyend = mesg->bodychunk = mesg->orig_header = 0;

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

	if (is_crlf(src, len)) {
		if ((end = mymemmem(src, len, "\r\n\r\n", 4)))
			mesg->body = end+4;
	} else {
		if ((end = mymemmem(src, len, "\n\n", 2)))
			mesg->body = end+2;
	}

	if (!end) {
		end = src + len;
		mesg->body = end;
		mesg->bodyend = end;
	}
	if (mesg->body)
		mesg->bodyend = src + len;

	size_t hlen = end - src;

	buf = malloc(hlen+1);
	if (!buf) {
		free(mesg);
		return 0;
	}
	memcpy(buf, src, hlen);

	end = buf+hlen;
	*end = 0;   // dereferencing *end is safe

	unfold_hdr(buf, end);

	mesg->msg = buf;
	mesg->end = end;
	mesg->bodychunk = 0;   // src is not ours
	mesg->orig_header = src;

	return mesg;
}

void
blaze822_free(struct message *mesg)
{
	if (!mesg)
		return;
	if (mesg->bodychunk == mesg->msg) {
		munmap(mesg->bodychunk, mesg->bodyend - mesg->msg);
	} else {
		free(mesg->msg);
		free(mesg->bodychunk);
	}
	free(mesg);
}

char *
blaze822_hdr_(struct message *mesg, const char *hdr, size_t hdrlen)
{
	char *v;

	if (hdrlen == 0 || hdrlen-1 >= (size_t)(mesg->end - mesg->msg))
		return 0;  // header too small for the key, probably empty

	// special case: first header, no leading nul
	if (memcmp(mesg->msg, hdr+1, hdrlen-1) == 0) {
		v = mesg->msg;
		hdrlen--;
	} else {
		v = mymemmem(mesg->msg, mesg->end - mesg->msg, hdr, hdrlen);
	}
	if (!v)
		return 0;
	v += hdrlen;
	while (*v && iswsp(*v))
		v++;
	return v;
}

char *
blaze822_chdr(struct message *mesg, const char *chdr)
{
	char hdr[256];
	char *c;

	size_t l = snprintf(hdr, sizeof hdr, "%c%s:", 0, chdr);
	for (c = hdr+1; *c; c++)
		*c = lc(*c);

	return blaze822_hdr_(mesg, hdr, l);
}

struct message *
blaze822_file(char *file)
{
	char *buf = 0;
	ssize_t rd = 0, n;

	int fd;
	if (strcmp(file, "/dev/stdin") == 0)
		fd = dup(0);
	else
		fd = open(file, O_RDONLY);
	if (fd < 0)
		return 0;

	struct stat st;
	if (fstat(fd, &st) < 0)
		goto error;

	if (S_ISFIFO(st.st_mode)) {  // unbounded read, grow buffer
		const ssize_t bufblk = 16384;
		ssize_t bufalloc = bufblk;
		buf = malloc(bufalloc);
		if (!buf)
			goto error;

		do {
			if (bufalloc < rd + bufblk) {
				bufalloc *= 2;
				buf = realloc(buf, bufalloc);
				if (!buf)
					goto error;
			}
			if ((n = read(fd, buf + rd, bufblk)) < 0) {
				if (errno == EINTR) {
					continue;
				} else {
					perror("read");
					goto error;
				}
			}
			rd += n;
		} while (n > 0);
	} else {  // file size known
		ssize_t s = st.st_size;

		buf = malloc(s+1);
		if (!buf)
			goto error;

		do {
			if ((n = read(fd, buf + rd, s - rd)) < 0) {
				if (errno == EINTR) {
					continue;
				} else {
					perror("read");
					goto error;
				}
			}
			rd += n;
		} while (rd < s && n > 0);
	}

	close(fd);

	buf[rd] = 0;

	// XXX duplicate header in ram...
	struct message *mesg = blaze822_mem(buf, rd);
	if (mesg)
		mesg->bodychunk = buf;
	return mesg;

error:
	close(fd);
	free(buf);
	return 0;
}

size_t
blaze822_headerlen(struct message *mesg)
{
	return mesg->end - mesg->msg;
}

char *
blaze822_body(struct message *mesg)
{
	return mesg->body;
}

char *
blaze822_orig_header(struct message *mesg)
{
	return mesg->orig_header;
}

size_t
blaze822_bodylen(struct message *mesg)
{
	if (!mesg->body || !mesg->bodyend)
		return 0;
	return mesg->bodyend - mesg->body;
}

char *
blaze822_next_header(struct message *mesg, char *prev)
{
	if (!prev) {
		prev = mesg->msg;
	} else {
		if (prev >= mesg->end)
			return 0;
		prev = prev + strlen(prev);
	}
	while (prev < mesg->end && *prev == 0)
		prev++;
	if (prev >= mesg->end)
		return 0;
	return prev;
}
