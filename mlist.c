#define _GNU_SOURCE

#include <sys/stat.h>

#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blaze822.h"

#define lc(c) ((c) | 0x20)
#define uc(c) ((c) & 0xdf)

/*

PRSTDF
prstdf

-N new
-n not new

-C cur
-c not cur

-m age

-r recursive?

*/

static int8_t flags[255];
static int flagsum;
static int flagset;
static int Nflag;
static int Cflag;
static int iflag;

static long icount;
static long iunseen;
static long iflagged;
static long imatched;

static long tdirs;
static long tunseen;
static long tflagged;
static long tcount;

void
list(char *prefix, char *file)
{
	if (flagset) {
		int sum = 0;
		char *f = strstr(file, ":2,");
		if (!f)
			return;
		icount++;
		f += 3;
		while (*f) {
			if (flags[(unsigned int)*f] == -1)
				return;
			if (flags[(unsigned int)*f] == 1)
				sum++;
			f++;
		}
		if (sum != flagsum)
			return;
	}

	if (iflag) {
		char *f = strstr(file, ":2,");
		if (!f)
			return;
		imatched++;
		if (!flagset)
			icount++, tcount++;
		if (!strchr(f, 'S'))
			iunseen++, tunseen++;
		if (strchr(f, 'F'))
			iflagged++, tflagged++;
		return;
	}

	if (prefix) {
		fputs(prefix, stdout);
		putc('/', stdout);
	}
	puts(file);
}

#ifdef __linux__
// faster implementation of readdir using a bigger getdents buffer

#include <sys/syscall.h>

struct linux_dirent64 {
	ino64_t d_ino;	   /* 64-bit inode number */
	off64_t d_off;	   /* 64-bit offset to next structure */
	unsigned short d_reclen; /* Size of this dirent */
	unsigned char d_type;    /* File type */
	char d_name[];	   /* Filename (null-terminated) */
};
#define BUF_SIZE 1024000
char buf[BUF_SIZE];

void
listdir(char *dir)
{
	int fd;
	ssize_t nread;
	ssize_t bpos;
	struct linux_dirent64 *d;

	fd = open(dir, O_RDONLY | O_DIRECTORY);
	if (fd == -1) {
		perror("open");
		return;
	}

	while (1) {
		nread = syscall(SYS_getdents64, fd, buf, BUF_SIZE);
		if (nread == -1) {
			perror("getdents64");
			break;
		}

		if (nread == 0)
			break;

		for (bpos = 0; bpos < nread; bpos += d->d_reclen) {
			d = (struct linux_dirent64 *)(buf + bpos);
			if (d->d_type != DT_REG && d->d_type != DT_UNKNOWN)
				continue;
			if (d->d_name[0] == '.')
				continue;
			list(dir, d->d_name);
		}
	}

	close(fd);
}
#else
void
listdir(char *dir)
{
	DIR *fd;
	struct dirent *d;

	fd = opendir(dir);
	if (!fd)
		return;
	while ((d = readdir(fd))) {
		if (d->d_type != DT_REG && d->d_type != DT_UNKNOWN)
			continue;
		if (d->d_name[0] == '.')
			continue;
		list(dir, d->d_name);
	}
	closedir(fd);
}
#endif

void
listarg(char *arg)
{
	squeeze_slash(arg);

	struct stat st;
	if (stat(arg, &st) < 0)
		return;
	if (S_ISDIR(st.st_mode)) {
		char subdir[PATH_MAX];
		struct stat st2;
		int maildir = 0;

		long gcount = icount;
		long gunseen = iunseen;
		long gflagged = iflagged;
		long gmatched = imatched;

		icount = 0;
		iunseen = 0;
		iflagged = 0;

		snprintf(subdir, sizeof subdir, "%s/cur", arg);
		if (stat(subdir, &st2) == 0) {
			maildir = 1;
			if (Cflag >= 0 && Nflag <= 0)
				listdir(subdir);
		}

		snprintf(subdir, sizeof subdir, "%s/new", arg);
		if (stat(subdir, &st2) == 0) {
			maildir = 1;
			if (Nflag >= 0 && Cflag <= 0)
				listdir(subdir);
		}

		if (!maildir)
			listdir(arg);

		if (iflag && imatched) {
			tdirs++;
			printf("%6ld unseen  %3ld flagged  %6ld msg  %s\n",
			    iunseen, iflagged, icount, arg);
		}

		icount = gcount;
		iunseen = gunseen;
		iflagged = gflagged;
		imatched = gmatched;
	} else if (S_ISREG(st.st_mode)) {
		list(0, arg);
	}
}

int
main(int argc, char *argv[])
{
	int c;
	while ((c = getopt(argc, argv, "PRSTDFprstdfX:x:NnCci")) != -1)
		switch (c) {
		case 'P': case 'R': case 'S': case 'T': case 'D': case 'F':
			flags[(unsigned int)c] = 1;
			break;
		case 'p': case 'r': case 's': case 't': case 'd': case 'f':
			flags[(unsigned int)uc(c)] = -1;
			break;
		case 'X':
			while (*optarg)
				flags[(unsigned int)*optarg++] = 1;
			break;
		case 'x':
			while (*optarg)
				flags[(unsigned int)*optarg++] = -1;
			break;
		case 'N': Nflag = 1; break;
		case 'n': Nflag = -1; break;
		case 'C': Cflag = 1; break;
		case 'c': Cflag = -1; break;
		case 'i': iflag = 1; break;
		default:
usage:
			fprintf(stderr,
			    "Usage: mlist [-DFPRST] [-X str]\n"
			    "	     [-dfprst] [-x str]\n"
			    "	     [-N | -n | -C | -c]\n"
			    "	     [-i] [dirs...]\n"
			);
			exit(1);
		}

	int i;
#if defined(__OpenBSD__)
	if (pledge("stdio rpath tty", NULL) == -1)
		err(1, "pledge");
#endif	
	for (i = 0, flagsum = 0, flagset = 0; (size_t)i < sizeof flags; i++) {
		if (flags[i] != 0)
			flagset++;
		if (flags[i] == 1)
			flagsum++;
	}

	if (optind == argc) {
		if (isatty(0))
			goto usage;
		blaze822_loop(0, 0, listarg);
	} else {
		for (i = optind; i < argc; i++)
			listarg(argv[i]);
	}

	if (iflag && tdirs > 1)
		printf("%6ld unseen  %3ld flagged  %6ld msg\n",
		    tunseen, tflagged, tcount);

	return 0;
}
