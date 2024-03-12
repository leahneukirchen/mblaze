#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "blaze822.h"
#include "xpledge.h"

static int Bflag;
static int rflag;
static int Rflag;
static int qflag;
static int Fflag;
static int Hflag;
static int Lflag;
static int Nflag;
static int tflag;
static int nflag;
static char defaulthflags[] = "from:subject:to:cc:date:reply-to:";
static char *hflag = defaulthflags;
static char *xflag;
static char *Oflag;

static char fallback_ct[] = "text/plain";

struct message *filters;

static int mimecount;
static int extractcount;
static int safe_output;

static int reply_found;

static char defaultAflags[] = "text/plain:text/html";
static char *Aflag = defaultAflags;

static int
printable(int c)
{
	return (unsigned)c-0x20 < 0x5f;
}

size_t
print_ascii(char *body, size_t bodylen, int oneline)
{
	if (safe_output) {
		safe_u8putstr(body, bodylen, oneline, stdout);
		return bodylen;
	} else {
		return fwrite(body, 1, bodylen, stdout);
	}
}

void
printhdr(char *hdr)
{
	int uc = 1;

	while (*hdr && *hdr != ':' && printable(*hdr)) {
		putc(uc ? toupper(*hdr) : *hdr, stdout);
		uc = (*hdr == '-');
		hdr++;
	}

	if (*hdr) {
		print_ascii(hdr, strlen(hdr), 1);
		fputc('\n', stdout);
	}
}

void
print_u8recode(char *body, size_t bodylen, char *srcenc)
{
	iconv_t ic;

	ic = iconv_open("UTF-8", srcenc);
	if (ic == (iconv_t)-1) {
		fprintf(stderr, "unsupported encoding: %s\n", srcenc);
		return;
	}

	char final_char = 0;

	char buf[4096];
	while (bodylen > 0) {
		char *bufptr = buf;
		size_t buflen = sizeof buf;
		size_t r = iconv(ic, &body, &bodylen, &bufptr, &buflen);

		if (bufptr != buf) {
			print_ascii(buf, bufptr-buf, 0);
			final_char = bufptr[-1];
		}

		if (r != (size_t)-1) {  // done, flush iconv
			bufptr = buf;
			buflen = sizeof buf;
			r = iconv(ic, 0, 0, &bufptr, &buflen);
			if (bufptr != buf) {
				print_ascii(buf, bufptr-buf, 0);
				final_char = bufptr[-1];
			}
			if (r != (size_t)-1)
				break;
		}

		if (r == (size_t)-1 && errno != E2BIG) {
			perror("iconv");
			break;
		}
	}

	if (final_char != '\n')
		printf("\n");

	iconv_close(ic);
}

char *
mimetype(char *ct)
{
	char *s;

	if (!ct)
		return 0;
	for (s = ct; *s && *s != ';' && *s != ' ' && *s != '\t'; s++)
		;

	return strndup(ct, s-ct);
}

char *
tlmimetype(char *ct)
{
	char *s;

	if (!ct)
		return 0;
	for (s = ct; *s && *s != ';' && *s != ' ' && *s != '\t' && *s != '/'; s++)
		;

	return strndup(ct, s-ct);
}

char *
mime_filename(struct message *msg)
{
	static char buf[512];
	char *v;
	char *filename = 0;

	if ((v = blaze822_hdr(msg, "content-disposition")) &&
	    blaze822_mime2231_parameter(v, "filename",
	    buf, sizeof buf, "UTF-8")) {
		filename = buf;
	} else if ((v = blaze822_hdr(msg, "content-type"))) {
		if (blaze822_mime2231_parameter(v, "name",
		    buf, sizeof buf, "UTF-8"))
			filename = buf;
	}

	if (Bflag && filename) {
		static char buf2[512];
		blaze822_decode_rfc2047(buf2, filename, sizeof buf2, "UTF-8");
		filename = buf2;
	}

	return filename;
}

static void choose_alternative(struct message *msg, int depth);

void
print_filename(char *filename)
{
	if (filename) {
		printf(" name=\"");
		safe_u8putstr(filename, strlen(filename), 1, stdout);
		printf("\"");
	}
}

static int
probably_utf8(char *charset)
{
	return !charset ||
	    strcasecmp(charset, "utf-8") == 0 ||
	    strcasecmp(charset, "utf8") == 0 ||
	    strcasecmp(charset, "us-ascii") == 0 ||
	    /* best guess for: */
	    strcasecmp(charset, "unknown-8bit") == 0 ||
	    strcasecmp(charset, "x-unknown") == 0;
}

