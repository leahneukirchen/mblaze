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
#include <unistd.h>

#include "blaze822.h"

static int rflag;
static int qflag;
static int Hflag;
static int Lflag;
static int tflag;
static char defaulthflags[] = "from:subject:to:cc:date:";
static char *hflag = defaulthflags;
static char *xflag;
static char *Oflag;

struct message *filters;

static int mimecount;

void
printhdr(char *hdr)
{
	int uc = 1;

	while (*hdr && *hdr != ':') {
		putc(uc ? toupper(*hdr) : *hdr, stdout);
		uc = (*hdr == '-');
		hdr++;
	}

	if (*hdr) {
		printf("%s\n", hdr);
	}
}

char *
mimetype(char *ct)
{
	char *s;
	for (s = ct; *s && *s != ';' && *s != ' ' && *s != '\t'; s++)
		;

	return strndup(ct, s-ct);
}

char *
tlmimetype(char *ct)
{
	char *s;
	for (s = ct; *s && *s != ';' && *s != ' ' && *s != '\t' && *s != '/'; s++)
		;

	return strndup(ct, s-ct);
}

typedef enum {
	MIME_CONTINUE,
	MIME_STOP,
	MIME_PRUNE,
} mime_action;

typedef mime_action (*mime_callback)(int, char *, char *, size_t);

mime_action
render_mime(int depth, char *ct, char *body, size_t bodylen)
{
	char *mt = mimetype(ct);
	char *tlmt = tlmimetype(ct);

	char *filename, *fn, *fne;
	if (blaze822_mime_parameter(ct, "name", &fn, &fne))
		filename = strndup(fn, fne-fn);

	mimecount++;

	int i;
	for (i = 0; i < depth+1; i++)
		printf("--- ");
	printf("%d: %s size=%zd", mimecount, mt, bodylen);
	if (filename)
		printf(" name=%s", filename);

	char *cmd;
	mime_action r = MIME_CONTINUE;

	if (filters &&
	    ((cmd = blaze822_chdr(filters, mt)) ||
	    (cmd = blaze822_chdr(filters, tlmt)))) {
		printf(" filter=\"%s\" ---\n", cmd);
		FILE *p;
		fflush(stdout);
		p = popen(cmd, "w");
		if (!p) {
			perror("popen");
			goto nofilter;
		}
		fwrite(body, bodylen, 1, p);
		if (pclose(p) != 0) {
			perror("pclose");
			goto nofilter;
		}
		r = MIME_PRUNE;
	} else {
nofilter:
		printf(" ---\n");

		if (strncmp(ct, "text/", 5) == 0) {
			fwrite(body, bodylen, 1, stdout);
		} else if (strncmp(ct, "message/rfc822", 14) == 0) {
			struct message *imsg = blaze822_mem(body, bodylen);
			char *h = 0;
			if (imsg) {
				while ((h = blaze822_next_header(imsg, h)))
					printf("%s\n", h);
				printf("\n");
			}
		} else if (strncmp(ct, "multipart/", 10) == 0) {
			;
		} else {
			printf("no filter or default handler\n");
		}
	}

	free(mt);
	free(tlmt);

	return r;
}

mime_action
list_mime(int depth, char *ct, char *body, size_t bodylen)
{
	(void) body;

	char *mt = mimetype(ct);
	char *fn, *fne;

	printf("%*.s%d: %s size=%zd", depth*2, "", ++mimecount, mt, bodylen);
	if (blaze822_mime_parameter(ct, "name", &fn, &fne)) {
		printf(" name=");
		fwrite(fn, 1, fne-fn, stdout);
	}
	printf("\n");

	return MIME_CONTINUE;
}

mime_action
walk_mime(struct message *msg, int depth, mime_callback visit)
{
	char *ct, *body, *bodychunk;
	size_t bodylen;

	mime_action r = MIME_CONTINUE;

	if (blaze822_mime_body(msg, &ct, &body, &bodylen, &bodychunk)) {

		mime_action r = visit(depth, ct, body, bodylen);

		if (r == MIME_CONTINUE) {
			if (strncmp(ct, "multipart/", 10) == 0) {
				struct message *imsg = 0;
				while (blaze822_multipart(msg, &imsg)) {
					r = walk_mime(imsg, depth+1, visit);
					if (r == MIME_STOP)
						break;
				}
			} else if (strncmp(ct, "message/rfc822", 14) == 0) {
				struct message *imsg = blaze822_mem(body, bodylen);
				if (imsg)
					walk_mime(imsg, depth+1, visit);
			}
		}

		free(bodychunk);
	}

	return r;
}

void
list(char *file)
{
	struct message *msg = blaze822_file(file);
	if (!msg)
		return;
	mimecount = 0;
	walk_mime(msg, 0, list_mime);
}

