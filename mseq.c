#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>

char *
seq_open(char *file)
{
	int fd;
	struct stat st;

	// env $SEQ or something
	if (!file)
		file = "map";
	fd = open(file, O_RDONLY);
	if (!fd)
		return 0;

	fstat(fd, &st);
	char *map = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);

	if (map == MAP_FAILED)
		return 0;

	return map;
}

static const char *
readlin(const char *p)
{
        static char b[PATH_MAX];
        int r = readlink(p, b, sizeof b - 1);
        if (r < 0)
		r = 0;
        b[r] = 0;
        return b;
}

char *
parse_relnum(char *a, long cur, long last, long *out)
{
	long base;
	char *b;

	if (strcmp(a, "+") == 0)
		a = ".+1";
	else if (strcmp(a, "-") == 0)
		a = ".-1";
	else if (strcmp(a, "$") == 0)
		a = "-1";
	
	if (*a == '.') {
		a++;
		base = cur;
	} else if (*a == '-') {
		base = last + 1;
	} else {
		base = 0;
	}
	errno = 0;
	long d = strtol(a, &b, 10);
	if (errno != 0) {
		perror("strtol");
		exit(1);
	}

	*out = base + d;
	if (*out <= 0)
		*out = 1;
	if (*out > last)
		*out = last;
	
	return b;
}

int
main(int argc, char *argv[])
{
	char *map = seq_open(0);
	if (!map)
		return 1;

	long lines;
	long cur = 0;

	const char *curfile = readlin("map.cur");
	char *curpath;

	char *s, *t;
	for (s = map, lines = 0; s; s = t) {
		t = strchr(s+1, '\n');
		if (!t)
			break;
		s++;
//		printf("{%.*s}\n", t-s, s);
		lines++;
		// XXX compare modulo whitespace
		if (!cur &&
		    strncmp(s, curfile, strlen(curfile)) == 0 &&
		    s[strlen(curfile)] == '\n')
			cur = lines;
	}
	lines--;

	long i, start, stop;
	for (i = 1; i < argc; i++) {
		char *a = argv[i];
		start = stop = 0;

		while (*a && *a != ':') {
			char *b = parse_relnum(a, cur, lines, &start);
			if (a == b) {
				printf("huh\n");
				exit(1);
			}
			a = b;
		}
		if (*a == ':') {
			a++;
			if (!*a) {
				stop = lines;
			} else {
				char *b = parse_relnum(a, cur, lines, &stop);
				if (a == b) {
					printf("huh\n");
					exit(1);
				}
				a = b;
			}
		} else if (!*a) {
			stop = start;
		} else {
			printf("huh\n");
			exit(1);
		}

		long line;
		for (s = map, line = 0; s; s = t) {
			t = strchr(s+1, '\n');
			if (!t)
				break;
			s++;
			line++;

			if (line >= start && line <= stop) {
				fwrite(s, 1, t-s, stdout);
				putchar('\n');
				cur = line;
				curpath = s;
				while (*curpath == ' ')
					curpath++;
			}
		}
	}

/*
	char *e = strchr(curpath, '\n');
	unlink("map.cur-");
	symlink(strndup(curpath, e-curpath), "map.cur-");
	rename("map.cur-", "map.cur");
*/

	return 0;
}
