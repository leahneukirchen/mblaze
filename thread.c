/* jwz-style threading
 * clean-room implementation of https://www.jwz.org/doc/threading.html
 * without looking at any code
 *
 * subject threading and sibling sorting is not done yet
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <wchar.h>

#include <search.h>


#include "blaze822.h"

struct container {
        char *mid;
	char *file;
	struct message *msg;
	struct container *parent;
	struct container *child;
	struct container *next;
};

static void *mids;

int
midorder(const void *a, const void *b)
{
        struct container *ia = (struct container *)a;
        struct container *ib = (struct container *)b;

	return strcmp(ia->mid, ib->mid);
}

char *
mid(struct message *msg)
{
	char *v;
	v = blaze822_hdr(msg, "message-id");
	// XXX intern mid?
	if (v) {
		char *m;

		m = strchr(v, '<');
		if (!m)
			return 0;
		v = strchr(m, '>');
		if (!v)
			return 0;
		return strndup(m+1, v-m-1);
	}
	else
		return 0;
}

struct container *
midcont(char *mid)
{
	struct container key, **result;
	key.mid = mid;

	if (!(result = tfind(&key, &mids, midorder))) {
		struct container *c = malloc(sizeof (struct container));
		c->mid = mid;
		c->file = 0;
		c->msg = 0;
		c->parent = c->child = c->next = 0;
		printf("ADD %s\n", mid);
		return *(struct container **)tsearch(c, &mids, midorder);
	} else {
		return *result;
	}
}

struct container *
store_id(char *file, struct message *msg)
{
	struct container *c;

	c = midcont(mid(msg));
	c->file = file;
	c->msg = msg;

	return c;
}

void
thread(char *file)
{
	struct message *msg;

	msg = blaze822(file);
	if (!msg)
		return;

	struct container *c = store_id(file, msg);

	char *mid = "";

	char *v;
	struct container *parent = 0, *me = 0;

	v = blaze822_hdr(msg, "references");
	if (v) {
		char *m, *n;
		parent = 0;
		while (1) {
			m = strchr(v, '<');
			if (!m)
				break;
			v = strchr(m, '>');
			if (!v)
				break;
			mid = strndup(m+1, v-m-1);
			// XXX free?

			printf("ref |%s|\n", mid);

			me = midcont(mid);
			if (parent && !me->parent) {
				me->parent = me;
				parent->child = me;
			}

			parent = me;
		}
	}
	
	v = blaze822_hdr(msg, "in-reply-to");
	char *irt;
	if (v) {
		char *m, *n;
		m = strchr(v, '<');
		if (!m)
			goto out;
		v = strchr(m, '>');
		if (!v)
			goto out;
		irt = strndup(m+1, v-m-1);
		printf("irt |%s|\n", irt);
		
		if (strcmp(irt, mid) != 0) {
			parent = midcont(irt);
		}
	}
out:

	if (parent) {
		c->parent = parent;
		parent->child = c;
	}
}

struct container *top;
struct container *lastc;

void
find_root(const void *nodep, const VISIT which, const int depth)
{
        (void)depth;

        if (which == postorder || which == leaf) {
		struct container *c = *(struct container **)nodep;

		if (!c->parent) {
//			printf("%s\n", c->mid);
			printf("root %s\n", c->mid);
			lastc->next = c;
			c->next = 0;
			lastc = c;
		}
	}
}

void
find_roots()
{
	top = malloc (sizeof (struct container));
	top->msg = 0;
	top->next = top->child = top->parent = 0;
	top->mid = "(top)";

	lastc = top;

	twalk(mids, find_root);
}

void
print_tree(struct container *c, int depth)
{
	do {
		if (depth >= 0) {
			int i;
			for (i = 0; i < depth; i++)
				printf(" ");
			if (c->file)
				printf("%s\n", c->file);
			else
				printf("<%s>\n", c->mid);
		}
	
		if (c->child)
			print_tree(c->child, depth+1);
	} while (c = c->next);
}

int
main(int argc, char *argv[])
{
	char *s;
	
	char *line = 0;
	size_t linelen = 0;
	int read;

	int i = 0;

	if (argc == 1 || (argc == 2 && strcmp(argv[1], "-") == 0)) {
		while ((read = getdelim(&line, &linelen, '\n', stdin)) != -1) {
			if (line[read-1] == '\n') line[read-1] = 0;
			printf("%s\n", line);
			thread(line);
			i++;
		}
	} else {
		for (i = 1; i < argc; i++) {
			thread(argv[i]);
		}
		i--;
	}

	printf("%d mails scanned\n", i);
	
	find_roots();

	print_tree(top, -1);

	return 0;
}
