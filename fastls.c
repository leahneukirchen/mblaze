#define _GNU_SOURCE
#include <dirent.h>     /* Defines DT_* constants */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#define handle_error(msg)					\
	do { perror(msg); exit(EXIT_FAILURE); } while (0)

struct linux_dirent64 {
	ino64_t        d_ino;    /* 64-bit inode number */
	off64_t        d_off;    /* 64-bit offset to next structure */
	unsigned short d_reclen; /* Size of this dirent */
	unsigned char  d_type;   /* File type */
	char           d_name[]; /* Filename (null-terminated) */
};


#define BUF_SIZE 1024000

void flagsort(char *s) {
        int i, j;
	if (!s[0])
		return;
	// adapted insertion sort from http://stackoverflow.com/a/2789530
        for (i = 1; s[i]; i++) {
                char t = s[i];
                for (j = i; j >= 1 && t < s[j-1]; j--)
                        s[j] = s[j-1];
                s[j] = t;
        }
}

int
main(int argc, char *argv[])
{
	int fd, nread;
	char buf[BUF_SIZE];
	struct linux_dirent64 *d;
	int bpos;
	char d_type;
	
	fd = open(argc > 1 ? argv[1] : ".", O_RDONLY | O_DIRECTORY);
	if (fd == -1)
		handle_error("open");
	
	while (1) {
		nread = syscall(SYS_getdents64, fd, buf, BUF_SIZE);
		if (nread == -1)
			handle_error("getdents64");
		
		if (nread == 0)
			break;

		printf("--------------- nread=%d ---------------\n", nread);
		printf("inode#    file type  d_reclen  d_off   d_name\n");
		for (bpos = 0; bpos < nread;) {
			d = (struct linux_dirent64 *) (buf + bpos);
			if (d->d_type != DT_REG)
				goto next;
			if (d->d_name[0] == '.')
				goto next;
			char *flags = strstr(d->d_name, ":2,");
			if (!flags)
				goto next;
//			if (strncmp(d->d_name, "1467836925_0.1439.juno,U=87588,FMD5=7e33429f656f1", strlen("1467836925_0.1439.juno,U=87588,FMD5=7e33429f656f1")-1))
//				goto next;
//			printf("%s\n", d->d_name);
			flagsort(flags+3);
			printf("%s\n", d->d_name);
//			if (!strchr(flags, 'S'))
//				printf("%s\n", d->d_name);

//			    goto next;

/*
			printf("%-10s ", (d_type == DT_REG) ?  "regular" :
			    (d_type == DT_DIR) ?  "directory" :
			    (d_type == DT_FIFO) ? "FIFO" :
			    (d_type == DT_SOCK) ? "socket" :
			    (d_type == DT_LNK) ?  "symlink" :
			    (d_type == DT_BLK) ?  "block dev" :
			    (d_type == DT_CHR) ?  "char dev" : "???");
			printf("%4d %10lld  %s\n", d->d_reclen,
			    (long long) d->d_off, d->d_name);
*/
next:
			bpos += d->d_reclen;
		}
	}

	exit(EXIT_SUCCESS);
}
