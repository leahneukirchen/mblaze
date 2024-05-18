/* jwz-style threading
 * clean-room implementation of https://www.jwz.org/doc/threading.html
 * without looking at any code
 *
 * subject threading and sibling sorting is not done yet
 */

#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <search.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "blaze822.h"
#include "xpledge.h"

static int vflag;
static int pflag;
static int rflag;
static int optional;

struct container {
	char *mid;
	char *file;
	time_t date;
	struct container *parent;
	struct container *child;
	struct container *next;
	int optional;
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
			return strdup(v);
		v = strchr(m, '>');
		if (!v)
			return strdup(m);
		return strndup(m+1, v-m-1);
	} else {
		// invent new message-id for internal tracking
		static long i;
		char buf[40];
		snprintf(buf, sizeof buf, "thread%08ld@localhost", ++i);
		return strdup(buf);
	}
}

struct container *
midcont(char *mid)
{
	struct container key, **result;
	if (!mid)
		exit(111);
	key.mid = mid;

	if (!(result = tfind(&key, &mids, midorder))) {
		struct container *c = malloc(sizeof (struct container));
		if (!c)
			exit(111);
		c->mid = mid;
		c->file = 0;
		c->date = -1;
		c->optional = 0;
		c->parent = c->child = c->next = 0;
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
	c->file = strdup(file);
	c->optional = optional;

	return c;
}

int
reachable(struct container *child, struct container *parent)
{
	int r = 0;

	if (strcmp(child->mid, parent->mid) == 0)
		return 1;
	if (child->child)
		r |= reachable(child->child, parent);
	if (child->next)
		r |= reachable(child->next, parent);
	return r;
}

void
thread(char *file)
{
	struct message *msg;

	while (*file == ' ' || *file == '\t')
		file++;

	msg = blaze822(file);
	if (!msg)
		return;

	struct container *c = store_id(file, msg);

	char *mid = "";

	char *v, *m;
	struct container *parent = 0, *me = 0;

	if ((v = blaze822_hdr(msg, "date"))) {
		c->date = blaze822_date(v);
	} else {
		c->date = -1;
	}

	v = blaze822_hdr(msg, "references");
	if (v) {
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

			me = midcont(mid);

			if (me == c)
				continue;

			if (parent && !me->parent &&
			    !reachable(me, parent) && !reachable(parent, me)) {
				me->parent = parent;
				me->next = parent->child;
				parent->child = me;
			}

			parent = me;
		}
	}

	v = blaze822_hdr(msg, "in-reply-to");
	char *irt;
	if (v) {
		m = strchr(v, '<');
		if (!m)
			goto out;
		v = strchr(m, '>');
		if (!v)
			goto out;
		irt = strndup(m+1, v-m-1);

		if (strcmp(irt, mid) != 0) {
			parent = midcont(irt);
		} else {
			free(irt);
		}
	}
out:

	if (parent && parent != c) {
		struct container *r;

		// check we don't introduce a new loop
		if (reachable(parent, c) || reachable(c, parent))
			goto out2;

		if (c->parent == parent) { // already correct
			goto out2;
		} else if (c->parent) {
			// if we already have a wrong parent, orphan us first

			if (c->parent->child == c)   // first in list
				c->parent->child = c->parent->child->next;
			for (r = c->parent->child; r; r = r->next) {
				if (r->next == c)
					r->next = c->next;
			}

			c->next = 0;
		}

		c->parent = parent;

		// add at the end
		if (!parent->child) {
			parent->child = c;
		} else {
			for (r = parent->child; r && r->next; r = r->next)
				if (r == c)
					goto out2;
			r->next = c;
			c->next = 0;
		}

out2:
		// someone said our parent was our child, a lie
		if (c->child == c->parent) {
			c->child->parent = 0;
			c->child = 0;
		}
	}

	blaze822_free(msg);
}

time_t
newest(struct container *c, int depth)
{
	time_t n = -1;

	if (!c)
		return n;

	do {
		if (c->child) {
			time_t r = newest(c->child, depth+1);
			if (n < r)
				n = r;
		}
		if (n < c->date)
			n = c->date;
	} while ((c = c->next));

	return n;
}

