#include <stdint.h>
#include <stdio.h>

#include "u8decode.h"

void
safe_u8putstr(char *s0, size_t l, FILE *stream)
{
	// tty-safe output of s, with relaxed utf-8 semantics:
	// - C0 and C1 are displayed as escape sequences
	// - valid utf-8 is printed as is
	// - rest is assumed to be latin-1, and translated into utf-8
	// - translate CRLF to CR

	unsigned char *s = (unsigned char *)s0;
	unsigned char *e = s + l;
	uint32_t c;

	while (s < e) {
		int l = u8decode((char *)s, &c);
		if (l == -1) {
			l = 1;
			if (*s <= 0x9fu) {
				// C1
				fputc(0xe2, stream);
				fputc(0x90, stream);
				fputc(0x80+0x1b, stream);

				fputc(0xe2, stream);
				fputc(0x90, stream);
				fputc(*s, stream);
			} else {
				/* invalid utf-8, assume it was latin-1 */
				fputc(0xc0 | (*s >> 6), stream);
				fputc(0x80 | (*s & 0x3f), stream);
			}
		} else if (c < 32 &&
		    *s != ' ' && *s != '\t' && *s != '\n' && *s != '\r') {
			// NUL
			if (l == 0)
				l = 1;
			// C0
			fputc(0xe2, stream);
			fputc(0x90, stream);
			fputc(0x80+*s, stream);
		} else if (c == 127) {
			// DEL
			fputc(0xe2, stream);
			fputc(0x90, stream);
			fputc(0xa1, stream);
		} else if (c == '\r') {
			if (e - s > 1 && s[1] == '\n')
				s++;
			fputc(*s, stream);
		} else {
			fwrite(s, 1, l, stream);
		}
		s += l;
	}
}
