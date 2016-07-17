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
#include <search.h>

#include "blaze822.h"
#include "blaze822_priv.h"

char *
blaze822_seq_open(char *file)
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
