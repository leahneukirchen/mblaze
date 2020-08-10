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
	static char *profile;

	if (!profile)
		profile = getenv("MBLAZE");
	if (profile) {
		snprintf(path, sizeof path, "%s/%s", profile, basename);
		return path;
	}

	if (!homedir)
		homedir = getenv("HOME");
	if (homedir && !*homedir)
		homedir = 0;
	if (!homedir) {
		struct passwd *pw = getpwuid(getuid());
		if (pw)
			homedir = pw->pw_dir;
	}
	if (!homedir)
		return "/dev/null/homeless";

	snprintf(path, sizeof path, "%s/.mblaze/%s", homedir, basename);

	return path;
}

char *
blaze822_seq_open(char *file)
{
	if (!file)
		file = getenv("MAILSEQ");
	if (!file)
		file = blaze822_home_file("seq");

	char *map;
	off_t len;
	int r = slurp(file, &map, &len);
	if (r != 0) {
		fprintf(stderr, "could not read sequence '%s': %s\n",
		    file, strerror(r));
		return 0;
	}

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
		char *f = strndup(s, e-s);
		if (!f)
			return -1;

		struct msgnum key;
		key.file = f;

		if (!tfind(&key, &msgnums, msgnumorder)) {
			struct msgnum *c = malloc(sizeof (struct msgnum));
			c->file = f;
			c->pos = line;
			tsearch(c, &msgnums, msgnumorder);
		}
	}

	return 0;
}

char *
blaze822_seq_cur(void)
{
	static char b[PATH_MAX];

	char *override = getenv("MAILDOT");
	if (override)
		return override;

	char *curlink = getenv("MAILCUR");
	if (!curlink)
		curlink = blaze822_home_file("cur");

	ssize_t r = readlink(curlink, b, sizeof b - 1);
	if (r < 0)
		return 0;
	b[r] = 0;
	return b;
}

