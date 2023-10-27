#include <errno.h>
#include <iconv.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blaze822.h"
#include "blaze822_priv.h"

// XXX keep trying bytewise on invalid iconv

int
blaze822_decode_qp(char *start, char *stop, char **deco, size_t *decleno, int underscore)
{
	static signed char hex[] = {
		-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
		-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
		-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
		 0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,
		-1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
		-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
		-1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
		-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
	};

	char *buf = malloc(stop - start + 1);
	if (!buf)
		return 0;

	*deco = buf;

	char *s = start;
	while (s < stop) {
		if (*s == '=' && s[1] == '\n') {
			s += 2;
		} else if (*s == '=' && s[1] == '\r' && s[2] == '\n') {
			s += 3;
		} else if (*s == '=' && s+2 < stop) {
			unsigned char c1 = s[1];
			unsigned char c2 = s[2];
			s += 3;
			if (c1 > 127 || c2 > 127 || hex[c1] < 0 || hex[c2] < 0) {
				*buf++ = '=';
				*buf++ = c1;
				*buf++ = c2;
				continue;
			}
			*buf++ = (hex[c1] << 4) | hex[c2];
		} else if (underscore && *s == '_') {
			*buf++ = ' ';
			s++;
		} else {
			*buf++ = *s++;
		}
	}

	*buf = 0;

	*decleno = buf - *deco;
	return 1;
}

int
blaze822_decode_b64(char *s, char *e, char **deco, size_t *decleno)
{
	static signed char b64[128] = {
		-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
		-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
		-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
		52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1, 0,-1,-1,
		-1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
		15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
		-1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
		41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
	};

	char *buf = malloc((e - s) / 4 * 3 + 1);
	if (!buf)
		return 0;

	*deco = buf;

	while (s + 4 <= e) {
		uint32_t v = 0;
		unsigned char t = 0;

#define SKIP_WS \
		while (s < e && isfws((unsigned char)*s)) \
			s++; \
		if (s >= e) \
			break;

		SKIP_WS;
		unsigned char c0 = *s++;
		SKIP_WS;
		unsigned char c1 = *s++;
		SKIP_WS;
		unsigned char c2 = *s++;
		SKIP_WS;
		unsigned char c3 = *s++;

#undef SKIP_WS

		if ((c0 | c1 | c2 | c3) > 127)
			goto error;

		v |= b64[c0]; t |= b64[c0]; v <<= 6;
		v |= b64[c1]; t |= b64[c1]; v <<= 6;
		v |= b64[c2]; t |= b64[c2]; v <<= 6;
		v |= b64[c3]; t |= b64[c3];

		if (t >= 64) {
error:
			*buf++ = '?';
			*buf++ = '?';
			*buf++ = '?';
			continue;
		}

		char d2 = v & 0xff; v >>= 8;
		char d1 = v & 0xff; v >>= 8;
		char d0 = v & 0xff;

		if (c1 != '=') *buf++ = d0;
		if (c2 != '=') *buf++ = d1;
		if (c3 != '=') *buf++ = d2;
	}

	*buf = 0;

	*decleno = buf - *deco;
	return 1;
}

