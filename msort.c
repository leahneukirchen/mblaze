#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "blaze822.h"

struct mail {
	char *file;
	long idx;
	union {
		long intkey;
		char *strkey;
	} k;
};

struct mail *mails;
ssize_t mailalloc = 1024;

int idx = 0;

int (*sortorder)(const void *, const void *);
int rflag;

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
                if (*disp)
                        from = strdup(disp);
                else if (*addr)
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

	if (!ia->k.strkey)
		ia->k.strkey = fetch_subj(ia->file);
	if (!ib->k.strkey)
		ib->k.strkey = fetch_subj(ib->file);

	int k = strcasecmp(ia->k.strkey, ib->k.strkey);
	if (k == 0)
		return ia->idx - ib->idx;   // XXX verify
	return k;
}

int
fromorder(const void *a, const void *b)
{
	struct mail *ia = (struct mail *)a;
	struct mail *ib = (struct mail *)b;

	if (!ia->k.strkey)
		ia->k.strkey = fetch_from(ia->file);
	if (!ib->k.strkey)
		ib->k.strkey = fetch_from(ib->file);

	int k = strcasecmp(ia->k.strkey, ib->k.strkey);
	if (k == 0)
		return ia->idx - ib->idx;   // XXX verify
	return k;
}

int
sizeorder(const void *a, const void *b)
{
	struct mail *ia = (struct mail *)a;
	struct mail *ib = (struct mail *)b;

	if (!ia->k.intkey)
		ia->k.intkey = fetch_size(ia->file);
	if (!ib->k.intkey)
		ib->k.intkey = fetch_size(ib->file);

	int k = ia->k.intkey - ib->k.intkey;
	if (k == 0)
		return ia->idx - ib->idx;   // XXX verify
	return k;
}

int
mtimeorder(const void *a, const void *b)
{
	struct mail *ia = (struct mail *)a;
	struct mail *ib = (struct mail *)b;

	if (!ia->k.intkey)
		ia->k.intkey = fetch_mtime(ia->file);
	if (!ib->k.intkey)
		ib->k.intkey = fetch_mtime(ib->file);

	int k = ia->k.intkey - ib->k.intkey;
	if (k == 0)
		return ia->idx - ib->idx;   // XXX verify
	return k;
}

int
dateorder(const void *a, const void *b)
{
	struct mail *ia = (struct mail *)a;
	struct mail *ib = (struct mail *)b;

	if (!ia->k.intkey)
		ia->k.intkey = fetch_date(ia->file);
	if (!ib->k.intkey)
		ib->k.intkey = fetch_date(ib->file);

	int k = ia->k.intkey - ib->k.intkey;
	if (k == 0)
		return ia->idx - ib->idx;   // XXX verify
	return k;
}

// taken straight from musl@a593414
int mystrverscmp(const char *l0, const char *r0)
{
	const unsigned char *l = (const void *)l0;
	const unsigned char *r = (const void *)r0;
	size_t i, dp, j;
	int z = 1;

	/* Find maximal matching prefix and track its maximal digit
	 * suffix and whether those digits are all zeros. */
	for (dp=i=0; l[i]==r[i]; i++) {
		int c = l[i];
		if (!c) return 0;
		if (!isdigit(c)) dp=i+1, z=1;
		else if (c!='0') z=0;
	}

	if (l[dp]!='0' && r[dp]!='0') {
		/* If we're not looking at a digit sequence that began
		 * with a zero, longest digit string is greater. */
		for (j=i; isdigit(l[j]); j++)
			if (!isdigit(r[j])) return 1;
		if (isdigit(r[j])) return -1;
	} else if (z && dp<i && (isdigit(l[i]) || isdigit(r[i]))) {
		/* Otherwise, if common prefix of digit sequence is
		 * all zeros, digits order less than non-digits. */
		return (unsigned char)(l[i]-'0') - (unsigned char)(r[i]-'0');
	}

	return l[i] - r[i];
}

int
fileorder(const void *a, const void *b)
{
	struct mail *ia = (struct mail *)a;
	struct mail *ib = (struct mail *)b;

	int x = mystrverscmp(a, b);

	if (x != 0)
		return x;
	return ia->idx - ib->idx;   // XXX verify
}

int
idxorder(const void *a, const void *b)
{
	struct mail *ia = (struct mail *)a;
	struct mail *ib = (struct mail *)b;

	return ia->idx - ib->idx;   // XXX verify
}

void
add(char *file)
{
	if (idx >= mailalloc) {
		mailalloc *= 2;
		if (mailalloc < 0)
			exit(-1);
		mails = realloc(mails, sizeof (struct mail) * mailalloc);
		memset(mails+mailalloc/2, 0, sizeof (struct mail) * mailalloc/2);
	}
	if (!mails)
		exit(-1);
	mails[idx].file = strdup(file);
	mails[idx].idx = idx;
	idx++;
}

int
main(int argc, char *argv[])
{
	sortorder = idxorder;

	int c;
	while ((c = getopt(argc, argv, "fdsFMSr")) != -1)
		switch(c) {
		case 'f': sortorder = fromorder; break;
		case 'd': sortorder = dateorder; break;
		case 's': sortorder = subjorder; break;
		case 'F': sortorder = fileorder; break;
		case 'M': sortorder = mtimeorder; break;
		case 'S': sortorder = sizeorder; break;
		case 'r': rflag = !rflag; break;
		default:
			// XXX usage
			exit(1);
		}


	mails = calloc(sizeof (struct mail), mailalloc);
	if (!mails)
		exit(-1);

	int i = blaze822_loop(argc-optind, argv+optind, add);

	qsort(mails, idx, sizeof (struct mail), sortorder);

	if (rflag)
		for (i = idx-1; i >= 0; i--)
			printf("%s\n", mails[i].file);
	else
		for (i = 0; i < idx; i++)
			printf("%s\n", mails[i].file);

	fprintf(stderr, "%d mails sorted\n", i);
	return 0;
}