int
blaze822_seq_setcur(char *s)
{
	if (strcmp(s, "/dev/stdin") == 0)
		return 0;

	char *override = getenv("MAILDOT");
	if (override)
		return 0;

	char curtmplink[PATH_MAX];
	char *curlink = getenv("MAILCUR");
	if (!curlink)
		curlink = blaze822_home_file("cur");

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
parse_relnum(char *a, long cur, long start, long last, long *out)
{
	long base;
	char *b;

	if (strcmp(a, "+") == 0)
		a = ".+1";
	else if (strcmp(a, ".-") == 0)
		a = ".-1";
	else if (strcmp(a, ".") == 0)
		a = ".+0";

	if (*a == '$') {
		a++;
		base = last;
	} else if (*a == '.') {
		a++;
		base = cur;
	} else if (*a == '-') {
		base = last + 1;
	} else if (*a == '+') {
		base = start;
	} else {
		base = 0;
	}

	long d;
	if (strchr(":=_^", *a)) {
		d = 0;
		b = a;
	} else {
		errno = 0;
		d = strtol(a, &b, 10);
		if (errno != 0) {
			perror("strtol");
			exit(2);
		}
	}

	*out = base + d;
	if (*out <= 0)
		*out = 1;
	if (*out > last)
		*out = last;

	return b;
}

static int
parse_thread(char *map, long a, long *starto, long *stopo)
{
	char *s, *t;
	long line;

	long start = 0, stop = 0, state = 0;

	for (s = map, line = 0; s; s = t+1) {
		t = strchr(s, '\n');
		if (!t)
			break;
		line++;
		if (!iswsp(*s)) {
			if (state == 0) {
				start = line;
			} else if (state == 1) {
				stop = line - 1;
				state = 2;
				break;
			}
		}
		if (line == a)
			state = 1;
		while (*s && iswsp(*s))
			s++;
	}

	if (state == 1) {
		stop = line;
		state = 2;
	}
	if (state == 2) {
		*starto = start;
		*stopo = stop;
		return 0;
	}
	return 1;
}

static int
parse_subthread(char *map, long a, long *stopo)
{
	char *s, *t;
	long line;

	long stop = 0;
	int minindent = -1;

	for (s = map, line = 0; s; s = t+1) {
		t = strchr(s, '\n');
		if (!t) {
			minindent = -1;
			break;
		}
		line++;
		int indent = 0;
		while (*s && iswsp(*s)) {
			s++;
			indent++;
		}
		if (line == a)
			minindent = indent;
		if (line > a && indent <= minindent) {
			stop = line - 1;
			break;
		}
	}

	if (line < a)
		return 1;

	if (minindent == -1)
		stop = line;

	*stopo = stop;

	return 0;
}

static int
parse_parent(char *map, long *starto, long *stopo)
{
	char *s, *t;
	long line;

	long previndent[256] = { 0 };

	for (s = map, line = 0; s; s = t+1) {
		t = strchr(s, '\n');
		if (!t)
			break;
		line++;
		long indent = 0;
		while (*s && iswsp(*s)) {
			s++;
			indent++;
		}
		if (indent > 255)
			indent = 255;
		previndent[indent] = line;
		if (line == *starto) {
			if (previndent[indent-1]) {
				*starto = *stopo = previndent[indent-1];
				return 0;
			} else {
				return 1;
			}
		}
	}

	return 1;
}

static int
parse_range(char *map, char *a, long *start, long *stop, long cur, long lines)
{
	*start = 0;
	*stop = 1;

	while (*a && *a != ':' && *a != '=' && *a != '_' && *a != '^') {
		char *b = parse_relnum(a, cur, 0, lines, start);
		if (a == b)
			return 1;
		a = b;
	}
	if (*start == 0)
		*start = strchr("=^_", *a) ? cur : 1;

	while (*a == '^') {
		a++;
		if (parse_parent(map, start, stop))
			return 2;
	}

	if (*a == ':') {
		a++;
		if (!*a) {
			*stop = lines;
		} else {
			char *b = parse_relnum(a, cur, *start, lines, stop);
			if (a == b)
				return 1;
		}
	} else if (*a == '=') {
		return parse_thread(map, *start, start, stop);
	} else if (*a == '_') {
		return parse_subthread(map, *start, stop);
	} else if (!*a) {
		*stop = *start;
	} else {
		return 1;
	}

	return 0;
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
	if (strcmp(range, ".") == 0) {
		if (!iter->lines && !iter->start) {
			iter->lines = 1;
			find_cur(map, iter);
			iter->start = iter->stop = iter->line = iter->cur + 1;
			char *cur = blaze822_seq_cur();
			return cur ? strdup(cur) : 0;
		} else {
			return 0;
		}
	}

	if (!map)
		return 0;

	if (!iter->lines)  // count total lines
		find_cur(map, iter);

	if (!iter->s) {
		int ret = parse_range(map, range, &iter->start, &iter->stop,
		    iter->cur, iter->lines);
		if (ret == 1) {
			fprintf(stderr, "can't parse range: %s\n", range);
			return 0;
		} else if (ret == 2) {
			fprintf(stderr, "message not found for specified range: %s\n", range);
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

int mystrverscmp(const char *, const char *);

static int
mailsort(const struct dirent **a, const struct dirent **b)
{
	return mystrverscmp((*a)->d_name, (*b)->d_name);
}

static long
iterdir(char *dir, void (*cb)(char *))
{
	struct dirent **namelist;

	int n;

	char sub[PATH_MAX];
	snprintf(sub, sizeof sub, "%s/cur", dir);

	char *m = "/cur";

	n = scandir(sub, &namelist, 0, mailsort);
	if (n == -1 && (errno == ENOENT || errno == ENOTDIR)) {
		m = "";
		n = scandir(dir, &namelist, 0, mailsort);
	}
		
	if (n == -1) {	
		if (errno == ENOTDIR)
			cb(dir);
		return 1;
	}

	long i = 0;
	for (i = 0; i < n; i++) {
		if (namelist[i]->d_name[0] != '.' &&
		    MAIL_DT(namelist[i]->d_type)) {
			snprintf(sub, sizeof sub, "%s%s/%s",
			    dir, m, namelist[i]->d_name);
			cb(sub);
		}
		free(namelist[i]);
	}
	free(namelist);

	return i;
}

long
blaze822_loop(int argc, char *argv[], void (*cb)(char *))
{
	char *line = 0;
	size_t linelen = 0;
	ssize_t rd;
	long i = 0;

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

	char *map = 0;
	int map_opened = 0;
	struct blaze822_seq_iter iter = { 0 };
	int j = 0;
	for (i = 0; i < argc; i++) {
		if (strchr(argv[i], '/')) {  // a file name
			j += iterdir(argv[i], cb);
		} else if (strcmp(argv[i], "-") == 0) {
			if (isatty(0)) {
				fprintf(stderr, "mblaze: warning: - now means "
				    "read mail text from standard input, "
				    "use .- to refer to previous mail\n");
			}
			cb("/dev/stdin");
			j++;
		} else {
			if (!map_opened) {
				map = blaze822_seq_open(0);
				map_opened = 1;
			}
			while ((line = blaze822_seq_next(map, argv[i], &iter))) {
				cb(line);
				free(line);
				j++;
			}
		}
	}
	free(map);
	return j;
}

long
blaze822_loop1(char *arg, void (*cb)(char *))
{
	char *args[] = { arg };
	return blaze822_loop(1, args, cb);
}
