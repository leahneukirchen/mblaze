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
#include <wchar.h>

#include "blaze822.h"

void
u8putstr(FILE *out, char *s, size_t l, int pad)
{
	while (*s && l) {
		putc(*s, out);
		// elongate by utf8 overhead
		if      ((*s & 0xf0) == 0xf0) l += 3;
		else if ((*s & 0xe0) == 0xe0) l += 2;
		else if ((*s & 0xc0) == 0xc0) l += 1;
		l--;
		s++;
	}
	if (pad)
		while (l-- > 0)
			putc(' ', out);
}

int
oneline(char *file)
{
	int indent = 0;
	while (*file == ' ') {
		indent++;
		file++;
	}
	indent *= 2;
	
	struct message *msg = blaze822(file);
	if (!msg) {
		int p = 80-33-3-indent;
		printf("%*.*s\\_ %*.*s\n", -33 - indent, 33 + indent, "",
		    -p, p, file);
		return 0;
	}

	char flag1, flag2;

	char *f = strstr(file, "2:,");
        if (!f)
		f = "";

	if (!strchr(f, 'S'))
		flag1 = '.';
	else if (strchr(f, 'T'))
		flag1 = 'x';
	else
		flag1 = ' ';

	if (strchr(f, 'F'))
		flag2 = '#';
	else if (strchr(f, 'R'))
		flag2 = '-';
	else
		flag2 = ' ';

	char date[16];
        char *v;

        if ((v = blaze822_hdr(msg, "date"))) {
		time_t t = blaze822_date(v);
		if (t != -1) {
			struct tm *tm;
			tm = localtime(&t);
			strftime(date, sizeof date, "%Y-%m-%d", tm);
		} else {
			strcpy(date, "(invalid)");
		}
	} else {
		strcpy(date, "(unknown)");
		// mtime perhaps?
	}


	char *from = "(unknown)";
        if ((v = blaze822_hdr(msg, "from"))) {
		char *disp, *addr;
		blaze822_addr(v, &disp, &addr);
		if (*disp)
			from = disp;
		else if (*addr)
			from = addr;
		else
			from = "(unknown)";
	}

	char fromdec[17];
	blaze822_decode_rfc2047(fromdec, from, sizeof fromdec - 1, "UTF-8");
	fromdec[sizeof fromdec - 1] = 0;


	char *subj = "(no subject)";
	char subjdec[100];
        if ((v = blaze822_hdr(msg, "subject"))) {
		subj = v;
	}
	blaze822_decode_rfc2047(subjdec, subj, sizeof subjdec - 1, "UTF-8");

	printf("%c%c%9s  ", flag1, flag2, date);
	u8putstr(stdout, fromdec, 17, 1);
	printf("  ");
	int z;
	for (z = 0; z < indent; z++)
		printf(" ");
	u8putstr(stdout, subjdec, 80-33-indent, 0);
	printf("\n");
}

int
main(int argc, char *argv[])
{
	int i = blaze822_loop(argc-1, argv+1, oneline);

	printf("%d mails scanned\n", i);

	return 0;
}
