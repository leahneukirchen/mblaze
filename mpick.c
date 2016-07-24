#define _GNU_SOURCE

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <locale.h>
#include <regex.h>
#include <fnmatch.h>

#include "blaze822.h"

enum op {
	EXPR_OR = 1,
	EXPR_AND,
	EXPR_NOT,
	EXPR_LT,
	EXPR_LE,
	EXPR_EQ,
	EXPR_NEQ,
	EXPR_GE,
	EXPR_GT,
	EXPR_STREQ,
	EXPR_STREQI,
	EXPR_GLOB,
	EXPR_GLOBI,
	EXPR_REGEX,
	EXPR_REGEXI,
	EXPR_PRUNE,
	EXPR_PRINT,
	EXPR_TYPE,
	EXPR_ALLSET,
	EXPR_ANYSET,
};

enum prop {
	PROP_ATIME = 1,
	PROP_CTIME,
	PROP_DEPTH,
	PROP_MTIME,
	PROP_PATH,
	PROP_REPLIES,
	PROP_SIZE,
	PROP_TOTAL,
	PROP_SUBJECT,
	PROP_FROM,
	PROP_FROM_NAME,
	PROP_FROM_ADDR,
	PROP_TO,
	PROP_TO_NAME,
	PROP_TO_ADDR,
	PROP_INDEX,
	PROP_DATE,
	PROP_FLAG,
};

enum flags {
	FLAG_PASSED = 1,
	FLAG_REPLIED = 2,
	FLAG_SEEN = 4,
	FLAG_TRASHED = 8,
	FLAG_DRAFT = 16,
	FLAG_FLAGGED = 32,
	/* custom */
	FLAG_NEW = 64,
	FLAG_CUR = 128,

	FLAG_PARENT = 256,
	FLAG_CHILD = 512,
};

struct expr {
	enum op op;
	union {
		enum prop prop;
		struct expr *expr;
		char *string;
		int64_t num;
		regex_t *regex;
	} a, b;
};

struct mailinfo {
	char *fpath;
	struct stat *sb;
	struct message *msg;
	time_t date;
	int depth;
	int index;
	int replies;
	int matched;
	int prune;
	long flags;
	off_t total;
	char subject[100];
};

struct mlist {
	struct mailinfo *m;
	struct mlist *parent;
	struct mlist *next;
};

struct thread {
	int matched;
	struct mlist childs[100];
	struct mlist *cur;
};

static struct thread *thr;

static char *argv0;
static int Tflag;

static int need_thr;

static long kept;
static long num;

static struct expr *expr;
static char *cur;
static char *pos;
static time_t now;
static int prune;

static void
ws()
{
	while (isspace((unsigned char) *pos))
		pos++;
}

static int
token(const char *token)
{
	if (strncmp(pos, token, strlen(token)) == 0) {
		pos += strlen(token);
		ws();
		return 1;
	} else {
		return 0;
	}
}

static void
parse_error(const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	fprintf(stderr, "%s: parse error: ", argv0);
	vfprintf(stderr, msg, ap);
	fprintf(stderr, "\n");
	exit(2);
}

static struct expr *
mkexpr(enum op op)
{
	struct expr *e = malloc(sizeof (struct expr));
	if (!e)
		parse_error("out of memory");
	e->op = op;
	return e;
}

static struct expr *
chain(struct expr *e1, enum op op, struct expr *e2)
{
	struct expr *i, *j, *e;
	if (!e1)
		return e2;
	if (!e2)
		return e1;
	for (j = 0, i = e1; i->op == op; j = i, i = i->b.expr)
		;
	if (!j) {
		e = mkexpr(op);
		e->a.expr = e1;
		e->b.expr = e2;
		return e;
	} else {
		e = mkexpr(op);
		e->a.expr = i;
		e->b.expr = e2;
		j->b.expr = e;
		return e1;
	}
}

static enum op
parse_op()
{
	if (token("<="))
		return EXPR_LE;
	else if (token("<"))
		return EXPR_LT;
	else if (token(">="))
		return EXPR_GE;
	else if (token(">"))
		return EXPR_GT;
	else if (token("==") || token("="))
		return EXPR_EQ;
	else if (token("!="))
		return EXPR_NEQ;

	return 0;
}

static struct expr *parse_cmp();
static struct expr *parse_or();