blaze822_mime_action
render_mime(int depth, struct message *msg, char *body, size_t bodylen)
{
	char *ct = blaze822_hdr(msg, "content-type");
	if (!ct)
		ct = fallback_ct;
	char *mt = mimetype(ct);
	char *tlmt = tlmimetype(ct);
	char *filename = mime_filename(msg);

	mimecount++;

	if (!Nflag) {
		int i;
		for (i = 0; i < depth+1; i++)
			printf("--- ");
		printf("%d: %s size=%zd", mimecount, mt, bodylen);
		print_filename(filename);
	}

	char *cmd;
	blaze822_mime_action r = MIME_CONTINUE;

	if (filters &&
	    ((cmd = blaze822_chdr(filters, mt)) ||
	    (cmd = blaze822_chdr(filters, tlmt)))) {
		char *charset = 0, *cs, *cse;
		if (blaze822_mime_parameter(ct, "charset", &cs, &cse)) {
			charset = strndup(cs, cse-cs);
			if (!Nflag)
				printf(" charset=\"%s\"", charset);
			setenv("PIPE_CHARSET", charset, 1);
			free(charset);
		}
		setenv("PIPE_CONTENTTYPE", ct, 1);

		char *messageid = blaze822_hdr(msg, "message-id");
		if (messageid) {
			setenv("MESSAGE_ID", messageid, 1);
		}

		char *output;
		size_t outlen;
		int e = filter(body, bodylen, cmd, &output, &outlen);

		if (e == 0 || e == 62) { // replace output (62 == raw)
			if (!Nflag)
				printf(" render=\"%s\" ---\n", cmd);
			if (outlen) {
				if (e == 0)
					print_ascii(output, outlen, 0);
				else
					fwrite(output, 1, outlen, stdout);
				if (output[outlen-1] != '\n')
					putchar('\n');
			}
		} else if (e == 63) { // skip filter
			free(output);
			goto nofilter;
		} else if (e == 64) { // decode output again
			if (!Nflag)
				printf(" filter=\"%s\" ---\n", cmd);
			struct message *imsg = blaze822_mem(output, outlen);
			if (imsg)
				blaze822_walk_mime(imsg, depth+1, render_mime);
			blaze822_free(imsg);
		} else if (e >= 65 && e <= 80) { // choose N-64th part
			struct message *imsg = 0;
			int n = e - 64;
			if (!Nflag)
				printf(" selector=\"%s\" part=%d ---\n", cmd, n);
			while (blaze822_multipart(msg, &imsg)) {
				if (--n == 0)
					blaze822_walk_mime(imsg, depth+1, render_mime);
			}
			blaze822_free(imsg);
		} else {
			if (!Nflag)
				printf(" filter=\"%s\" FAILED status=%d", cmd, e);
			free(output);
			goto nofilter;
		}

		free(output);

		r = MIME_PRUNE;
	} else {
nofilter:
		if (!Nflag)
			printf(" ---\n");

		if (strncmp(ct, "text/", 5) == 0 ||
		    strncmp(ct, "message/delivery-status", 23) == 0) {
			char *charset = 0, *cs, *cse;
			if (blaze822_mime_parameter(ct, "charset", &cs, &cse))
				charset = strndup(cs, cse-cs);
			if (probably_utf8(charset)) {
				print_ascii(body, bodylen, 0);
				if (bodylen > 0 && body[bodylen-1] != '\n')
					putchar('\n');
			} else {
				print_u8recode(body, bodylen, charset);
			}
			free(charset);
		} else if (strncmp(ct, "message/rfc822", 14) == 0) {
			struct message *imsg = blaze822_mem(body, bodylen);
			char *h = 0;
			while (imsg && (h = blaze822_next_header(imsg, h))) {
				char d[4096];
				blaze822_decode_rfc2047(d, h, sizeof d, "UTF-8");
				printhdr(d);
			}
			printf("\n");
		} else if (strncmp(ct, "multipart/alternative", 21) == 0) {
			if (strcmp(Aflag, "all") != 0) {
				choose_alternative(msg, depth);
				r = MIME_PRUNE;
			} // else default blaze822_mime_walk action
		} else if (strncmp(ct, "multipart/", 10) == 0) {
			; // default blaze822_mime_walk action
		} else {
			printf("no filter or default handler\n");
		}
	}

	free(mt);
	free(tlmt);

	return r;
}

