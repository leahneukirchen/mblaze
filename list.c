#define _GNU_SOURCE
#include <dirent.h>     /* Defines DT_* constants */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <limits.h>

#define handle_error(msg)					\
	do { perror(msg); exit(EXIT_FAILURE); } while (0)

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
static long iseen;
static long iflagged;

void
list(char *prefix, char *file)
{
	if (flagset) {
		int sum = 0;
		char *f = strstr(file, ":2,");
		if (!f)
			return;
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
		icount++;
		while (*f) {
			if (*f == 'S')
				iseen++;
			if (*f == 'F')
				iflagged++;
			f++;
		}
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

struct linux_dirent64 {
	ino64_t        d_ino;    /* 64-bit inode number */
	off64_t        d_off;    /* 64-bit offset to next structure */
	unsigned short d_reclen; /* Size of this dirent */
	unsigned char  d_type;   /* File type */
	char           d_name[]; /* Filename (null-terminated) */
};
#define BUF_SIZE 1024000

void
listdir(char *dir)
{
	int fd, nread;
	char buf[BUF_SIZE];
	struct linux_dirent64 *d;
	int bpos;

	fd = open(dir, O_RDONLY | O_DIRECTORY);
	if (fd == -1) {
		perror("open");
		return;
	}
	
	while (1) {
		nread = syscall(SYS_getdents64, fd, buf, BUF_SIZE);
		if (nread == -1)
			handle_error("getdents64");
		
		if (nread == 0)
			break;

		for (bpos = 0; bpos < nread;) {
			d = (struct linux_dirent64 *) (buf + bpos);
			if (d->d_type != DT_REG && d->d_type != DT_UNKNOWN)
				goto next;
			if (d->d_name[0] == '.')
				goto next;
			list(dir, d->d_name);
next:
			bpos += d->d_reclen;
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

int
main(int argc, char *argv[])
{
	int c;
	while ((c = getopt(argc, argv, "PRSTDFprstdfNnCcim:")) != -1)
		switch(c) {
		case 'P': case 'R': case 'S': case 'T': case 'D': case 'F':
			flags[(unsigned int)c] = 1;
			break;
		case 'p': case 'r': case 's': case 't': case 'd': case 'f':
			flags[(unsigned int)uc(c)] = -1;
			break;
		case 'N': Nflag = 1; break;
		case 'n': Nflag = -1; break;
		case 'C': Cflag = 1; break;
		case 'c': Cflag = -1; break;
		case 'i': iflag = 1; break;
		case 'm': // XXX todo
		default:
			// XXX usage
			exit(1);
		}

        int i;
	
	for (i = 0, flagsum = 0, flagset = 0; i < sizeof flags; i++) {
		if (flags[i] != 0)
			flagset++;
		if (flags[i] == 1)
			flagsum++;
	}

        for (i = optind; i < argc; i++) {
		struct stat st;
		if (stat(argv[i], &st) < 0)
			continue;
		if (S_ISDIR(st.st_mode)) {
			char subdir[PATH_MAX];
			struct stat st2;
			int maildir = 0;

			long gcount = icount;
			long gseen = iseen;
			long gflagged = iflagged;

			icount = 0;
			iseen = 0;
			iflagged = 0;

			snprintf(subdir, sizeof subdir, "%s/cur", argv[i]);
			if (stat(subdir, &st2) == 0) {
				maildir = 1;
				if (Cflag >= 0 && Nflag <= 0)
					listdir(subdir);
			}

			snprintf(subdir, sizeof subdir, "%s/new", argv[i]);
			if (stat(subdir, &st2) == 0) {
				maildir = 1;
				if (Nflag >= 0 && Cflag <= 0)
					listdir(subdir);
			}

			if (!maildir)
				listdir(argv[i]);

			if (iflag && icount)
				printf("%6ld unseen  %3ld flagged  %6ld msg  %s\n",
				    icount-iseen, iflagged, icount, argv[i]);

			icount = gcount;
			iseen = gseen;
			iflagged = gflagged;
		} else if (S_ISREG(st.st_mode)) {
			list(0, argv[i]);
		}
        }

	if (icount || iseen || iflagged)
		printf("%6ld unseen  %3ld flagged  %6ld msg\n",
		    icount-iseen, iflagged, icount);

exit(0);

	exit(EXIT_SUCCESS);
}