static struct expr *
parse_inner()
{
	if (token("prune")) {
		struct expr *e = mkexpr(EXPR_PRUNE);
		return e;
	} else if (token("print")) {
		struct expr *e = mkexpr(EXPR_PRINT);
		return e;
	} else if (token("!")) {
		struct expr *e = parse_cmp();
		struct expr *not = mkexpr(EXPR_NOT);
		not->a.expr = e;
		return not;
	} else if (token("(")) {
		struct expr *e = parse_or();
		if (token(")"))
			return e;
		parse_error("missing ) at '%.15s'", pos);
		return 0;
	} else {
		parse_error("unknown expression at '%.15s'", pos);
		return 0;
	}
}

static int
parse_string(char **s)
{
	char *buf = 0;
	size_t bufsiz = 0;
	size_t len = 0;

	if (*pos == '"') {
		pos++;
		while (*pos &&
		    (*pos != '"' || (*pos == '"' && *(pos+1) == '"'))) {
			if (len >= bufsiz) {
				bufsiz = 2*bufsiz + 16;
				buf = realloc(buf, bufsiz);
				if (!buf)
					parse_error("string too long");
			}
			if (*pos == '"')
				pos++;
			buf[len++] = *pos++;
		}
		if (!*pos)
			parse_error("unterminated string");
		if (buf)
			buf[len] = 0;
		pos++;
		ws();
		*s = buf ? buf : (char *) "";
		return 1;
	} else if (*pos == '$') {
		char t;
		char *e = ++pos;

		while (isalnum((unsigned char) *pos) || *pos == '_')
			pos++;
		if (e == pos)
			parse_error("invalid environment variable name");

		t = *pos;
		*pos = 0;
		*s = getenv(e);
		if (!*s)
			*s = (char *) "";
		*pos = t;
		ws();
		return 1;
	}

	return 0;
}

static struct expr *
parse_strcmp()
{
	enum prop prop;
	enum op op;

	if (token("subject"))
		prop = PROP_SUBJECT;
	else if (token("from"))
		prop = PROP_FROM;
	else if (token("to"))
		prop = PROP_TO;
	else
		return parse_inner();

	if (token("~~~"))
		op = EXPR_GLOBI;
	else if (token("~~"))
		op = EXPR_GLOB;
	else if (token("=~~"))
		op = EXPR_REGEXI;
	else if (token("=~"))
		op = EXPR_REGEX;
	else if (token("==="))
		op = EXPR_STREQI;
	else if (token("=="))
		op = EXPR_STREQ;
	else if (token("="))
		op = EXPR_STREQ;
	else
		parse_error("invalid string operator at '%.15s'", pos);

	char *s;
	if (parse_string(&s)) {
		int r = 0;
		struct expr *e = mkexpr(op);
		e->a.prop = prop;
		if (op == EXPR_REGEX) {
			e->b.regex = malloc(sizeof (regex_t));
			r = regcomp(e->b.regex, s, REG_EXTENDED | REG_NOSUB);
		} else if (op == EXPR_REGEXI) {
			e->b.regex = malloc(sizeof (regex_t));
			r = regcomp(e->b.regex, s, REG_EXTENDED | REG_NOSUB | REG_ICASE);
		} else {
			e->b.string = s;
		}

		if (r != 0) {
			char msg[256];
			regerror(r, e->b.regex, msg, sizeof msg);
			parse_error("invalid regex '%s': %s", s, msg);
			exit(2);
		}

		return e;
	}

	parse_error("invalid string at '%.15s'", pos);
	return 0;
}

static int64_t
parse_num(int64_t *r)
{
	char *s = pos;
	if (isdigit((unsigned char) *pos)) {
		int64_t n;

		for (n = 0; isdigit((unsigned char) *pos) && n <= INT64_MAX / 10 - 10; pos++)
			n = 10 * n + (*pos - '0');
		if (isdigit((unsigned char) *pos))
			parse_error("number too big: %s", s);
		if (token("c"))      ;
		else if (token("b")) n *= 512LL;
		else if (token("k")) n *= 1024LL;
		else if (token("M")) n *= 1024LL * 1024;
		else if (token("G")) n *= 1024LL * 1024 * 1024;
		else if (token("T")) n *= 1024LL * 1024 * 1024 * 1024;
		ws();
		*r = n;
		return 1;
	} else {
		return 0;
	}
}

