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
	while (*s && l > 0) {
		putc(*s, out);
		if ((*s++ & 0xc0) != 0x80)
			l--;
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
	
	struct message *msg = blaze822(file);

	if (!msg) {
		printf("%*.*s \\ %33.33s\n", -33 - indent, 33 + indent, "",
		    file);
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
		}
	} else {
		strcpy(date, "(invalid)");
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
	if (!decode_rfc2047(from, fromdec, sizeof fromdec))
		memcpy(fromdec, from, sizeof fromdec);
	fromdec[sizeof fromdec - 1] = 0;


	char *subj = "(no subject)";
	char subjdec[1000];   // XXX rewrite decode_rfc2047, it overflows!
        if ((v = blaze822_hdr(msg, "subject"))) {
		if (decode_rfc2047(v, subjdec, sizeof subjdec - 1))
			subj = subjdec;
		else
			subj = v;
		
	}

	printf("%c%c%9s  ", flag1, flag2, date);
	u8putstr(stdout, fromdec, 17, 1);
	printf("  ");
	int z;
	for (z = 0; z < indent; z++)
		printf(" ");
	u8putstr(stdout, subj, 80-33-indent, 0);
	printf("\n");
}

int
main(int argc, char *argv[])
{
	char *s;
	
	char *line = 0;
	size_t linelen = 0;
	int read;

	int i = 0;

	if (argc == 1 || (argc == 2 && strcmp(argv[1], "-") == 0)) {
		while ((read = getdelim(&line, &linelen, '\n', stdin)) != -1) {
			if (line[read-1] == '\n') line[read-1] = 0;
//			printf("%s\n", line);
			oneline(line);
			i++;
		}
	} else {
		for (i = 1; i < argc; i++) {
			oneline(argv[i]);
		}
		i--;
	}

	printf("%d mails scanned\n", i);

	return 0;
}
