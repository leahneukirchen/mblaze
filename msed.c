#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blaze822.h"

static char *expr;

char *
subst(char *str, char *srch, char *repl, char *flags)
{
	static char buf[4096];
	char *bufe = buf + sizeof buf;

	int dflag = !!strchr(flags, 'd');
	int gflag = !!strchr(flags, 'g');
	int iflag = !!strchr(flags, 'i');

#define APP(o, l) do { if (bufe-b < (ssize_t)l) return str; memcpy(b, str+i+o, l); b += l; } while (0)
#define APPC(c) do { if (b >= bufe) return str; *b++ = c; } while (0)

	regex_t srchrx;
	regmatch_t pmatch[10];
	if (regcomp(&srchrx, srch, iflag ? REG_ICASE : 0) != 0)
		return str;

	char *b = buf;

	regoff_t i = 0;
	while (1) {
		if (regexec(&srchrx, str+i, 9, pmatch, 0) != 0)
			break;

		if (dflag)
			return 0;

		APP(0, pmatch[0].rm_so);

		char *t = repl;
		while (*t) {
			// & == \0
			if (*t == '&' || (*t == '\\' && isdigit(*(t+1)))) {
				int n;
				if (*t == '&') {
					t++;
					n = 0;
				} else {
					t++;
					n = *t++ - '0';
				}

				APP(pmatch[n].rm_so,
				    pmatch[n].rm_eo - pmatch[n].rm_so);
			} else if (*t == '\\' && *(t+1)) {
				t++;
				APPC(*t++);
			} else {
				APPC(*t++);
			}
		}

		i += pmatch[0].rm_eo;  // advance to end of match
		if (!gflag)
			break;
	}

	if (i > 0) { // any match?
		APP(0, strlen(str + i));
		*b = 0;
		return buf;
	}

	return str;
}

void
printhdr(char *hdr, int rest)
{
	int uc = 1;

	while (*hdr && *hdr != ':') {
		putc(uc ? toupper(*hdr) : *hdr, stdout);
		uc = (*hdr == '-');
		hdr++;
	}

	if (rest) {
		printf("%s\n", hdr);
	}
}

void
sed(char *file)
{
	struct message *msg = blaze822_file(file);
	if (!msg)
		return;

	char *h = 0;
	while ((h = blaze822_next_header(msg, h))) {
		regex_t headerrx;
		char headersel[1024];

		char *v = strchr(h, ':');
		if (!v)
			continue;
		v++;
		while (*v && (*v == ' ' || *v == '\t'))
			v++;

		v = strdup(v);

		char *e = expr;
		while (*e) {
			while (*e &&
			    (*e == ' ' || *e == '\t' || *e == '\n' || *e == ';'))
				e++;

			*headersel = 0;
			if (*e == '/') {
				e++;
				char *s = e;
				// parse_headers, sets headersel
				while (*e && *e != '/')
					e++;
				snprintf(headersel, sizeof headersel,
				    "^(%.*s)*:", (int)(e-s), s);
				for (s = headersel; *s && *(s+1); s++)
					if (*s == ':')
						*s = '|';
				int rv;
				if ((rv = regcomp(&headerrx, headersel, REG_EXTENDED)) != 0) {
					char buf[100];
					regerror(rv, &headerrx, buf, sizeof buf);
					fprintf(stderr, "msed: %s\n", buf);
					exit(1);
				}
				if (*e)
					e++;
			}

			char sep;
			char *s;
			if (!*headersel || regexec(&headerrx, h, 0, 0, 0) == 0) {
				switch (*e) {
				case 'd':
					free(v);
					v = 0;
					break;
				case 'a':
					// skipped here;
					e++;
					if ((*e == ' ' || *e == ';' || *e == '\n' || !*e)) {
						break;
					}
					sep = *e++;
					if (!sep) {
						fprintf(stderr, "msed: unterminated a command\n");
						exit(1);
					}
					while (*e && *e != sep)
						e++;
					if (*e == sep)
						e++;
					if (!(*e == ' ' || *e == ';' || *e == '\n' || !*e)) {
						fprintf(stderr, "msed: unterminated a command\n");
						exit(1);
					}
					break;
				case 'c':
					sep = *++e;
					s = ++e;
					while (*e && *e != sep)
						e++;
					free(v);
					v = strndup(s, e-s);
					break;
				case 's':
					sep = *++e;
					s = ++e;
					while (*e && *e != sep)
						e++;
					char *t = ++e;
					while (*e && *e != sep)
						e++;
					char *u = ++e;
					while (*e == 'd' || *e == 'g' || *e == 'i')
						e++;

					if (!(*e == ' ' || *e == ';' || *e == '\n' || !*e)) {
						fprintf(stderr, "msed: unterminated s command\n");
						exit(1);
					}

					// XXX stack allocate
					char *from = strndup(s, t-s-1);
					char *to = strndup(t, u-t-1);
					char *flags = strndup(u, e-u);

					char *ov = v;
					char *r = subst(ov, from, to, flags);
					if (r)
						v = strdup(r);
					else
						v = 0;

					free(ov);
					free(from);
					free(to);
					free(flags);

					break;
				default:
					fprintf(stderr, "msed: unknown command: '%c'\n", *e);
					exit(1);
				}
			}
			while (*e && *e != ';' && *e != '\n')
				e++;
		}
		if (v) {
			printhdr(h, 0);
			printf(": %s\n", v);
			free(v);
		}
	}

	// loop, do all a//

	char *hs, *he;
	char *e = expr;
	while (*e) {
		while (*e &&
		    (*e == ' ' || *e == '\t' || *e == '\n' || *e == ';'))
			e++;

		hs = he = 0;
		if (*e == '/') {
			e++;
			hs = e;
			// parse_headers, sets headersel
			while (*e && *e != '/')
				e++;
			he = e;
			if (*e)
				e++;
		}

		char sep;
		char *s;
		char *h = 0;
		char *v = 0;
		switch (*e) {
		case 'a':
			if (he != hs) {
				h = strndup(hs, he-hs);
			} else {
				fprintf(stderr, "msed: used command a without header name\n");
				exit(1);
			}

			e++;
			if (*e == ' ' || *e == '\t' || *e == '\n' || *e == ';' || !*e) {
				fprintf(stderr, "msed: no header value for %s\n", h);
				exit(1);
			} else {
				sep = *e;
				if (!sep) {
					fprintf(stderr, "msed: unterminated a command\n");
					exit(1);
				}
				s = ++e;
				while (*e && *e != sep)
					e++;
				v = strndup(s, e-s);
			}

			if (blaze822_chdr(msg, h)) {
				free(h);
				free(v);
				break;
			}

			printhdr(h, 0);
			printf(": %s\n", v);
			free(h);
			free(v);

			break;

		case 'c':
		case 'd':
		case 's':
			// ignore here;
			break;
		}
		while (*e && *e != ';' && *e != '\n')
			e++;
	}

	printf("\n");
	fwrite(blaze822_body(msg), 1, blaze822_bodylen(msg), stdout);
}

int
main(int argc, char *argv[])
{
	int c;
	while ((c = getopt(argc, argv, "")) != -1)
		switch (c) {
		default:
			fprintf(stderr, "Usage: msed [expr] [msgs...]\n");
			exit(1);
		}

	expr = argv[optind];
	optind++;

	if (argc == optind && isatty(0))
		blaze822_loop1(".", sed);
	else
		blaze822_loop(argc-optind, argv+optind, sed);

	return 0;
}