static struct expr *
parse_flag()
{
	enum flags flag;

	if (token("passed")) {
		flag = FLAG_PASSED;
	} else if (token("replied")) {
		flag = FLAG_REPLIED;
	} else if (token("seen")) {
		flag = FLAG_SEEN;
	} else if (token("trashed")) {
		flag = FLAG_TRASHED;
	} else if (token("draft")) {
		flag = FLAG_DRAFT;
	} else if (token("flagged")) {
		flag = FLAG_FLAGGED;
	} else if (token("new")) {
		flag = FLAG_NEW;
	} else if (token("cur")) {
		flag = FLAG_CUR;
	} else if (token("parent")) {
		flag = FLAG_PARENT;
		need_thr = 1;
	} else if (token("child")) {
		flag = FLAG_CHILD;
		need_thr = 1;
	} else
		return parse_strcmp();

	struct expr *e = mkexpr(EXPR_ANYSET);
	e->a.prop = PROP_FLAG;
	e->b.num = flag;

	return e;
}


static struct expr *
parse_cmp()
{
	enum prop prop;
	enum op op;

	op = 0;

	if (token("depth"))
		prop = PROP_DEPTH;
	else if (token("index"))
		prop = PROP_INDEX;
	else if (token("replies")) {
		prop = PROP_REPLIES;
		need_thr = 1;
	} else if (token("size"))
		prop = PROP_SIZE;
	else if (token("total"))
		prop = PROP_TOTAL;
	else
		return parse_flag();

	if (!(op = parse_op()))
		parse_error("invalid comparison at '%.15s'", pos);

	int64_t n;
	if (parse_num(&n)) {
		struct expr *e = mkexpr(op);
		e->a.prop = prop;
		e->b.num = n;
		return e;
	}

	return 0;
}

static int
parse_dur(int64_t *n)
{
	char *s, *r;
	if (!parse_string(&s))
		return 0;

	if (*s == '/' || *s == '.') {
		struct stat st;
		if (stat(s, &st) < 0)
			parse_error("can't stat file '%s': %s",
			    s, strerror(errno));
		*n = st.st_mtime;
		return 1;
	}

	struct tm tm = { 0 };
	r = strptime(s, "%Y-%m-%d %H:%M:%S", &tm);
	if (r && !*r) {
		*n = mktime(&tm);
		return 1;
	}
	r = strptime(s, "%Y-%m-%d", &tm);
	if (r && !*r) {
		tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
		*n = mktime(&tm);
		return 1;
	}
	r = strptime(s, "%H:%M:%S", &tm);
	if (r && !*r) {
		struct tm *tmnow = localtime(&now);
		tm.tm_year = tmnow->tm_year;
		tm.tm_mon = tmnow->tm_mon;
		tm.tm_mday = tmnow->tm_mday;
		*n = mktime(&tm);
		return 1;
	}
	r = strptime(s, "%H:%M", &tm);
	if (r && !*r) {
		struct tm *tmnow = localtime(&now);
		tm.tm_year = tmnow->tm_year;
		tm.tm_mon = tmnow->tm_mon;
		tm.tm_mday = tmnow->tm_mday;
		tm.tm_sec = 0;
		*n = mktime(&tm);
		return 1;
	}

	if (*s == '-') {
		s++;

		errno = 0;
		int64_t d;
		d = strtol(s, &r, 10);
		if (errno == 0 && r[0] == 'd' && !r[1]) {
			struct tm *tmnow = localtime(&now);
			tmnow->tm_mday -= d;
			tmnow->tm_hour = tmnow->tm_min = tmnow->tm_sec = 0;
			*n = mktime(tmnow);
			return 1;
		}
		if (errno == 0 && r[0] == 'h' && !r[1]) {
			*n = now - (d*60*60);
			return 1;
		}
		if (errno == 0 && r[0] == 'm' && !r[1]) {
			*n = now - (d*60);
			return 1;
		}
		if (errno == 0 && r[0] == 's' && !r[1]) {
			*n = now - d;
			return 1;
		}
		parse_error("invalid relative time format '%s'", s-1);
	}

	parse_error("invalid time format '%s'", s);
	return 0;
}

static struct expr *
parse_timecmp()
{
	enum prop prop;
	enum op op;

	if (token("atime"))
		prop = PROP_ATIME;
	else if (token("ctime"))
		prop = PROP_CTIME;
	else if (token("mtime"))
		prop = PROP_MTIME;
	else if (token("date"))
		prop = PROP_DATE;
	else
		return parse_cmp();

	op = parse_op();
	if (!op)
		parse_error("invalid comparison at '%.15s'", pos);

	int64_t n;
	if (parse_num(&n) || parse_dur(&n)) {
		struct expr *e = mkexpr(op);
		e->a.prop = prop;
		e->b.num = n;
		return e;
	}

	return 0;
}