static int extract_argc;
static char **extract_argv;
static int extract_stdout;

static int
writefile(char *name, char *buf, ssize_t len)
{
	int fd = open(name, O_CREAT | O_EXCL | O_WRONLY, 0666);
	if (fd == -1) {
		perror("open");
		return -1;
	}
	if (write(fd, buf, len) != len) {
		// XXX partial write
		perror("write");
		return -1;
	}
	close(fd);
	return 0;
}

mime_action
extract_mime(int depth, char *ct, char *body, size_t bodylen)
{
	(void) body;
	(void) depth;

	char *filename = 0, *fn, *fne;

	if (blaze822_mime_parameter(ct, "name", &fn, &fne))
		filename = strndup(fn, fne-fn);

	mimecount++;

	if (extract_argc == 0) {
		if (extract_stdout) { // output all parts
			fwrite(body, bodylen, 1, stdout);
		} else { // extract all named attachments
			if (filename) {
				printf("%s\n", filename);
				writefile(filename, body, bodylen);
			}
		}
	} else {
		int i;
		for (i = 0; i < extract_argc; i++) {
			char *a = extract_argv[i];
			char *b;
			errno = 0;
			long d = strtol(a, &b, 10);
			if (errno == 0 && !*b && d == mimecount) {
				// extract by id
				if (extract_stdout) {
					fwrite(body, bodylen, 1, stdout);
				} else {
					char buf[255];
					if (!filename) {
						snprintf(buf, sizeof buf,
						    "attachment%d", mimecount);
						filename = buf;
					}
					printf("%s\n", filename);
					writefile(filename, body, bodylen);
				}
			} else if (filename && strcmp(a, filename) == 0) {
				// extract by name
				if (extract_stdout) {
					fwrite(body, bodylen, 1, stdout);
				} else {
					printf("%s\n", filename);
					writefile(filename, body, bodylen);
				}
			}
		}
	}

	free(filename);
	return MIME_CONTINUE;
}

void
extract(char *file, int argc, char **argv, int use_stdout)
{
	struct message *msg = blaze822_file(file);
	if (!msg)
		return;
	mimecount = 0;
	extract_argc = argc;
	extract_argv = argv;
	extract_stdout = use_stdout;
	walk_mime(msg, 0, extract_mime);
}

void
show(char *file)
{
	struct message *msg;

	if (qflag)
		msg = blaze822(file);
	else
		msg = blaze822_file(file);
	if (!msg)
		return;

	if (Hflag) {  // raw headers
		size_t hl = blaze822_headerlen(msg);
		char *header = malloc(hl);
		if (!header)
			return;
		int fd = open(file, O_RDONLY);
		if (fd == -1)
			return;
		hl = read(fd, header, hl);
		fwrite(header, hl, 1, stdout);
	} else if (Lflag) {  // all headers
		char *h = 0;
		while ((h = blaze822_next_header(msg, h))) {
			char d[4096];
			blaze822_decode_rfc2047(d, h, sizeof d, "UTF-8");
			printhdr(d);
		}
	} else {  // selected headers
		char *h = hflag;
		char *v;
		while (*h) {
			char *n = strchr(h, ':');
			if (n)
				*n = 0;
			v = blaze822_chdr(msg, h);
			if (v) {
				printhdr(h);
				printf(": %s\n", v);
			}
			if (n) {
				*n = ':';
				h = n + 1;
			} else {
				break;
			}
		}
	}

	if (qflag)  // no body
		goto done;

	printf("\n");

	if (rflag || !blaze822_check_mime(msg)) {  // raw body
		fwrite(blaze822_body(msg), blaze822_bodylen(msg), 1, stdout);
		goto done;
	}

	mimecount = 0;
	walk_mime(msg, 0, render_mime);

done:
	blaze822_free(msg);
}

int
main(int argc, char *argv[])
{
	int c;
	while ((c = getopt(argc, argv, "h:qrtHLx:O:")) != -1)
		switch(c) {
		case 'h': hflag = optarg; break;
		case 'q': qflag = 1; break;
		case 'r': rflag = 1; break;
		case 'H': Hflag = 1; break;
		case 'L': Lflag = 1; break;
		case 't': tflag = 1; break;
		case 'x': xflag = optarg; break;
		case 'O': Oflag = optarg; break;
                default:
                        // XXX usage
                        exit(1);
                }

	if (xflag) { // extract
		extract(xflag, argc-optind, argv+optind, 0);
	} else if (Oflag) { // extract to stdout
		extract(Oflag, argc-optind, argv+optind, 1);
	} else if (tflag) { // list
		blaze822_loop(argc-optind, argv+optind, list);
	} else { // show
		if (!(qflag || rflag))
			filters = blaze822("filters");
		blaze822_loop(argc-optind, argv+optind, show);
	}

	return 0;
}