int
blaze822_decode_rfc2047(char *dst, char *src, size_t dlen, char *tgtenc)
{
	iconv_t ic = (iconv_t)-1;
	char *srcenc = 0;

	// need space for trailing nul
	if (dlen-- == 0)
		return 0;

	char *startdst = dst;
	size_t startdlen = dlen;

	char *b = src;

	// XXX use memmem
	char *s = strstr(src, "=?");
	if (!s)
		goto nocodeok;

	// keep track of partial multibyte sequences
	char *partial = 0;
	size_t partiallen = 0;

	do {
		char *t;
		t = b;
		while (t < s)  // strip space-only inbetween encoded words
			if (!isfws(*t++)) {
				if (partial)  // mixed up encodings
					goto nocode;
				while (b < s && dlen) {
					*dst++ = *b++;
					dlen--;
				}
				break;
			}

		if (!dlen)
			break;

		s += 2;

		char *e = strchr(s, '?');
		if (!e)
			goto nocode;

		*e = 0;
		if (!srcenc || strcmp(srcenc, s) != 0) {
			if (partial)  // mixed up encodings
				goto nocode;
			free(srcenc);
			srcenc = strdup(s);
			char *lang = strchr(srcenc, '*');
			if (lang)
				*lang = 0;  // kill RFC2231 language tag
			if (!srcenc)
				goto nocode;
			if (ic != (iconv_t)-1)
				iconv_close(ic);
			ic = iconv_open(tgtenc, srcenc);
		}
		*e = '?';
		e++;

		if (ic == (iconv_t)-1)
			goto nocode;

		char enc = lc(*e);
		e++;
		if (*e++ != '?')
			goto nocode;
		char *start = e;
		char *stop = strstr(e, "?=");
		if (!stop)
			goto nocode;

		char *dec = 0, *decchunk;
		size_t declen = 0;
		if (enc == 'q')
			blaze822_decode_qp(start, stop, &dec, &declen, 1);
		else if (enc == 'b')
			blaze822_decode_b64(start, stop, &dec, &declen);
		else
			goto nocode;

		if (partial) {
			dec = realloc(dec, declen + partiallen);
			if (!dec)
				goto nocode;
			memmove(dec + partiallen, dec, declen);
			memcpy(dec, partial, partiallen);
			declen += partiallen;
			free(partial);
			partial = 0;
			partiallen = 0;
		}

		decchunk = dec;
		ssize_t r = iconv(ic, &dec, &declen, &dst, &dlen);
		if (r < 0) {
			if (errno == E2BIG) {
				break;
			} else if (errno == EILSEQ) {
				goto nocode;
			} else if (errno == EINVAL) {
				partial = malloc(declen);
				if (!partial)
					goto nocode;
				memcpy(partial, dec, declen);
				partiallen = declen;
			} else {
				perror("iconv");
				goto nocode;
			}
		}

		while (!partial && declen && dlen) {
			*dst++ = *dec++;
			declen--;
			dlen--;
		}

		free(decchunk);

		b = stop + 2;
	} while (dlen && (s = strstr(b, "=?")));

	while (*b && dlen > 1) {
		*dst++ = *b++;
		dlen--;
	}

	if (memchr(startdst, 0, dst - startdst)) {
		dst = startdst;
		dlen = startdlen;
		goto nocodeok;
	}

	*dst = 0;

	if (ic != (iconv_t)-1)
		iconv_close(ic);
	free(srcenc);

	return 1;

nocode:
	fprintf(stderr, "error decoding rfc2047\n");
	if (ic != (iconv_t)-1)
		iconv_close(ic);
nocodeok:
	free(srcenc);
	while (*src && dlen > 1) {
		*dst++ = *src++;
		dlen--;
	}
	*dst = 0;

	return 1;
}

#ifdef TEST
int
main() {
	char *r;
	size_t l;
	char test[] = "Keld_J=F8rn_Simonsen";
	blaze822_decode_qp(test, test + sizeof test, &r, &l);
	printf("%s %d\n", r, l);

	char *r2;
	size_t l2;
	char test2[] = "SWYgeW91IGNhbiByZWFkIHRoaXMgeW8="; // dSB1bmRlcnN0YW5kIHRoZSBleGFtcGxlLg==";
	blaze822_decode_b64(test2, test2+sizeof test2, &r2, &l2);
	printf("%s %d\n", r2, l2);

	char test3[] = "=?ISO-8859-1?Q?Keld_J=F8rn_Simonsen?= <keld@dkuug.dk>";
	char test3dec[255];
	blaze822_decode_rfc2047(test3dec, test3, sizeof test3dec, "UTF-8");
	printf("%s\n", test3dec);

	char test4[] = "=?ISO-8859-1?B?SWYgeW91IGNhbiByZWFkIHRoaXMgeW8=?= "
	    "=?ISO-8859-2?B?dSB1bmRlcnN0YW5kIHRoZSBleGFtcGxlLg==?= z "
	    "=?ISO-8859-1?Q?a?= =?ISO-8859-2?Q?_b?=";
	char test4dec[255];
	blaze822_decode_rfc2047(test4dec, test4, sizeof test4dec, "UTF-8");
	printf("%s\n", test4dec);

	char test5[] = "=?UTF-8?Q?z=E2=80?= =?UTF-8?Q?=99z?=";
	char test5dec[255];
	blaze822_decode_rfc2047(test5dec, test5, sizeof test5dec, "UTF-8");
	printf("%s\n", test5dec);
}
#endif
