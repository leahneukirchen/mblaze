#include <stdint.h>
#include <stdio.h>

void
safe_u8putstr(char *s0, size_t l, FILE *stream)
{
	// tty-safe output of s, with relaxed utf-8 semantics:
	// - C0 and C1 are displayed as escape sequences
	// - valid utf8 is printed as is
	// - rest is assumed to be latin1, and translated into utf8
	// - translate CRLF to CR

	unsigned char *s = (unsigned char *)s0;
	unsigned char *e = s + l;

	while (s < e) {
		if ((*s & 0x80) == 0) {
			if (*s < 32 &&
			    *s != ' ' && *s != '\t' && *s != '\n' && *s != '\r') {
				// C0
				fputc(0xe2, stream);
				fputc(0x90, stream);
				fputc(0x80+*s, stream);
			} else if (*s == 127) {
				// DEL
				fputc(0xe2, stream);
				fputc(0x90, stream);
				fputc(0xa1, stream);
			} else if (*s == '\r') {
				if (e - s > 1 && s[1] == '\n')
					s++;
				fputc(*s, stream);
			} else {
				// safe ASCII
				fputc(*s, stream);
			}
		} else if ((*s & 0xc0) == 0x80) {
			if (*s >= 0xa0)
				goto latin1;

			// C1
			fputc(0xe2, stream);
			fputc(0x90, stream);
			fputc(0x80+0x1b, stream);

			fputc(0xe2, stream);
			fputc(0x90, stream);
			fputc(*s, stream);
		} else {
			uint32_t f = 0;
			if (e - s >= 4)
				f = (s[0]<<24) | (s[1]<<16) | (s[2]<<8) | s[3];
			else if (e - s == 3)
				f = (s[0]<<24) | (s[1]<<16) | (s[2]<<8);
			else if (e - s == 2)
				f = (s[0]<<24) | (s[1]<<16);
			else if (e - s == 1)
				f = (s[0]<<24);

			if      ((f & 0xe0c00000) == 0xc0800000) goto u2;
			else if ((f & 0xf0c0c000) == 0xe0808000) goto u3;
			else if ((f & 0xf8c0c0c0) == 0xf0808080) {
				fputc(*s++, stream);
u3:                             fputc(*s++, stream);
u2:                             fputc(*s++, stream);
				fputc(*s, stream);
			} else {
latin1:
				/* invalid utf8, assume it was latin1 */
				fputc(0xc0 | (*s >> 6), stream);
				fputc(0x80 | (*s & 0x3f), stream);
			}
		}
		s++;
	}
}
