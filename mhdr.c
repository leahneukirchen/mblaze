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
static int Aflag;
static int Dflag;
static int Mflag;
static int dflag;

static int status;

static void
printhdr(char *hdr)
{
	int uc = 1;

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
print_addresses(char *s)
{
	char *disp, *addr;
	while ((s = blaze822_addr(s, &disp, &addr))) {
		if (disp && addr) {
			if (dflag) {
				char d[4096];
				blaze822_decode_rfc2047(d, disp, sizeof d,
				    "UTF-8");
				printf("%s <%s>\n", d, addr);
			} else {
				printf("%s <%s>\n", disp, addr);
			}
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
	printf("%ld\n", t);
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
	status = 0;

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
			if (strncmp(hdr, h, l) == 0 && hdr[l] == ':') {
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

	msg = blaze822(file);
	if (!msg)
		return;

	if (!hflag)
		return headerall(msg);
	if (Mflag)
		return headermany(msg);

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
	while ((c = getopt(argc, argv, "h:ADMd")) != -1)
		switch(c) {
		case 'h': hflag = optarg; break;
		case 'A': Aflag = 1; break;
		case 'D': Dflag = 1; break;
		case 'M': Mflag = 1; break;
		case 'd': dflag = 1; break;
		default:
			fprintf(stderr,
"Usage: mhdr [-h header] [-d] [-M] [-A|-D] [msgs...]\n");
			exit(2);
		}

	status = 1;

	if (argc == optind && isatty(0))
		blaze822_loop1(".", header);
	else
		blaze822_loop(argc-optind, argv+optind, header);
	
	return status;
}
