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
#include <unistd.h>

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
			return strdup(v);
		v = strchr(m, '>');
		if (!v)
			return strdup(m);
		return strndup(m+1, v-m-1);
	} else {
		// invent new message-id for internal tracking
		static long i;
		char buf[32];
		snprintf(buf, sizeof buf, "thread%08ld@localhost", ++i);
		return strdup(buf);
	}
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
	c->msg = msg;

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

	msg = blaze822(file);
	if (!msg)
		return;

	struct container *c = store_id(file, msg);

	char *mid = "";

	char *v, *m;
	struct container *parent = 0, *me = 0;

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
		}
	}
out:

	if (parent && parent != c) {
		struct container *r;

		if (c->parent == parent) { // already correct
			goto out2;
		} else if (c->parent) {
			// if we already have a wrong parent, orphan us first

			if (c->parent->child == c)   // first in list
				c->parent->child = c->next;
			for (r = c->parent->child; r; r = r->next) {
				if (r->next == c)
					r->next = c->next;
			}
		}

		c->parent = parent;

		// add at the end
		for (r = parent->child; r && r->next; r = r->next)
			;
		if (!r) {
			parent->child = c;
		} else {
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
			lastc = c;
		}
	}
}

void
find_roots()
{
	top = malloc (sizeof (struct container));
	top->msg = 0;
	top->file = 0;
	top->next = top->child = top->parent = 0;
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
			c->msg = c->child->msg;
			c->child = c->child->child;
		}
	} while ((c = c->next));
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
	} while ((c = c->next));
}

int
main(int argc, char *argv[])
{
	int i = blaze822_loop(argc-1, argv+1, thread);

	find_roots();
	prune_tree(top, -1);
	print_tree(top, -1);

	fprintf(stderr, "%d mails threaded\n", i);

	return 0;
}
