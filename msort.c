#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "blaze822.h"
#include "xpledge.h"

struct mail {
	char *file;
	long idx;
	char *from;
	char *subj;
	time_t date;
	time_t mtime;
	off_t size;
};

struct mail *mails;
ssize_t mailalloc = 1024;

int idx;
int rflag;

static int (*sortorders[16])(const void *, const void *);
int order_idx;

int mystrverscmp(const char *, const char *);

char *
fetch_subj(char *file)
{
	struct message *msg = blaze822(file);
	if (!msg)
		return " (error)";
	char *v = blaze822_hdr(msg, "subject");
	if (!v) {
		blaze822_free(msg);
		return " (no subject)";
	}

	char *ov = 0;
	char *s;
	while (v != ov) {
		while (*v == ' ')
			v++;
		if (strncasecmp(v, "Re:", 3) == 0)
			v += 3;
		if (strncasecmp(v, "Aw:", 3) == 0)
			v += 3;
		if (strncasecmp(v, "Sv:", 3) == 0)
			v += 3;
		if (strncasecmp(v, "Wg:", 3) == 0)
			v += 3;
		if (strncasecmp(v, "Fwd:", 4) == 0)
			v += 4;
		// XXX skip [prefix]?
		ov = v;
	}
	s = strdup(v);

	blaze822_free(msg);
	return s;
}

char *
fetch_from(char *file)
{
	char *from = " (unknown)";
	struct message *msg = blaze822(file);
	if (!msg)
		return " (error)";
	char *v = blaze822_hdr(msg, "from");
	if (v) {
		char *disp, *addr;
		blaze822_addr(v, &disp, &addr);
		if (disp)
			from = strdup(disp);
		else if (addr)
			from = strdup(addr);
	}

	blaze822_free(msg);
	return from;
}

time_t
fetch_date(char *file)
{
	time_t t = -1;
	struct message *msg = blaze822(file);
	if (!msg)
		return -1;
	char *v = blaze822_hdr(msg, "date");
	if (v)
		t = blaze822_date(v);
	blaze822_free(msg);
	return t;
}

time_t
fetch_mtime(char *file)
{
	struct stat st;
	if (stat(file, &st) < 0)
		return -1;
	return st.st_mtime;
}

off_t
fetch_size(char *file)
{
	struct stat st;
	if (stat(file, &st) < 0)
		return 0;
	return st.st_size;
}

int
subjorder(const void *a, const void *b)
{
	struct mail *ia = (struct mail *)a;
	struct mail *ib = (struct mail *)b;

	if (!ia->subj)
		ia->subj = fetch_subj(ia->file);
	if (!ib->subj)
		ib->subj = fetch_subj(ib->file);

	return strcasecmp(ia->subj, ib->subj);
}

int
fromorder(const void *a, const void *b)
{
	struct mail *ia = (struct mail *)a;
	struct mail *ib = (struct mail *)b;

	if (!ia->from)
		ia->from = fetch_from(ia->file);
	if (!ib->from)
		ib->from = fetch_from(ib->file);

	return strcasecmp(ia->from, ib->from);
}

int
sizeorder(const void *a, const void *b)
{
	struct mail *ia = (struct mail *)a;
	struct mail *ib = (struct mail *)b;

	if (!ia->size)
		ia->size = fetch_size(ia->file);
	if (!ib->size)
		ib->size = fetch_size(ib->file);

	if (ia->size > ib->size)
		return 1;
	else if (ia->size < ib->size)
		return -1;
	return 0;
}

int
mtimeorder(const void *a, const void *b)
{
	struct mail *ia = (struct mail *)a;
	struct mail *ib = (struct mail *)b;

	if (!ia->mtime)
		ia->mtime = fetch_mtime(ia->file);
	if (!ib->mtime)
		ib->mtime = fetch_mtime(ib->file);

	if (ia->mtime > ib->mtime)
		return 1;
	else if (ia->mtime < ib->mtime)
		return -1;
	return 0;
}