static void
choose_alternative(struct message *msg, int depth)
{
	int n = 0;
	int m = 0;
	char *p = Aflag + strlen(Aflag);

	struct message *imsg = 0;
	while (blaze822_multipart(msg, &imsg)) {
		m++;

		if (blaze822_bodylen(imsg) == 0)
			continue;

		char *ict = blaze822_hdr(imsg, "content-type");
		if (!ict)
			ict = fallback_ct;
		char *imt = mimetype(ict);

		char *s = strstr(Aflag, imt);
		if (s && s < p &&
		    (s[strlen(imt)] == 0 || s[strlen(imt)] == ':')) {
			p = s;
			n = m;
		}

		free(imt);
	}
	blaze822_free(imsg);

	if (n == 0) {
		// No part matched, use last part as per RFC1341 7.2.3
		// "In general, choosing the best type means displaying
		// only the LAST part that can be displayed."
		n = m;
	}

	imsg = 0;
	while (blaze822_multipart(msg, &imsg))
		if (--n == 0)
			blaze822_walk_mime(imsg, depth+1, render_mime);
	blaze822_free(imsg);
}

blaze822_mime_action
reply_mime(int depth, struct message *msg, char *body, size_t bodylen)
{
	(void)depth;

	char *ct = blaze822_hdr(msg, "content-type");
	char *mt = mimetype(ct);
	char *tlmt = tlmimetype(ct);

	if (!ct || strncmp(ct, "text/plain", 10) == 0) {
		char *charset = 0, *cs, *cse;
		if (blaze822_mime_parameter(ct, "charset", &cs, &cse))
			charset = strndup(cs, cse-cs);
		if (probably_utf8(charset))
			print_ascii(body, bodylen, 0);
		else
			print_u8recode(body, bodylen, charset);
		reply_found++;
		free(charset);
	}

	free(mt);
	free(tlmt);

	return MIME_CONTINUE;
}

blaze822_mime_action
list_mime(int depth, struct message *msg, char *body, size_t bodylen)
{
	(void)body;

	char *ct = blaze822_hdr(msg, "content-type");
	if (!ct)
		ct = fallback_ct;
	char *mt = mimetype(ct);
	char *filename = mime_filename(msg);

	printf("  %*.s%d: %s size=%zd", depth*2, "", ++mimecount, mt, bodylen);
	print_filename(filename);
	printf("\n");

	free(mt);
	return MIME_CONTINUE;
}

void
list(char *file)
{
	while (*file == ' ' || *file == '\t')
		file++;
	struct message *msg = blaze822_file(file);
	if (!msg)
		return;
	mimecount = 0;
	printf("%s\n", file);
	blaze822_walk_mime(msg, 0, list_mime);
}

void
reply(char *file)
{
	while (*file == ' ' || *file == '\t')
		file++;
	struct message *msg = blaze822_file(file);
	if (!msg)
		return;
	reply_found = 0;
	blaze822_walk_mime(msg, 0, reply_mime);
	if (!reply_found)
		exit(1);
}

static int extract_argc;
static char **extract_argv;
static int extract_stdout;


static const char *
basenam(const char *s)
{
	char *r = strrchr(s, '/');
	return r ? r + 1 : s;
}

static int
writefile(char *name, char *buf, ssize_t len)
{
	int fd = open(basenam(name), O_CREAT | O_EXCL | O_WRONLY, 0666);
	if (fd == -1) {
		perror("open");
		return -1;
	}

	ssize_t wr = 0, n;
	do {
		if ((n = write(fd, buf + wr, len - wr)) == -1) {
			if (errno == EINTR) {
				continue;
			} else {
				perror("write");
				return -1;
			}
		}
		wr += n;
	} while (wr < len);

	close(fd);
	return 0;
}

