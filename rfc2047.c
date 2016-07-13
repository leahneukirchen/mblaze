#include <stdlib.h>
#include <errno.h>
#include <iconv.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "blaze822.h"
#include "blaze822_priv.h"

// XXX error detection on decode
// XXX keep trying bytewise on invalid iconv

int
blaze822_decode_qp(char *start, char *stop, char **deco, size_t *decleno)
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

	char *buf = malloc(4 * (stop - start) + 1);
	if (!buf)
		return 0;

	*deco = buf;

	char *s = start;
	size_t declen;

	while (s < stop) {
		if (*s == '=' && s[1] == '\n') {
			s += 2;
		} else if (*s == '=' && s+2 < stop) {
			*buf++ = (hex[s[1]] << 4) | hex[s[2]];
			s += 3;
		} else if (*s == '_') {
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

	char *buf = malloc((e - s) / 4 * 3);
	if (!buf)
		return 0;

	*deco = buf;

	while (s + 4 <= e) {
		while (s < e && isfws((unsigned char) *s))
			s++;
		if (s < e) {
			uint32_t v = 0;
			v |= b64[s[0]]; v <<= 6;
			v |= b64[s[1]]; v <<= 6;
			v |= b64[s[2]]; v <<= 6;
			v |= b64[s[3]];
			
			char d2 = v & 0xff; v >>= 8;
			char d1 = v & 0xff; v >>= 8;
			char d0 = v & 0xff;

			if (s[1] != '=') *buf++ = d0;
			if (s[2] != '=') *buf++ = d1;
			if (s[3] != '=') *buf++ = d2;
			
			s += 4;
		}
	}

	*decleno = buf - *deco;
	return 1;
}

int
blaze822_decode_rfc2047(char *dst, char *src, size_t dlen, char *tgtenc)
{
	iconv_t ic = (iconv_t)-1;

	char *b = src;

	// use memmem
	char *s = strstr(src, "=?");
	if (!s)
		goto nocodeok;

	do {
		char *t;
		t = b;
		while (t < s)  // strip space-only inbetween encoded words
			if (!isfws(*t++)) {
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
		ic = iconv_open(tgtenc, s);
		*e = '?';
		e++;

		if (ic == (iconv_t)-1)
			goto nocode;

		char enc = lc(*e++);
		if (*e++ != '?')
			goto nocode;
		char *start = e++;
		char *stop = strstr(e, "?=");
		if (!stop)
			goto nocode;

		char *dec, *decchunk;
		size_t declen;
		if (enc == 'q')
			blaze822_decode_qp(start, stop, &dec, &declen);
		else if (enc == 'b')
			blaze822_decode_b64(start, stop, &dec, &declen);
		else
			goto nocode;

		decchunk = dec;
		int r = iconv(ic, &dec, &declen, &dst, &dlen);
		if (r < 0) {
			if (errno == E2BIG) {
				iconv_close(ic);
				break;
			} else if (errno == EILSEQ || errno == EINVAL) {
				goto nocode;
			} else {
				perror("iconv");
				goto nocode;
			}
		}

		iconv_close(ic);

		while (declen && dlen) {
			*dst++ = *dec++;
			declen--;
			dlen--;
		}

		free(decchunk);

		b = stop + 2;
	} while (dlen && (s = strstr(b, "=?")));

	while (*b && dlen > 0) {
		*dst++ = *b++;
		dlen--;
	}

	*dst = 0;

	return 1;

nocode:
	if (ic != (iconv_t)-1)
		iconv_close(ic);

	fprintf(stderr, "error decoding rfc2047\n");
nocodeok:
	while (*src && dlen) {
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
	decode_qp(test, test + sizeof test, &r, &l);
	printf("%s %d\n", r, l);

	char *r2;
	size_t l2;
	char test2[] = "SWYgeW91IGNhbiByZWFkIHRoaXMgeW8=dSB1bmRlcnN0YW5kIHRoZSBleGFtcGxlLg==";
	decode_b64(test2, test2+sizeof test2, &r2, &l2);
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
	
}
#endif