int
dateorder(const void *a, const void *b)
{
	struct mail *ia = (struct mail *)a;
	struct mail *ib = (struct mail *)b;

	if (!ia->date)
		ia->date = fetch_date(ia->file);
	if (!ib->date)
		ib->date = fetch_date(ib->file);

	if (ia->date > ib->date)
		return 1;
	else if (ia->date < ib->date)
		return -1;
	return 0;
}

int
fileorder(const void *a, const void *b)
{
	struct mail *ia = (struct mail *)a;
	struct mail *ib = (struct mail *)b;

	return mystrverscmp(ia->file, ib->file);
}

int
unreadorder(const void *a, const void *b)
{
	struct mail *ia = (struct mail *)a;
	struct mail *ib = (struct mail *)b;

	char *fa = strstr(ia->file, MAILDIR_COLON_SPEC_VER_COMMA);
	char *fb = strstr(ib->file, MAILDIR_COLON_SPEC_VER_COMMA);

	int unreada = fa ? !strchr(fa, 'S') : 0;
	int unreadb = fb ? !strchr(fb, 'S') : 0;

	return unreada - unreadb;
}

int
flaggedorder(const void *a, const void *b)
{
	struct mail *ia = (struct mail *)a;
	struct mail *ib = (struct mail *)b;

	char *fa = strstr(ia->file, MAILDIR_COLON_SPEC_VER_COMMA);
	char *fb = strstr(ib->file, MAILDIR_COLON_SPEC_VER_COMMA);

	int unreada = fa ? !!strchr(fa, 'F') : 0;
	int unreadb = fb ? !!strchr(fb, 'F') : 0;

	return unreadb - unreada;
}

int
idxorder(const void *a, const void *b)
{
	struct mail *ia = (struct mail *)a;
	struct mail *ib = (struct mail *)b;

	if (ia->idx > ib->idx)
		return 1;
	else if (ia->idx < ib->idx)
		return -1;
	else
		return 0;
}

void
add(char *file)
{
	if (idx >= mailalloc) {
		mailalloc *= 2;
		if (mailalloc < 0)
			exit(-1);
		mails = realloc(mails, sizeof (struct mail) * mailalloc);
		if (!mails)
			exit(-1);
		memset(mails+mailalloc/2, 0, sizeof (struct mail) * mailalloc/2);
	}
	if (!mails)
		exit(-1);
	mails[idx].file = strdup(file);
	mails[idx].idx = idx;
	idx++;
}

int
order(const void *a, const void *b)
{
	int i;
	for (i = 0; i < order_idx; i++) {
		int r = (sortorders[i])(a, b);
		if (r != 0)
			return r;
	}
	return idxorder(a, b);
}

void
addorder(int (*sortorder)(const void *, const void *))
{
	if (order_idx < (int)(sizeof sortorders / sizeof sortorders[0]))
		sortorders[order_idx++] = sortorder;
}

int
main(int argc, char *argv[])
{
	int c, i;

	while ((c = getopt(argc, argv, "fdsFMSUIr")) != -1)
		switch (c) {
		case 'f': addorder(fromorder); break;
		case 'd': addorder(dateorder); break;
		case 's': addorder(subjorder); break;
		case 'F': addorder(fileorder); break;
		case 'M': addorder(mtimeorder); break;
		case 'S': addorder(sizeorder); break;
		case 'U': addorder(unreadorder); break;
		case 'I': addorder(flaggedorder); break;
		case 'r': rflag = !rflag; break;
		default:
			fprintf(stderr,
			    "Usage: msort [-r] [-fdsFMSUI] [msgs...]\n");
			exit(1);
		}

	xpledge("stdio rpath", "");

	mails = calloc(mailalloc, sizeof (struct mail));
	if (!mails)
		exit(-1);

	if (argc == optind && isatty(0))
		blaze822_loop1(":", add);
	else
		blaze822_loop(argc-optind, argv+optind, add);

	qsort(mails, idx, sizeof (struct mail), order);

	if (rflag)
		for (i = idx-1; i >= 0; i--)
			printf("%s\n", mails[i].file);
	else
		for (i = 0; i < idx; i++)
			printf("%s\n", mails[i].file);

	return 0;
}