blaze822_mime_action
extract_mime(int depth, struct message *msg, char *body, size_t bodylen)
{
	(void)depth;

	char *filename = mime_filename(msg);

	mimecount++;

	if (extract_argc == 0) {
		if (extract_stdout) { // output all parts
			fwrite(body, 1, bodylen, stdout);
		} else { // extract all named attachments
			if (filename) {
				safe_u8putstr(filename, strlen(filename), 1, stdout);
				printf("\n");
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
					if (rflag) {
						fwrite(blaze822_orig_header(msg),
						    1, blaze822_headerlen(msg),
						    stdout);
						if (blaze822_orig_header(msg)[
						        blaze822_headerlen(msg)]
						    == '\r')
							printf("\r\n\r\n");
						else
							printf("\n\n");
						fwrite(blaze822_body(msg),
						    1, blaze822_bodylen(msg),
						    stdout);
					} else {
						fwrite(body, 1, bodylen, stdout);
					}
				} else {
					char buf[255];
					char *bufptr;
					if (filename) {
						bufptr = filename;
					} else {
						snprintf(buf, sizeof buf,
						    "attachment%d", mimecount);
						bufptr = buf;
					}
					printf("%s\n", bufptr);
					writefile(bufptr, body, bodylen);
				}
				extractcount++;
			} else if (filename &&
			    fnmatch(a, filename, FNM_PATHNAME) == 0) {
				// extract by name
				if (extract_stdout) {
					if (rflag) {
						fwrite(blaze822_orig_header(msg),
						    1, blaze822_headerlen(msg),
						    stdout);
						printf("\n\n");
						fwrite(blaze822_body(msg),
						    1, blaze822_bodylen(msg),
						    stdout);
					} else {
						fwrite(body, 1, bodylen, stdout);
					}
				} else {
					safe_u8putstr(filename, strlen(filename), 1, stdout);
					printf("\n");
					writefile(filename, body, bodylen);
				}
				extractcount++;
			}
		}
	}

	return MIME_CONTINUE;
}

void
extract_cb(char *file)
{
	while (*file == ' ' || *file == '\t')
		file++;
	struct message *msg = blaze822_file(file);
	if (!msg)
		return;
	mimecount = 0;
	blaze822_walk_mime(msg, 0, extract_mime);
}

void
extract(char *file, int argc, char **argv, int use_stdout)
{
	extract_argc = argc;
	extract_argv = argv;
	extract_stdout = use_stdout;
	blaze822_loop1(file, extract_cb);
}

static char *newcur;

static void
print_date_header(char *v)
{
	static time_t now = -1;
	if (now == -1) {
		setenv("TZ", "", 1);
		tzset();
		now = time(0);
	}

	printf("Date: ");
	print_ascii(v, strlen(v), 1);

	time_t t = blaze822_date(v);
	if (t == -1) {
		printf(" (invalid)");
	} else {
		printf(" (");
		time_t d = t < now ? now - t : t - now;

		char l;
		if (d > 60*60*24*7*52) l = 'y';
		else if (d > 60*60*24*7) l = 'w';
		else if (d > 60*60*24) l = 'd';
		else if (d > 60*60) l = 'h';
		else if (d > 60) l = 'm';
		else l = 's';
		int p = 3;

		long z;
		switch (l) {
		case 'y':
			z = d / (60*60*24*7*52);
			d = d % (60*60*24*7*52);
			if (z > 0) {
				printf("%ld year%s", z, z > 1 ? "s" : "");
				if (!--p) break;
				printf(", ");
			}
		/* FALL THROUGH */
		case 'w':
			z = d / (60*60*24*7);
			d = d % (60*60*24*7);
			if (z > 0) {
				printf("%ld week%s", z, z > 1 ? "s" : "");
				if (!--p) break;
				printf(", ");
			}
		/* FALL THROUGH */
		case 'd':
			z = d / (60*60*24);
			d = d % (60*60*24);
			if (z > 0) {
				printf("%ld day%s", z, z > 1 ? "s" : "");
				if (!--p) break;
				printf(", ");
			}
		/* FALL THROUGH */
		case 'h':
			z = d / (60*60);
			d = d % (60*60);
			if (z > 0) {
				printf("%ld hour%s", z, z > 1 ? "s" : "");
				if (!--p) break;
				printf(", ");
			}
		/* FALL THROUGH */
		case 'm':
			z = d / (60);
			d = d % (60);
			if (z > 0) {
				printf("%ld minute%s", z, z > 1 ? "s" : "");
				if (!--p) break;
				printf(", ");
			}
		/* FALL THROUGH */
		case 's':
			z = d;
			printf("%ld second%s", z, z > 1 ? "s" : "");
		}

		if (t < now)
			printf(" ago)");
		else
			printf(" in the future)");
	}
	printf("\n");
}

static void
print_decode_header(char *h, char *v)
{
	char d[16384];
	blaze822_decode_rfc2047(d, v, sizeof d, "UTF-8");
	printhdr(h);
	fputc(':', stdout);
	fputc(' ', stdout);
	print_ascii(d, strlen(d), 1);
	fputc('\n', stdout);
}

