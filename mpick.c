// FNM_CASEFOLD
#define _GNU_SOURCE
#include <fnmatch.h>

// strptime
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#ifdef __has_include
  #if __has_include(<stdnoreturn.h>)
    #include <stdnoreturn.h>
  #else
    #define noreturn /**/
  #endif
#else
  #define noreturn /**/
#endif

/* For Solaris. */
#if !defined(FNM_CASEFOLD) && defined(FNM_IGNORECASE)
#define FNM_CASEFOLD FNM_IGNORECASE
#endif

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#include "blaze822.h"
#include "xpledge.h"

enum op {
	EXPR_OR = 1,
	EXPR_AND,
	EXPR_COND,
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
	EXPR_REDIR_FILE,
	EXPR_REDIR_PIPE,
	EXPR_TYPE,
	EXPR_ALLSET,
	EXPR_ANYSET,
	EXPR_BINDING,
};

enum prop {
	PROP_ATIME = 1,
	PROP_CTIME,
	PROP_DEPTH,
	PROP_KEPT,
	PROP_MTIME,
	PROP_PATH,
	PROP_REPLIES,
	PROP_SIZE,
	PROP_TOTAL,
	PROP_INDEX,
	PROP_DATE,
	PROP_FLAG,
	PROP_HDR,
	PROP_HDR_ADDR,
	PROP_HDR_DISP,
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

	FLAG_INFO = 1024,
};

enum var {
	VAR_CUR = 1,
};

struct expr {
	enum op op;
	union {
		enum prop prop;
		struct expr *expr;
		char *string;
		int64_t num;
		regex_t *regex;
		enum var var;
		struct binding *binding;
	} a, b, c;
	int extra;
};