static struct expr *
parse_and()
{
	struct expr *e1 = parse_timecmp();
	struct expr *r = e1;

	while (token("&&")) {
		struct expr *e2 = parse_timecmp();
		r = chain(r, EXPR_AND, e2);
	}

	return r;
}

static struct expr *
parse_or()
{
	struct expr *e1 = parse_and();
	struct expr *r = e1;

	while (token("||")) {
		struct expr *e2 = parse_and();
		r = chain(r, EXPR_OR, e2);
	}

	return r;
}

static struct expr *
parse_expr(const char *s)
{
	pos = (char *)s;
	struct expr *e = parse_or();
	if (*pos)
		parse_error("trailing garbage at '%.15s'", pos);
	return e;
}

static struct expr *
parse_msglist(const char *s)
{
	int64_t n, m;
	int r;
	struct expr *e1, *e2;
	char *d;

	switch (*s) {
	case '/':
		s++;
		e1 = mkexpr(EXPR_REGEXI);
		e1->a.prop = PROP_SUBJECT;
		e1->b.regex = malloc(sizeof (regex_t));
		r = regcomp(e1->b.regex, s, REG_EXTENDED | REG_NOSUB | REG_ICASE);
		if (r != 0) {
			char msg[256];
			regerror(r, e1->b.regex, msg, sizeof msg);
			parse_error("invalid regex '%s': %s", s, msg);
		}
		return e1;
	case ':':
		if (strlen(s) <= 1)
			parse_error("missing type at '%.15s'", s);

		enum flags flag;
		n = 0;

		switch (*++s) {
		case 'P': flag = FLAG_PASSED; break;
		case 'F': flag = FLAG_FLAGGED; break;
		case 'D': flag = FLAG_DRAFT; break;
		case 'd': /* FALL TROUGH */
		case 'T': flag = FLAG_TRASHED; break;
		case 'u': n = 1; /* FALL TROUGH */
		case 'r': /* FALL TROUGH */
		case 'S': flag = FLAG_SEEN; break;
		case 'o': n = 1; /* FALL TROUGH */
		case 'n': flag = FLAG_NEW; break;
		default: parse_error("unknown type at '%.15s'", s);
		}

		e1 = mkexpr(EXPR_ANYSET);
		e1->a.prop = PROP_FLAG;
		e1->b.num = flag;

		if (!n)
			return e1;

		e2 = mkexpr(EXPR_NOT);
		e2->a.expr = e1;
		return e2;
	default:
		pos = (char *)s;

		if ((d = strchr(s, '-')) && parse_num(&n) &&
		    (pos = (char *)d + 1) && parse_num(&m)) {
			/* index >= n */
			e1 = mkexpr(EXPR_GE);
			e1->a.prop = PROP_INDEX;
			e1->b.num = n;

			/* index <= m */
			e2 = mkexpr(EXPR_LE);
			e2->a.prop = PROP_INDEX;
			e2->b.num = m;

			/* e1 && e2 */
			return chain(e1, EXPR_AND, e2);
		} else if (parse_num(&n)) {
			e1 = mkexpr(EXPR_EQ);
			e1->a.prop = PROP_INDEX;
			e1->b.num = n;

			return e1;
		} else {
			expr = chain(parse_expr("from.addr == 's'"), EXPR_AND, expr);
		}
	}
	return NULL;
}

time_t
msg_date(struct mailinfo *m)
{
	if (m->date)
		return m->date;

	char *b;
	if (m->msg && (b = blaze822_hdr(m->msg, "date")))
		return (m->date = blaze822_date(b));

	return -1;
}

char *
msg_subject(struct mailinfo *m)
{
	if (*m->subject != '\0')
		return m->subject;

	char *b;
	if (m->msg == 0 || (b = blaze822_hdr(m->msg, "subject")) == 0)
		return "";

	blaze822_decode_rfc2047(m->subject, b, sizeof m->subject - 1, "UTF-8");
	return m->subject;
}