void
show(char *file)
{
	struct message *msg;

	while (*file == ' ' || *file == '\t')
		file++;

	if (newcur) {
		printf("\014\n");
		free(newcur);
	}
	newcur = strdup(file);

	if (qflag && !Hflag)
		msg = blaze822(file);
	else
		msg = blaze822_file(file);
	if (!msg) {
		fprintf(stderr, "mshow: %s: %s\n", file, strerror(errno));
		return;
	}

	if (Hflag) {  // raw headers
		fwrite(blaze822_orig_header(msg), 1, blaze822_headerlen(msg),
		    stdout);
		printf("\n");
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
				if (strcasecmp("date", h) == 0)
					print_date_header(v);
				else
					print_decode_header(h, v);
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

	if (rflag) {  // raw body
		print_ascii(blaze822_body(msg), blaze822_bodylen(msg), 0);
		goto done;
	}

	mimecount = 0;
	blaze822_walk_mime(msg, 0, render_mime);

done:
	blaze822_free(msg);
}

int
main(int argc, char *argv[])
{
	pid_t pid1 = -1, pid2 = -1;

	int c;
	while ((c = getopt(argc, argv, "h:A:BqrtFHLNx:O:Rn")) != -1)
		switch (c) {
		case 'h': hflag = optarg; break;
		case 'A': Aflag = optarg; break;
		case 'B': Bflag = 1; break;
		case 'q': qflag = 1; break;
		case 'r': rflag = 1; break;
		case 'F': Fflag = 1; break;
		case 'H': Hflag = 1; break;
		case 'L': Lflag = 1; break;
		case 'N': Nflag = 1; break;
		case 't': tflag = 1; break;
		case 'x': xflag = optarg; break;
		case 'O': Oflag = optarg; break;
		case 'R': Rflag = 1; break;
		case 'n': nflag = 1; break;
		default:
			fprintf(stderr,
			    "Usage: mshow [-h headers] [-A mimetypes] [-nqrFHLN] [msgs...]\n"
			    "	    mshow -x msg parts...\n"
			    "	    mshow -O msg parts...\n"
			    "	    mshow -t msgs...\n"
			    "	    mshow -R msg\n"
			);
			exit(1);
		}

	xpledge("stdio rpath wpath cpath proc exec", NULL);

	if (!rflag && !xflag && !Oflag && !Rflag && !tflag)
		safe_output = 1;

	if (safe_output && isatty(1)) {
		char *pg;
		pg = getenv("MBLAZE_PAGER");
		if (!pg) {
			pg = getenv("PAGER");
			if (pg && strcmp(pg, "less") == 0) {
				static char lesscmd[] = "less -RFX";
				pg = lesscmd;
			}
		}
		if (pg && *pg && strcmp(pg, "cat") != 0) {
			pid2 = pipeto(pg);
			if (pid2 < 0)
				fprintf(stderr,
				    "mshow: spawning pager '%s': %s\n",
				    pg, strerror(errno));
			else if (!getenv("MBLAZE_NOCOLOR"))
				pid1 = pipeto("mcolor");  // ignore error
		}
	}

	if (xflag) { // extract
		xpledge("stdio rpath wpath cpath", NULL);
		extract(xflag, argc-optind, argv+optind, 0);
	} else if (Oflag) { // extract to stdout
		xpledge("stdio rpath", NULL);
		extract(Oflag, argc-optind, argv+optind, 1);
	} else if (tflag) { // list
		xpledge("stdio rpath", NULL);
		if (argc == optind && isatty(0))
			blaze822_loop1(".", list);
		else
			blaze822_loop(argc-optind, argv+optind, list);
	} else if (Rflag) { // render for reply
		xpledge("stdio rpath", NULL);
		blaze822_loop(argc-optind, argv+optind, reply);
	} else { // show
		/* XXX pledge: still r/w on the whole file-system + fork/exec */
		if (!(qflag || rflag || Fflag)) {
			char *f = getenv("MAILFILTER");
			if (!f)
				f = blaze822_home_file("filter");
			if (f)
				filters = blaze822(f);
		}
		if (argc == optind && isatty(0))
			blaze822_loop1(".", show);
		else
			blaze822_loop(argc-optind, argv+optind, show);
		if (!nflag) // don't set cur
			if (newcur)
				blaze822_seq_setcur(newcur);
	}

	if (pid2 > 0)
		pipeclose(pid2);
	if (pid1 > 0)
		pipeclose(pid1);

	if ((xflag || Oflag) && extractcount < argc-optind) {
		fprintf(stderr, "mshow: could not extract all attachments\n");
		return 1;
	}

	return 0;
}