struct mailinfo {
	char *file;
	char *fpath;
	struct stat *sb;
	struct message *msg;
	time_t date;
	int depth;
	int index;
	int replies;
	int matched;
	int prune;
	int flags;
	off_t total;
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

struct file {
	enum op op;
	const char *name;
	const char *mode;
	FILE *fp;
	struct file *next;
};

struct pos {
	char *pos;
	char *line;
	size_t linenr;
};

struct binding {
	char *name;
	size_t nlen;
	size_t refcount;
	struct expr *expr;
	struct binding *next;
};

struct scope {
	struct binding *bindings;
	struct scope *prev;
};

struct scope *scopeq = NULL;

static struct thread *thr;

static char *argv0;
static int Tflag;

static int need_thr;

static long kept;
static long num;

static struct expr *expr;
static long cur_idx;
static char *cur;
static time_t now;
static int prune;

static char *pos;
static const char *fname;
static char *line = NULL;
static int linenr = 0;

static struct file *files, *fileq = NULL;

static void *
xcalloc(size_t nmemb, size_t size)
{
	void *r;
	if ((r = calloc(nmemb, size)))
		return r;
	perror("calloc");
	exit(2);
}

static char *
xstrdup(const char *s)
{
	char *r;
	if ((r = strdup(s)))
		return r;
	perror("strdup");
	exit(2);
}

static void
ws()
{
	for (; *pos;) {
		while (isspace((unsigned char)*pos)) {
			if (*pos == '\n') {
				line = pos+1;
				linenr++;
			}
			pos++;
		}
		if (*pos != '#')
			break;

		pos += strcspn(pos, "\n\0");
		if (*pos != '\n')
			break;
	}
}

static int
token(char *token)
{
	if (strncmp(pos, token, strlen(token)) == 0) {
		pos += strlen(token);
		ws();
		return 1;
	} else {
		return 0;
	}
}

static int
peek(char *token)
{
	return strncmp(pos, token, strlen(token)) == 0;
}

noreturn static void
parse_error(char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	fprintf(stderr, "%s: parse error: %s:%d:%ld: ", argv0, fname, linenr, pos-line+1);
	vfprintf(stderr, msg, ap);
	fprintf(stderr, "\n");
	exit(2);
}

noreturn static void
parse_error_at(struct pos *savepos, char *msg, ...)
{
	char *e;

	if (savepos) {
		pos = savepos->pos;
		line = savepos->line;
		linenr = savepos->linenr;
	}

	va_list ap;
	va_start(ap, msg);
	fprintf(stderr, "%s: parse error: %s:%d:%ld: ", argv0, fname, linenr, pos-line+1);
	vfprintf(stderr, msg, ap);
	fprintf(stderr, " at '");
	for (e = pos+15; *pos && *pos != '\n' && pos <= e; pos++)
		putc(*pos, stderr);
	fprintf(stderr, "'\n");
	exit(2);
}

static struct expr *
mkexpr(enum op op)
{
	struct expr *e = malloc(sizeof (struct expr));
	if (!e)
		parse_error("out of memory");
	e->op = op;
	e->extra = 0;
	return e;
}

static void
freeexpr(struct expr *e)
{
	if (!e) return;
	switch (e->op) {
	case EXPR_OR:
	case EXPR_AND:
		freeexpr(e->a.expr);
		freeexpr(e->b.expr);
		break;
	case EXPR_COND:
		freeexpr(e->a.expr);
		freeexpr(e->b.expr);
		freeexpr(e->c.expr);
		break;
	case EXPR_NOT:
		freeexpr(e->a.expr);
		break;
	case EXPR_BINDING:
		e->a.binding->refcount -= 1;
		if (e->a.binding->refcount == 0) {
			freeexpr(e->a.binding->expr);
			free(e->a.binding);
		}
		break;
	case EXPR_REDIR_FILE:
	case EXPR_REDIR_PIPE:
		free(e->a.string);
		free(e->b.string);
		break;
	case EXPR_STREQ:
	case EXPR_STREQI:
	case EXPR_GLOB:
	case EXPR_GLOBI:
	case EXPR_REGEX:
	case EXPR_REGEXI:
		switch (e->a.prop) {
		case PROP_PATH: break;
		case PROP_HDR:
		case PROP_HDR_ADDR:
		case PROP_HDR_DISP:
			free(e->b.string);
		}
		if (e->op == EXPR_REGEX || e->op == EXPR_REGEXI)
			regfree(e->c.regex);
		else
			free(e->c.string);
		break;
	}
	free(e);
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

static int
parse_ident(char **sp, size_t *lp)
{
	char *p = pos;
	if (!isalpha(*pos) && *pos != '_')
		return 0;
	p++;
	while (*p && (isalnum(*p) || *p == '_'))
		p++;
	*sp = pos;
	*lp = p-pos;
	pos = p;
	ws();
	return 1;
}

static struct expr *
parse_binding()
{
	struct scope *sc;
	struct binding *b;
	char *s;
	size_t l = 0;
	struct pos savepos = { pos, line, linenr };

	if (parse_ident(&s, &l)) {
		for (sc = scopeq; sc; sc = sc->prev) {
			for (b = sc->bindings; b; b = b->next) {
				if (b->nlen == l && strncmp(b->name, s, l) == 0) {
					struct expr *e = mkexpr(EXPR_BINDING);
					e->a.binding = b;
					b->refcount++;
					return e;
				}
			}
		}
	}

	parse_error_at(&savepos, "unknown expression");
	return 0;
}

static struct expr *parse_cond();

static struct expr *
parse_let()
{
	if (!token("let"))
		return parse_binding();

	struct scope *sc;
	char *s;
	size_t l;

	sc = xcalloc(1, sizeof (struct scope));
	sc->prev = scopeq;
	scopeq = sc;

	struct binding *b, *bq;
	struct expr *e;
	bq = NULL;
	for (;;) {
		if (!parse_ident(&s, &l))
			parse_error_at(NULL, "missing ident");
		if (!token("="))
			parse_error_at(NULL, "missing =");
		e = parse_cond();
		b = xcalloc(1, sizeof (struct binding));
		b->name = s;
		b->nlen = l;
		b->expr = e;
		if (bq) {
			bq->next = b;
			bq = b;
		} else {
			bq = sc->bindings = b;
		}
		if (!token("let"))
			break;
	}
	if (!token("in"))
		parse_error_at(NULL, "missing `in`");

	e = parse_cond();

	struct binding *bs;
	for (b = sc->bindings; b; b = bs) {
		bs = b->next;
		if (b->refcount != 0)
			continue;
		freeexpr(b->expr);
		free(b);
	}

	scopeq = sc->prev;
	free(sc);

	return e;
}

static struct expr *parse_cmp();

static struct expr *
parse_inner()
{
	if (token("prune")) {
		return mkexpr(EXPR_PRUNE);
	} else if (token("print")) {
		return mkexpr(EXPR_PRINT);
	} else if (token("skip")) {
		struct expr *e = mkexpr(EXPR_PRINT);
		struct expr *not = mkexpr(EXPR_NOT);
		not->a.expr = e;
		return not;
	} else if (token("!")) {
		struct expr *e = parse_cmp();
		struct expr *not = mkexpr(EXPR_NOT);
		not->a.expr = e;
		return not;
	}
	if (peek("(")) {
		struct pos savepos = { pos, line, linenr };
		(void) token("(");
		struct expr *e = parse_cond();
		if (token(")"))
			return e;
		parse_error_at(&savepos, "unterminated (");
		return 0;
	}
	return parse_let();
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
			if (len+1 >= bufsiz) {
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
		*s = buf ? buf : xstrdup("");
		return 1;
	} else if (*pos == '$') {
		char t;
		char *e = ++pos;

		while (isalnum((unsigned char)*pos) || *pos == '_')
			pos++;
		if (e == pos)
			parse_error("invalid environment variable name");

		t = *pos;
		*pos = 0;
		*s = getenv(e);
		if (!*s)
			*s = "";
		*s = xstrdup(*s);
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
	int negate;
	char *h;

	h = 0;
	prop = 0;
	negate = 0;

	if (token("from"))
		h = xstrdup("from");
	else if (token("to"))
		h = xstrdup("to");
	else if (token("subject"))
		h = xstrdup("subject");
	else if (token("path"))
		prop = PROP_PATH;
	else if (!parse_string(&h))
		return parse_inner();

	if (!prop) {
		if (token(".")) {
			if (token("addr"))      prop = PROP_HDR_ADDR;
			else if (token("disp")) prop = PROP_HDR_DISP;
			else parse_error_at(NULL, "unknown decode parameter");
		} else
			prop = PROP_HDR;
	}

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
	else if (token("!~~~"))
		negate = 1, op = EXPR_GLOBI;
	else if (token("!~~"))
		negate = 1, op = EXPR_GLOB;
	else if (token("!=~~"))
		negate = 1, op = EXPR_REGEXI;
	else if (token("!=~"))
		negate = 1, op = EXPR_REGEX;
	else if (token("!==="))
		negate = 1, op = EXPR_STREQI;
	else if (token("!==") || token("!="))
		negate = 1, op = EXPR_STREQ;
	else
		parse_error_at(NULL, "invalid string operator");

	char *s;
	if (!parse_string(&s)) {
		parse_error_at(NULL, "invalid string");
		return 0;
	}

	int r = 0;
	struct expr *e = mkexpr(op);
	e->a.prop = prop;
	switch (prop) {
	case PROP_HDR:
	case PROP_HDR_ADDR:
	case PROP_HDR_DISP:
		e->b.string = h;
		break;
	case PROP_PATH: break;
	}

	if (op == EXPR_REGEX || op == EXPR_REGEXI) {
		e->c.regex = malloc(sizeof (regex_t));
		r = regcomp(e->c.regex, s, REG_EXTENDED | REG_NOSUB |
		    (op == EXPR_REGEXI ? REG_ICASE : 0));
		if (r != 0) {
			char msg[256];
			regerror(r, e->c.regex, msg, sizeof msg);
			parse_error("invalid regex '%s': %s", s, msg);
			exit(2);
		}
		free(s);
	} else {
		e->c.string = s;
	}

	if (negate) {
		struct expr *not = mkexpr(EXPR_NOT);
		not->a.expr = e;
		return not;
	}

	return e;
}

static int64_t
parse_num(int64_t *r)
{
	char *s = pos;
	if (isdigit((unsigned char)*pos)) {
		int64_t n;

		for (n = 0; isdigit((unsigned char)*pos) && n <= INT64_MAX / 10 - 10; pos++)
			n = 10 * n + (*pos - '0');
		if (isdigit((unsigned char)*pos))
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
	} else if (token("info")) {
		flag = FLAG_INFO;
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

	if (token("depth"))
		prop = PROP_DEPTH;
	else if (token("kept"))
		prop = PROP_KEPT;
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
		parse_error_at(NULL, "invalid comparison");

	int64_t n;
	if (parse_num(&n)) {
		struct expr *e = mkexpr(op);
		e->a.prop = prop;
		e->b.num = n;
		return e;
	} else if (token("cur")) {
		struct expr *e = mkexpr(op);
		e->a.prop = prop;
		e->b.var = VAR_CUR;
		e->extra = 1;
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
		goto ret;
	}

	struct tm tm = { 0 };
	r = strptime(s, "%Y-%m-%d %H:%M:%S", &tm);
	if (r && !*r) {
		*n = mktime(&tm);
		goto ret;
	}
	r = strptime(s, "%Y-%m-%d", &tm);
	if (r && !*r) {
		tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
		*n = mktime(&tm);
		goto ret;
	}
	r = strptime(s, "%H:%M:%S", &tm);
	if (r && !*r) {
		struct tm *tmnow = localtime(&now);
		tm.tm_year = tmnow->tm_year;
		tm.tm_mon = tmnow->tm_mon;
		tm.tm_mday = tmnow->tm_mday;
		*n = mktime(&tm);
		goto ret;
	}
	r = strptime(s, "%H:%M", &tm);
	if (r && !*r) {
		struct tm *tmnow = localtime(&now);
		tm.tm_year = tmnow->tm_year;
		tm.tm_mon = tmnow->tm_mon;
		tm.tm_mday = tmnow->tm_mday;
		tm.tm_sec = 0;
		*n = mktime(&tm);
		goto ret;
	}

	if (*s == '-') {
		int64_t d;
		errno = 0;
		d = strtol(s+1, &r, 10);
		if (errno == 0 && r[0] == 'd' && !r[1]) {
			struct tm *tmnow = localtime(&now);
			tmnow->tm_mday -= d;
			tmnow->tm_hour = tmnow->tm_min = tmnow->tm_sec = 0;
			*n = mktime(tmnow);
			goto ret;
		}
		if (errno == 0 && r[0] == 'h' && !r[1]) {
			*n = now - (d*60*60);
			goto ret;
		}
		if (errno == 0 && r[0] == 'm' && !r[1]) {
			*n = now - (d*60);
			goto ret;
		}
		if (errno == 0 && r[0] == 's' && !r[1]) {
			*n = now - d;
			goto ret;
		}
		parse_error("invalid relative time format '%s'", s);
	}

	parse_error("invalid time format '%s'", s);
	return 0;
ret:
	free(s);
	return 1;
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
		parse_error_at(NULL, "invalid comparison");

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
parse_redir(struct expr *e)
{
	char *s;
	const char *m;

	if (peek("||"))
		return e;

	if (token("|")) {
		if (!parse_string(&s))
			parse_error_at(NULL, "expected command");
		struct expr *r = mkexpr(EXPR_REDIR_PIPE);
		r->a.string = s;
		r->b.string = xstrdup("w");
		return chain(e, EXPR_AND, r);
	}
	else if (token(">>")) m = "a+";
	else if (token(">")) m = "w+";
	else return e;

	if (!parse_string(&s))
		parse_error_at(NULL, "expected file name");
	struct expr *r = mkexpr(EXPR_REDIR_FILE);
	r->a.string = s;
	r->b.string = xstrdup(m);
	return chain(e, EXPR_AND, r);
}

static struct expr *
parse_and()
{
	struct expr *e1 = parse_redir(parse_timecmp());
	struct expr *r = e1;

	while (token("&&")) {
		struct expr *e2 = parse_redir(parse_timecmp());
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
parse_cond()
{
	struct expr *e1 = parse_or();

	if (token("?")) {
		struct expr *e2 = parse_or();
		if (token(":")) {
			struct expr *e3 = parse_cond();
			struct expr *r = mkexpr(EXPR_COND);
			r->a.expr = e1;
			r->b.expr = e2;
			r->c.expr = e3;

			return r;
		} else {
			parse_error_at(NULL, "expected :", pos);
		}
	}

	return e1;
}

static struct expr *
parse_expr()
{
	ws();
	return parse_cond();
}

static struct expr *
parse_buf(const char *f, char *s)
{
	struct expr *e;
	fname = f;
	line = pos = s;
	linenr = 1;
	e = parse_expr();
	if (*pos)
		parse_error_at(NULL, "trailing garbage");
	return e;
}

time_t
msg_date(struct mailinfo *m)
{
	if (m->date)
		return m->date;

	// XXX: date comparisation should handle zero dates
	if (!m->msg && m->fpath) {
		if (!(m->msg = blaze822(m->fpath))) {
			m->fpath = NULL;
			return -1;
		}
	}

	char *b;
	if (m->msg && (b = blaze822_hdr(m->msg, "date")))
		return (m->date = blaze822_date(b));

	return -1;
}

char *
msg_hdr(char **s, const char *h, struct mailinfo *m)
{
	static char hdrbuf[4096];

	if (!m->msg && m->fpath) {
		if (!(m->msg = blaze822(m->fpath))) {
			m->fpath = NULL;
			return NULL;
		}
	}

	// XXX: only return one header for now
	if (*s)
		return NULL;

	char *b;
	if (!m->msg || !(b = blaze822_chdr(m->msg, h)))
		return NULL;
	*s = b;

	blaze822_decode_rfc2047(hdrbuf, b, sizeof hdrbuf - 1, "UTF-8");
	return hdrbuf;
}

char *
msg_hdr_addr(char **s, const char *h, struct mailinfo *m, int rdisp)
{
	if (!m->msg && m->fpath) {
		if (!(m->msg = blaze822(m->fpath))) {
			m->fpath = NULL;
			return NULL;
		}
	}

	char *b = *s;
	if (!b) {
		if (!m->msg || !(b = blaze822_chdr(m->msg, h)))
			return NULL;
	}

	char *disp, *addr;
	*s = blaze822_addr(b, &disp, &addr);

	if (rdisp)
		return disp;
	return addr;
}

char *
msg_hdr_address(char **s, const char *h, struct mailinfo *m)
{
	return msg_hdr_addr(s, h, m, 0);
}

char *
msg_hdr_display(char **s, const char *h, struct mailinfo *m)
{
	return msg_hdr_addr(s, h, m, 1);
}

FILE *
redir(struct expr *e)
{
	struct file *file;
	FILE *fp;

	for (file = files; file; file = file->next) {
		if (e->op == file->op &&
		    strcmp(e->a.string, file->name) == 0 &&
			strcmp(e->b.string, file->mode) == 0)
			return file->fp;
	}

	fflush(stdout);
	fp = NULL;
	switch (e->op) {
	case EXPR_REDIR_FILE: fp = fopen(e->a.string, e->b.string); break;
	case EXPR_REDIR_PIPE: fp = popen(e->a.string, e->b.string); break;
	}
	if (!fp) {
		fprintf(stderr, "%s: %s: %s\n", argv0, e->a.string, strerror(errno));
		exit(3);
	}
	file = xcalloc(1, sizeof (struct file));
	file->op = e->op;
	file->name = e->a.string;
	file->mode = e->b.string;
	file->fp = fp;
	if (!files) files = file;
	if (fileq) fileq->next = file;
	fileq = file;
	return fp;
}

int
eval(struct expr *e, struct mailinfo *m)
{
	FILE *fp;
	switch (e->op) {
	case EXPR_OR:
		return eval(e->a.expr, m) || eval(e->b.expr, m);
	case EXPR_AND:
		return eval(e->a.expr, m) && eval(e->b.expr, m);
	case EXPR_COND:
		return eval(e->a.expr, m)
		    ? eval(e->b.expr, m)
			: eval(e->c.expr, m);
	case EXPR_NOT:
		return !eval(e->a.expr, m);
	case EXPR_BINDING:
		return eval(e->a.binding->expr, m);
	case EXPR_PRUNE:
		prune = 1;
		return 1;
	case EXPR_PRINT:
		return 1;
	case EXPR_REDIR_FILE:
	case EXPR_REDIR_PIPE:
		fp = redir(e);
		fputs(m->file, fp);
		putc('\n', fp);
		fflush(fp);
		return 1;
	case EXPR_LT:
	case EXPR_LE:
	case EXPR_EQ:
	case EXPR_NEQ:
	case EXPR_GE:
	case EXPR_GT:
	case EXPR_ALLSET:
	case EXPR_ANYSET: {
		long v = 0, n;

		if (!m->sb && m->fpath && (
		    e->a.prop == PROP_ATIME ||
		    e->a.prop == PROP_CTIME ||
		    e->a.prop == PROP_MTIME ||
		    e->a.prop == PROP_SIZE)) {
			m->sb = xcalloc(1, sizeof *m->sb);
			if (stat(m->fpath, m->sb) == -1)
				m->fpath = NULL;
			// XXX: stat based expressions should handle 0
		}

		n = e->b.num;

		if (e->extra)
			switch (e->b.var) {
			case VAR_CUR:
				if (!cur_idx)
					n = (e->op == EXPR_LT || e->op == EXPR_LE) ? LONG_MAX : -1;
				else
					n = cur_idx;
				break;
			}

		switch (e->a.prop) {
		case PROP_ATIME: if (m->sb) v = m->sb->st_atime; break;
		case PROP_CTIME: if (m->sb) v = m->sb->st_ctime; break;
		case PROP_MTIME: if (m->sb) v = m->sb->st_mtime; break;
		case PROP_KEPT: v = kept; break;
		case PROP_REPLIES: v = m->replies; break;
		case PROP_SIZE: if (m->sb) v = m->sb->st_size; break;
		case PROP_DATE: v = msg_date(m); break;
		case PROP_FLAG: v = m->flags; break;
		case PROP_INDEX: v = m->index; break;
		case PROP_DEPTH: v = m->depth; break;
		default: parse_error("unknown property");
		}

		switch (e->op) {
		case EXPR_LT: return v < n;
		case EXPR_LE: return v <= n;
		case EXPR_EQ: return v == n;
		case EXPR_NEQ: return v != n;
		case EXPR_GE: return v >= n;
		case EXPR_GT: return v > n;
		case EXPR_ALLSET: return (v & n) == n;
		case EXPR_ANYSET: return (v & n) > 0;
		default: parse_error("invalid operator");
		}
	}
	case EXPR_STREQ:
	case EXPR_STREQI:
	case EXPR_GLOB:
	case EXPR_GLOBI:
	case EXPR_REGEX:
	case EXPR_REGEXI: {
		const char *s = NULL;
		char *p = NULL;
		char *(*fn)(char **, const char *, struct mailinfo *) = 0;
		int rv = 0;
		switch (e->a.prop) {
		case PROP_HDR: fn = msg_hdr; break;
		case PROP_HDR_ADDR: fn = msg_hdr_address; break;
		case PROP_HDR_DISP: fn = msg_hdr_display; break;
		case PROP_PATH: s = m->fpath ? m->fpath : ""; break;
		default: return 0;
		}
		for (;;) {
			if (fn && !(s = fn(&p, e->b.string, m)))
				break;
			switch (e->op) {
			case EXPR_STREQ: rv = strcmp(e->c.string, s) == 0; break;
			case EXPR_STREQI: rv = strcasecmp(e->c.string, s) == 0; break;
			case EXPR_GLOB: rv = fnmatch(e->c.string, s, 0) == 0; break;
			case EXPR_GLOBI:
				rv = fnmatch(e->c.string, s, FNM_CASEFOLD) == 0; break;
			case EXPR_REGEX:
			case EXPR_REGEXI:
				rv = regexec(e->c.regex, s, 0, 0, 0) == 0;
				break;
			}
			if (!fn || rv) return rv;
		}
		return 0;
	}
	}
	return 0;
}

struct mailinfo *
mailfile(struct mailinfo *m, char *file)
{
	static int init;
	if (!init) {
		// delay loading of the cur mail until we need to scan the first
		// file, in case someone in the pipe updated it before
		cur = blaze822_seq_cur();
		init = 1;
	}
	char *fpath = file;

	m->index = num++;
	m->file = file;

	while (*fpath == ' ' || *fpath == '\t') {
		m->depth++;
		fpath++;
	}

	char *e = fpath + strlen(fpath) - 1;
	while (fpath < e && (*e == ' ' || *e == '\t'))
		*e-- = 0;

	if (fpath[0] == '<') {
		m->flags |= FLAG_SEEN | FLAG_INFO;
		m->fpath = NULL;
		return m;
	}

	if ((e = strrchr(fpath, '/') - 1) && (e - fpath) >= 2 &&
	    *e-- == 'w' && *e-- == 'e' && *e-- == 'n')
		m->flags |= FLAG_NEW;

	if (cur && strcmp(cur, fpath) == 0) {
		m->flags |= FLAG_CUR;
		cur_idx = m->index;
	}

	char *f = strstr(fpath, MAILDIR_COLON_SPEC_VER_COMMA);
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

	m->fpath = fpath;
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
			fputs(ml->m->file, stdout);
			putc('\n', stdout);

			kept++;
		}

		/* free collected mails */
		if (ml->m->msg)
			blaze822_free(ml->m->msg);

		if (ml->m->sb)
			free(ml->m->sb);

		free(ml->m->file);
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
	char *f;

	f = xstrdup(file);
	m = xcalloc(1, sizeof *m);
	m = mailfile(m, f);

	if (m->depth == 0) {
		/* process previous thread */
		if (thr)
			do_thr();

		/* new thread */
		thr = xcalloc(1, sizeof *thr);
		thr->matched = 0;
		ml = thr->cur = thr->childs;
		thr->cur->m = m;
	} else {
		ml = thr->cur + 1;

		if (thr->cur->m->depth < m->depth) {
			/* previous mail is a parent */
			thr->cur->m->flags |= FLAG_PARENT;
			ml->parent = thr->cur;
		} else if (thr->cur->m->depth == m->depth) {
			/* same depth == same parent */
			ml->parent = thr->cur->parent;
		} else if (thr->cur->m->depth > m->depth) {
			/* find parent mail */
			struct mlist *pl;
			for (pl = thr->cur; pl->m->depth >= m->depth; pl--) ;
			ml->parent = pl;
		}

		m->flags |= FLAG_CHILD;

		thr->cur->next = ml;
		thr->cur = ml;
		ml->m = m;
	}

	for (ml = ml->parent; ml; ml = ml->parent)
		ml->m->replies++;
}

void
oneline(char *file)
{
	struct mailinfo m = { 0 };
	m.index = num++;
	(void) mailfile(&m, file);
	if (expr && !eval(expr, &m))
		goto out;

	fputs(file, stdout);
	putc('\n', stdout);
	fflush(stdout);
	kept++;

out:
	if (m.msg)
		blaze822_free(m.msg);
	if (m.sb)
		free(m.sb);
}

int
main(int argc, char *argv[])
{
	long i;
	int c;
	int vflag;

	argv0 = argv[0];
	now = time(0);
	num = 1;
	vflag = 0;

	while ((c = getopt(argc, argv, "F:Tt:v")) != -1)
		switch (c) {
		case 'F':
		{
			char *s;
			off_t len;
			int r = slurp(optarg, &s, &len);
			if (r != 0) {
				fprintf(stderr, "%s: error opening file '%s': %s\n",
				    argv0, optarg, strerror(r));
				exit(1);
			}
			expr = chain(expr, EXPR_AND, parse_buf(optarg, s));
			free(s);
			break;
		}
		case 'T': Tflag = need_thr = 1; break;
		case 't': expr = chain(expr, EXPR_AND, parse_buf("argv", optarg)); break;
		case 'v': vflag = 1; break;
		default:
			fprintf(stderr, "Usage: %s [-Tv] [-t test] [-F file] [msgs...]\n", argv0);
			exit(1);
		}

	xpledge("stdio rpath wpath cpath proc exec", 0);

	void (*cb)(char *) = need_thr ? collect : oneline;
	if (argc == optind && isatty(0))
		i = blaze822_loop1(":", cb);
	else
		i = blaze822_loop(argc-optind, argv+optind, cb);

	/* print and free last thread */
	if (Tflag && thr)
		do_thr();

	freeexpr(expr);

	if (vflag)
		fprintf(stderr, "%ld mails tested, %ld picked.\n", i, kept);

	for (; files; files = fileq) {
		fileq = files->next;
		if (files->op == EXPR_REDIR_PIPE)
			pclose(files->fp);
		else
			fclose(files->fp);
		free(files);
	}

	return 0;
}
