#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blaze822.h"
#include "blaze822_priv.h"

char *
blaze822_home_file(char *basename)
{
	static char path[PATH_MAX];
	static char *homedir;

	if (!homedir)
		homedir = getenv("HOME");
	if (!homedir)
		homedir = getpwuid(getuid())->pw_dir;

	snprintf(path, sizeof path, "%s/%s", homedir, basename);

	return path;
}

char *
blaze822_seq_open(char *file)
{
	int fd;
	struct stat st;

	// env $SEQ or something
	if (!file)
		file = getenv("MAILMAP");
	if (!file)
		file = blaze822_home_file(".santoku/map");
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

static void *msgnums;

struct msgnum {
	char *file;
	long pos;
};

int
msgnumorder(const void *a, const void *b)
{
        struct msgnum *ia = (struct msgnum *)a;
        struct msgnum *ib = (struct msgnum *)b;

        return strcmp(ia->file, ib->file);
}

long
blaze822_seq_find(char *file)
{
	struct msgnum key, **result;
        key.file = file;

        if (!(result = tfind(&key, &msgnums, msgnumorder)))
		return 0;

	return (*result)->pos;
}

int
blaze822_seq_load(char *map)
{
	char *s, *t;
	long line;

	for (s = map, line = 0; s; s = t+1) {
		t = strchr(s, '\n');
		if (!t)
			break;
		line++;
		while (*s && iswsp(*s))
			s++;
		char *e = t;
		while (s < e && isfws(*(e-1)))
			e--;
//		printf("{%.*s}\n", e-s, s);
		char *f = strndup(s, e-s);
		if (!f)
			return -1;

		struct msgnum key, **result;
		key.file = f;

		if (!(result = tfind(&key, &msgnums, msgnumorder))) {
			struct msgnum *c = malloc(sizeof (struct msgnum));
			c->file = f;
			c->pos = line;
			tsearch(c, &msgnums, msgnumorder);
		}
	}

	return 0;
}

char *
blaze822_seq_cur()
{
        static char b[PATH_MAX];

	char *curlink = getenv("MAILCUR");
	if (!curlink)
		curlink = blaze822_home_file(".santoku/cur");

	int r = readlink(curlink, b, sizeof b - 1);
	if (r < 0)
		return 0;
	b[r] = 0;
	return b;
}

int
blaze822_seq_setcur(char *s)
{
	char curtmplink[PATH_MAX];
	char *curlink = getenv("MAILCUR");
	if (!curlink)
		curlink = blaze822_home_file(".santoku/cur");

	if (snprintf(curtmplink, sizeof curtmplink, "%s-", curlink) >= PATH_MAX)
		return -1;  // truncation

	if (unlink(curtmplink) < 0 && errno != ENOENT)
		return -1;
	if (symlink(s, curtmplink) < 0)
		return -1;
	if (rename(curtmplink, curlink) < 0)
		return -1;
	return 0;
}

static char *
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

static int
parse_range(char *a, long *start, long *stop, long cur, long lines)
{
	*start = *stop = 1;

	while (*a && *a != ':') {
		char *b = parse_relnum(a, cur, lines, start);
		if (a == b)
			return 0;
		a = b;
	}
	if (*a == ':') {
		a++;
		if (!*a) {
			*stop = lines;
		} else {
			char *b = parse_relnum(a, cur, lines, stop);
			if (a == b)
				return 0;
		}
	} else if (!*a) {
		*stop = *start;
	} else {
		return 0;
	}

	return 1;
}

void
find_cur(char *map, struct blaze822_seq_iter *iter)
{
	char *s, *t;
	long cur = 0;
	const char *curfile = blaze822_seq_cur();

	iter->lines = 0;
	for (s = map; s; s = t+1) {
		t = strchr(s, '\n');
		if (!t)
			break;
		while (*s == ' ' || *s == '\t')
			s++;

//		printf("{%.*s}\n", t-s, s);
		iter->lines++;
		if (!cur && curfile &&
		    strncmp(s, curfile, strlen(curfile)) == 0 &&
		    (s[strlen(curfile)] == '\n' ||
		    s[strlen(curfile)] == ' ' ||
		    s[strlen(curfile)] == '\t'))
			iter->cur = iter->lines;
	}
}

char *
blaze822_seq_next(char *map, char *range, struct blaze822_seq_iter *iter)
{
	if (!map)
		return 0;

	if (!iter->lines)  // count total lines
		find_cur(map, iter);

	if (!iter->start) {
		if (!parse_range(range, &iter->start, &iter->stop,
				 iter->cur, iter->lines)) {
			fprintf(stderr, "can't parse range: %s\n", range);
			return 0;
		}

		iter->s = map;
		iter->line = 1;
	}

	if (!iter->s)
		return 0;

	while (iter->line < iter->start) {
		char *t = strchr(iter->s, '\n');
		if (!t)
			return 0;
		iter->line++;
		iter->s = t + 1;
	}

	if (iter->line > iter->stop) {
		iter->start = iter->stop = 0;  // reset iteration
		return 0;
	}

	char *t = strchr(iter->s, '\n');
	if (!t)
		return 0;
	iter->cur = iter->line;
	iter->line++;
	char *r = strndup(iter->s, t-iter->s);
	iter->s = t + 1;
	return r;
}

static void
iterfile(char *dir, void (*cb)(char *))
{
	DIR *fd, *fd2;
        struct dirent *d;

	fd = opendir(dir);
	if (!fd) {
		if (errno == ENOTDIR)
			cb(dir);
		return;
	}

	char sub[PATH_MAX];
	snprintf(sub, sizeof sub, "%s/cur", dir);
	fd2 = opendir(sub);
	if (fd2) {
		closedir(fd);
		fd = fd2;
	}

	while ((d = readdir(fd))) {
		if (d->d_type != DT_REG && d->d_type != DT_UNKNOWN)
			continue;
		if (d->d_name[0] == '.')
			continue;
		if (fd2)
			snprintf(sub, sizeof sub, "%s/cur/%s", dir, d->d_name);
		else
			snprintf(sub, sizeof sub, "%s/%s", dir, d->d_name);
		cb(sub);
	}
	closedir(fd);
}

int
blaze822_loop(int argc, char *argv[], void (*cb)(char *))
{
	char *line = 0;
	size_t linelen = 0;
	ssize_t rd;
	int i = 0;

	if (argc == 0) {
		while ((rd = getdelim(&line, &linelen, '\n', stdin)) != -1) {
			if (line[rd-1] == '\n')
				line[rd-1] = 0;
			cb(line);
			i++;
		}
		free(line);
		return i;
	}

	char *map = blaze822_seq_open(0);
	struct blaze822_seq_iter iter = { 0 };
	int j = 0;
	for (i = 0; i < argc; i++) {
		if (strchr(argv[i], '/')) {  // a file name
			iterfile(argv[i], cb);
			j++;
		} else {
			while ((line = blaze822_seq_next(map, argv[i], &iter))) {
				cb(line);
				free(line);
				j++;
			}
		}
	}
	return j;
}

int
blaze822_loop1(char *arg, void (*cb)(char *))
{
	char *args[] = { arg };
	return blaze822_loop(1, args, cb);
}
