#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blaze822.h"

static char *hflag;
static char *pflag;
static int Aflag;
static int Dflag;
static int Hflag;
static int Mflag;
static int dflag;

static char *curfile;
static int status;

static void
printhdr(char *hdr)
{
	int uc = 1;

	if (Hflag)
		printf("%s\t", curfile);

	while (*hdr && *hdr != ':') {
		putc(uc ? toupper(*hdr) : *hdr, stdout);
		uc = (*hdr == '-');
		hdr++;
	}
	fputs(hdr, stdout);
	fputc('\n', stdout);

	status = 0;
}

void
headerall(struct message *msg)
{
	char *h = 0;
	while ((h = blaze822_next_header(msg, h))) {
		if (dflag) {
			char d[4096];
			blaze822_decode_rfc2047(d, h, sizeof d, "UTF-8");
			printhdr(d);
		} else {
			printhdr(h);
		}
	}

	blaze822_free(msg);
}

void
print_quoted(char *s)
{
	char *t;

	for (t = s; *t; t++)
                if ((unsigned char)*t < 32 || strchr("()<>[]:;@\\,.\"", *t))
			goto quote;
	
	printf("%s", s);
	return;

quote:
	putchar('"');
	for (t = s; *t; t++) {
		if (*t == '"' || *t == '\\')
			putchar('\\');
		putchar(*t);
	}
	putchar('"');

}

void
print_addresses(char *s)
{
	char *disp, *addr;
	char sdec[4096];

	if (dflag) {
		blaze822_decode_rfc2047(sdec, s, sizeof sdec, "UTF-8");
		sdec[sizeof sdec - 1] = 0;
		s = sdec;
	}

	while ((s = blaze822_addr(s, &disp, &addr))) {
		if (Hflag && addr)
			printf("%s\t", curfile);

		if (disp && addr) {
			print_quoted(disp);
			printf(" <%s>\n", addr);
		} else if (addr) {
			printf("%s\n", addr);
		}
	}
}

void
print_date(char *s)
{
	time_t t = blaze822_date(s);
	if (t == -1)
		return;
	printf("%ld\n", (long)t);
}

void
print_decode_header(char *s)
{
	char d[4096];
	blaze822_decode_rfc2047(d, s, sizeof d, "UTF-8");
	printf("%s\n", d);
}

void
print_header(char *v)
{
	if (pflag) {
		char *s, *se;
		if (blaze822_mime_parameter(v, pflag, &s, &se)) {
			*se = 0;
			v = s;
		} else {
			return;
		}
	}

	status = 0;

	if (Hflag && !Aflag)
		printf("%s\t", curfile);

	if (Aflag)
		print_addresses(v);
	else if (Dflag)
		print_date(v);
	else if (dflag)
		print_decode_header(v);
	else
		printf("%s\n", v);
}

void
headermany(struct message *msg)
{
	char *hdr = 0;
	while ((hdr = blaze822_next_header(msg, hdr))) {
		char *h = hflag;
		while (*h) {
			char *n = strchr(h, ':');
			if (n)
				*n = 0;

			size_t l = strlen(h);
			if (strncasecmp(hdr, h, l) == 0 && hdr[l] == ':') {
				hdr += l + 1;
				while (*hdr == ' ' || *hdr == '\t')
					hdr++;
				print_header(hdr);
			}

			if (n) {
				*n = ':';
				h = n + 1;
			} else {
				break;
			}
		}
	}

	blaze822_free(msg);
}

void
header(char *file)
{
	struct message *msg;

	while (*file == ' ' || *file == '\t')
		file++;

	curfile = file;

	msg = blaze822(file);
	if (!msg)
		return;

	if (!hflag) {
		headerall(msg);
		return;
	}
	if (Mflag) {
		headermany(msg);
		return;
	}

	char *h = hflag;
	while (*h) {
		char *n = strchr(h, ':');
		if (n)
			*n = 0;
		char *v = blaze822_chdr(msg, h);
		if (v)
			print_header(v);
		if (n) {
			*n = ':';
			h = n + 1;
		} else {
			break;
		}
	}

	blaze822_free(msg);
}

int
main(int argc, char *argv[])
{
	int c;
	while ((c = getopt(argc, argv, "h:p:ADHMd")) != -1)
		switch (c) {
		case 'h': hflag = optarg; break;
		case 'p': pflag = optarg; break;
		case 'A': Aflag = 1; break;
		case 'D': Dflag = 1; break;
		case 'H': Hflag = 1; break;
		case 'M': Mflag = 1; break;
		case 'd': dflag = 1; break;
		default:
			fprintf(stderr,
"Usage: mhdr [-h header [-p parameter]] [-d] [-H] [-M] [-A|-D] [msgs...]\n");
			exit(2);
		}

	status = 1;

	if (argc == optind && isatty(0))
		blaze822_loop1(".", header);
	else
		blaze822_loop(argc-optind, argv+optind, header);

	return status;
}