int
eval(struct expr *e, struct mailinfo *m)
{
	switch (e->op) {
	case EXPR_OR:
		return eval(e->a.expr, m) || eval(e->b.expr, m);
	case EXPR_AND:
		return eval(e->a.expr, m) && eval(e->b.expr, m);
	case EXPR_NOT:
		return !eval(e->a.expr, m);
		return 1;
	case EXPR_PRUNE:
		prune = 1;
		return 1;
	case EXPR_PRINT:
		return 1;
	case EXPR_LT:
	case EXPR_LE:
	case EXPR_EQ:
	case EXPR_NEQ:
	case EXPR_GE:
	case EXPR_GT:
	case EXPR_ALLSET:
	case EXPR_ANYSET: {
		long v = 0;

		if (m->sb == '\0' && (
		    e->a.prop == PROP_ATIME ||
		    e->a.prop == PROP_CTIME ||
		    e->a.prop == PROP_MTIME ||
		    e->a.prop == PROP_SIZE) &&
		    (m->sb = calloc(1, sizeof *m->sb)) != NULL &&
		    stat(m->fpath, m->sb) != 0) {
			fprintf(stderr, "stat");
			exit(2);
		}

		switch (e->a.prop) {
		case PROP_ATIME: v = m->sb->st_atime; break;
		case PROP_CTIME: v = m->sb->st_ctime; break;
		case PROP_MTIME: v = m->sb->st_mtime; break;
		case PROP_REPLIES: v = m->replies; break;
		case PROP_SIZE: v = m->sb->st_size; break;
		case PROP_DATE: v = msg_date(m);
		case PROP_FLAG: v = m->flags; break;
		case PROP_INDEX: v = m->index; break;
		case PROP_DEPTH: v = m->depth; break;
		default:
			parse_error("unknown property");
		}

		switch (e->op) {
		case EXPR_LT: return v < e->b.num;
		case EXPR_LE: return v <= e->b.num;
		case EXPR_EQ: return v == e->b.num;
		case EXPR_NEQ: return v != e->b.num;
		case EXPR_GE: return v >= e->b.num;
		case EXPR_GT: return v > e->b.num;
		case EXPR_ALLSET: return (v & e->b.num) == e->b.num;
		case EXPR_ANYSET: return (v & e->b.num) > 0;
		}
	}
	case EXPR_STREQ:
	case EXPR_STREQI:
	case EXPR_GLOB:
	case EXPR_GLOBI:
	case EXPR_REGEX:
	case EXPR_REGEXI: {
		const char *s = "";
		switch(e->a.prop) {
		case PROP_PATH: s = m->fpath; break;
		case PROP_SUBJECT: s = msg_subject(m); break;
		}
		switch (e->op) {
		case EXPR_STREQ: return strcmp(e->b.string, s) == 0;
		case EXPR_STREQI: return strcasecmp(e->b.string, s) == 0;
		case EXPR_GLOB: return fnmatch(e->b.string, s, 0) == 0;
		case EXPR_GLOBI: return fnmatch(e->b.string, s, FNM_CASEFOLD) == 0;
		case EXPR_REGEX:
		case EXPR_REGEXI: return regexec(e->b.regex, s, 0, 0, 0) == 0;
		}
	}
	}
	return 0;
}

struct mailinfo *
mailfile(char *file)
{
	static int init;
	if (!init) {
		// delay loading of the seqmap until we need to scan the first
		// file, in case someone in the pipe updated the map before
		char *seqmap = blaze822_seq_open(0);
		blaze822_seq_load(seqmap);
		cur = blaze822_seq_cur();
		init = 1;
	}

	struct mailinfo *m;
	m = calloc(1, sizeof *m);
	if (!m) {
		fprintf(stderr, "calloc");
		exit(2);
	}
	memset(m->subject, 0, sizeof m->subject);

	m->fpath = file;
	m->index = num++;
	m->flags = 0;
	m->replies = 0;
	m->depth = 0;
	m->sb = 0;
	m->msg = 0;

	while (*m->fpath == ' ' || *m->fpath== '\t') {
		m->depth++;
		m->fpath++;
	}

	char *e = m->fpath + strlen(m->fpath) - 1;
	while (m->fpath < e && (*e == ' ' || *e == '\t'))
		*e-- = 0;

	m->msg = blaze822(m->fpath);
	if (!m->msg)
		return m;

	if ((e = strrchr(m->fpath, '/') - 1) && (e - m->fpath) >= 2 &&
	    *e-- == 'w' && *e-- == 'e' && *e-- == 'n')
		m->flags |= FLAG_NEW;

	if (cur && strcmp(cur, m->fpath) == 0)
		m->flags |= FLAG_CUR;

	char *f = strstr(m->fpath, ":2,");
	if (f) {
		if (strchr(f, 'P'))
			m->flags |= FLAG_PASSED;
		if (strchr(f, 'R'))
			m->flags |= FLAG_REPLIED;
		if (strchr(f, 'S'))
			m->flags |= FLAG_SEEN;
		if (strchr(f, 'T'))
			m->flags |= FLAG_TRASHED;
		if (strchr(f, 'D'))
			m->flags |= FLAG_DRAFT;
		if (strchr(f, 'F'))
			m->flags |= FLAG_FLAGGED;
	}

	return m;
}