struct container *top;
struct container *lastc;

void
find_root(const void *nodep, const VISIT which, const int depth)
{
	(void)depth;

	if (which == preorder || which == leaf) {
		struct container *c = *(struct container **)nodep;

		if (!c->parent) {
			lastc->next = c;
			c->next = 0;
			time_t r = newest(c->child, 0);
			if (c->date < r)
				c->date = r;
			lastc = c;
		}
	}
}

void
find_roots()
{
	top = malloc(sizeof (struct container));
	if (!top)
		exit(111);
	top->date = -1;
	top->file = 0;
	top->next = top->child = top->parent = 0;
	top->optional = 0;
	top->mid = "(top)";

	lastc = top;

	twalk(mids, find_root);

	top->child = top->next;
	top->next = 0;
}

void
prune_tree(struct container *c, int depth)
{
	do {
		if (c->child)
			prune_tree(c->child, depth+1);
		if (depth >= 0 && !c->file && c->child && !c->child->next) {
			// turn into child if we don't exist and only have a child
			c->mid = c->child->mid;
			c->file = c->child->file;
			if (c->child->date > c->date)
				c->date = c->child->date;
			c->optional = c->child->optional;
			c->child = c->child->child;
		}
	} while ((c = c->next));
}

int
alloptional(struct container *c)
{
	do {
		if (!c->optional && c->file)
			return 0;
		if (c->child && !alloptional(c->child))
			return 0;
	} while ((c = c->next));

	return 1;
}

static int
dateorder(const void *a, const void *b)
{
	struct container *ia = *(struct container **)a;
	struct container *ib = *(struct container **)b;

	if (ia->date < ib->date)
		return -1;
	else if (ia->date > ib->date)
		return 1;
	return 0;
}

static int
reverse_dateorder(const void *a, const void *b)
{
	return dateorder(b, a);
}

void
sort_tree(struct container *c, int depth)
{
	if (c && c->child) {
		struct container *r;
		int i, j;
		for (r = c->child, i = 0; r; r = r->next, i++)
			sort_tree(r, depth+1);

		if (i == 1)  // no sort needed
			return;

		struct container **a = calloc(i, sizeof (struct container *));
		if (!a)
			return;

		for (r = c->child, i = 0; r; r = r->next, i++)
			a[i] = r;

		qsort(a, i, sizeof (struct container *),
		    rflag && depth < 0 ? reverse_dateorder : dateorder);

		c->child = a[0];
		for (j = 0; j+1 < i; j++)
			a[j]->next = a[j+1];
		a[i-1]->next = 0;

		free(a);
	}
}

void
print_tree(struct container *c, int depth)
{
	do {
		// skip toplevel threads when they are unresolved or all optional
		// (or when -p is given, skip those subthreads)
		if ((depth <= 1 || pflag) &&
		    (c->optional || !c->file) &&
		    (!c->child || alloptional(c->child)))
			continue;

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
	} while ((c = c->next));
}

int
main(int argc, char *argv[])
{
	int c;

	optional = 1;

	xpledge("stdio rpath", "");

	while ((c = getopt(argc, argv, "S:prv")) != -1)
		switch (c) {
		case 'S': blaze822_loop1(optarg, thread); break;
		case 'v': vflag = 1; break;
		case 'p': pflag = 1; break;
		case 'r': rflag = 1; break;
		default:
			fprintf(stderr, "Usage: mthread [-vpr] [-S dir] [msgs...]\n");
			exit(1);
		}

	optional = 0;

	if (argc == optind && isatty(0))
		blaze822_loop1(":", thread);
	else
		blaze822_loop(argc-optind, argv+optind, thread);

	// the tree of all toplevel threads has depth -1,
	// so toplevel threads have depth 0.

	find_roots();
	if (!vflag)
		prune_tree(top, -1);
	sort_tree(top, -1);
	print_tree(top, -1);

	return 0;
}
