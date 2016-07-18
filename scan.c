#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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

static char *cur;

void
oneline(char *file)
{
	int indent = 0;
	while (*file == ' ' || *file == '\t') {
		indent++;
		file++;
	}
	indent *= 2;

	char *e = file + strlen(file) - 1;
	while (file < e && (*e == ' ' || *e == '\t'))
		*e-- = 0;
	
	struct message *msg = blaze822(file);
	if (!msg) {
		int p = 80-38-3-indent;
		printf("%*.*s\\_ %*.*s\n", -38 - indent, 38 + indent, "",
		    -p, p, file);
		return;
	}

	char flag1, flag2;

	char *f = strstr(file, ":2,");
        if (!f)
		f = "";

	if (strcmp(cur, file) == 0)
		flag1 = '>';
	else if (!strchr(f, 'S'))
		flag1 = '.';
	else if (strchr(f, 'T'))
		flag1 = 'x';
	else
		flag1 = ' ';

	if (strchr(f, 'F'))
		flag2 = '*';
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

	long lineno = blaze822_seq_find(file);
	if (lineno)
		printf("%c%c %-3ld %-10s  ", flag1, flag2, lineno, date);
	else
		printf("%c%c     %-10s  ", flag1, flag2, date);
	u8putstr(stdout, fromdec, 17, 1);
	printf("  ");
	int z;
	for (z = 0; z < indent; z++)
		printf(" ");
	u8putstr(stdout, subjdec, 80-38-indent, 0);
	printf("\n");

	blaze822_free(msg);
}

int
main(int argc, char *argv[])
{
	char *seqmap = blaze822_seq_open(0);
	blaze822_seq_load(seqmap);
	cur = blaze822_seq_cur();

	int i = blaze822_loop(argc-1, argv+1, oneline);

	fprintf(stderr, "%d mails scanned\n", i);

	return 0;
}
