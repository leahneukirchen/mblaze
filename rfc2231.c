#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blaze822.h"

int
blaze822_mime2231_parameter(char *s, char *name,
    char *dst, size_t dlen, char *tgtenc)
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

	int i = 0;
	char namenum[64];

	char *srcenc = 0;

	char *dststart = dst;
	char *dstend = dst + dlen;

	char *sbuf, *ebuf;

	snprintf(namenum, sizeof namenum, "%s*", name);
	if (blaze822_mime_parameter(s, namenum, &sbuf, &ebuf)) {
		i = 100;
		goto found_extended;
	}
	if (blaze822_mime_parameter(s, name, &sbuf, &ebuf)) {
		i = 100;
		goto found_plain;
	}

	while (i < 100) {
		snprintf(namenum, sizeof namenum, "%s*%d*", name, i);
		if (blaze822_mime_parameter(s, namenum, &sbuf, &ebuf)) {
found_extended:
			// decode extended
			if (i == 0 || i == 100) { // extended-initial-value
				char *encstart = sbuf;
				sbuf = strchr(sbuf, '\'');
				if (!sbuf)
					return 0;
				srcenc = strndup(encstart, sbuf - encstart);
				if (!srcenc)
					return 0;
				sbuf = strchr(sbuf+1, '\'');
				if (!sbuf)
					return 0;
				sbuf++;
			}
			while (sbuf < ebuf && dst < dstend) {
				if (sbuf[0] == '%') {
					unsigned char c1 = sbuf[1];
					unsigned char c2 = sbuf[2];
					if (c1 < 127 && c2 < 127 &&
					    hex[c1] > -1 && hex[c2] > -1) {
						*dst++ = (hex[c1] << 4) | hex[c2];
						sbuf += 3;
					} else {
						*dst++ = *sbuf++;
					}
				} else {
					*dst++ = *sbuf++;
				}
			}
			*dst = 0;
		} else {
			namenum[strlen(namenum) - 1] = 0;   // strip last *
			if (blaze822_mime_parameter(s, namenum, &sbuf, &ebuf)) {
found_plain:
				// copy plain
				if (ebuf - sbuf < dstend - dst) {
					memcpy(dst, sbuf, ebuf - sbuf);
					dst += ebuf - sbuf;
				}
				*dst = 0;
			} else {
				break;
			}
		}
		i++;
	}

	if (i <= 0)
		return 0;

	if (!srcenc)
		return 1;

	iconv_t ic = iconv_open(tgtenc, srcenc);
	free(srcenc);
	if (ic == (iconv_t)-1)
		return 1;

	size_t tmplen = dlen;
	char *tmp = malloc(tmplen);
	if (!tmp)
		return 1;
	char *tmpend = tmp;

	size_t dstlen = dst - dststart;
	dst = dststart;

	size_t r = iconv(ic, &dst, &dstlen, &tmpend, &tmplen);
	if (r == (size_t)-1) {
		free(tmp);
		return 1;
	}

	iconv_close(ic);

	// copy back
	memcpy(dststart, tmp, tmpend - tmp);
	dststart[tmpend - tmp] = 0;
	free(tmp);

	return 1;
}