void
do_thr()
{
	struct mlist *ml;

	if (!thr)
		return;

	for (ml = thr->childs; ml; ml = ml->next) {
		if (!ml->m)
			continue;
		if ((ml->m->prune = prune) || (Tflag && thr->matched))
			continue;
		if (expr && eval(expr, ml->m)) {
			ml->m->matched = 1;
			thr->matched++;
		}
	}
	prune = 0;

	for (ml = thr->childs; ml; ml = ml->next) {
		if (!ml->m)
			break;

		if (((Tflag && thr->matched) || ml->m->matched) && !ml->m->prune) {
			int i;
			for (i = 0; i < ml->m->depth; i++)
				putchar(' ');

			fputs(ml->m->fpath, stdout);
			putchar('\n');

			kept++;
		}

		/* free collected mails */
		if (ml->m->msg)
			blaze822_free(ml->m->msg);

		if (ml->m->sb)
			free(ml->m->sb);

		free(ml->m->fpath);
		free(ml->m);
	}

	free(thr);
	thr = 0;
}

void
collect(char *file)
{
	struct mailinfo *m;
	struct mlist *ml;

	if ((m = mailfile(file)) == 0)
		return;

	if (m->depth == 0) {
		if (thr)
			do_thr();

		/* new thread */
		thr = calloc(1, sizeof *thr);
		if (!thr) {
			fprintf(stderr, "calloc");
			exit(2);
		}

		thr->matched = 0;
		ml = thr->cur = thr->childs;
		thr->cur->m = m;
	} else {
		ml = thr->cur + 1;

		if (thr->cur->m->depth < m->depth) {
			/* previous mail is a prent */
			thr->cur->m->flags |= FLAG_PARENT;
			ml->parent = thr->cur;
		} else if (thr->cur->m->depth == m->depth) {
			/* same depth == same parent */
			ml->parent = thr->cur->parent;
		} else if (thr->cur->m->depth > m->depth) {
			/* find parent mail */
			struct mlist *pl;
			for (pl = thr->cur; pl->m->depth >= m->depth; pl--);
			ml->parent = pl;
		}

		m->flags |= FLAG_CHILD;

		thr->cur->next = ml;
		thr->cur = ml;
		ml->m = m;
	}

	for (ml = ml->parent; ml; ml = ml->parent)
		ml->m->replies++;

	m->fpath = strdup(m->fpath);
}

void
oneline(char *file)
{
	struct mailinfo *m;

	m = mailfile(file);
	if (expr && !eval(expr, m))
		goto out;

	fputs(file, stdout);
	putchar('\n');
	kept++;

out:
	if (m->msg)
		blaze822_free(m->msg);
	if (m->sb)
		free(m->sb);
	free(m);
}

int
main(int argc, char *argv[])
{
	long i;
	int c;

	argv0 = argv[0];

	while ((c = getopt(argc, argv, "Tt:")) != -1)
		switch (c) {
		case 'T': Tflag = need_thr = 1; break;
		case 't': expr = chain(expr, EXPR_AND, parse_expr(optarg)); break;
		}

	if (optind != argc)
		for (c = optind; c < argc; c++)
			expr = chain(expr, EXPR_AND, parse_msglist(argv[c]));

	if (isatty(0)) {
		i = blaze822_loop1(":", need_thr ? collect : oneline);
	} else
		i = blaze822_loop(0, NULL, need_thr ? collect : oneline);

	/* print and free last thread */
	if (Tflag && thr)
		do_thr();

	fprintf(stderr, "%ld mails tested, %ld picked.\n", i, kept);
	return 0;
}
