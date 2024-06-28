#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blaze822.h"
#include "blaze822_priv.h"
#include "xpledge.h"

static int fflag;
static int rflag;
static int Aflag;
static char *cflag;
static char *Cflag;
static int Sflag;

struct name {
	char *id;
	char *file;
};

static void *names;

int
nameorder(const void *a, const void *b)
{
	struct name *ia = (struct name *)a;
	struct name *ib = (struct name *)b;

	return strcmp(ia->id, ib->id);
}

char *
namefind(char *id)
{
	struct name key, **result;
	key.id = id;

	if (!(result = tfind(&key, &names, nameorder)))
		return 0;

	return (*result)->file;
}

void
namescan(char *dir)
{
	DIR *fd;
	struct dirent *d;

	fd = opendir(dir);
	if (!fd)
		return;
	while ((d = readdir(fd))) {
		if (!MAIL_DT(d->d_type))
			continue;

		if (d->d_name[0] == '.')
			continue;

		char file[PATH_MAX];
		snprintf(file, sizeof file, "%s/%s", dir, d->d_name);

		char *e;
		if ((e = strstr(d->d_name, MAILDIR_COLON_SPEC_VER_COMMA)))
			*e = 0;

		struct name *c = malloc(sizeof (struct name));
		c->id = strdup(d->d_name);
		c->file = strdup(file);
		tsearch(c, &names, nameorder);
	}
	closedir(fd);

	// add dir so know we scanned it already
	struct name *c = malloc(sizeof (struct name));
	c->id = c->file = strdup(dir);
	tsearch(c, &names, nameorder);
}

char *
search(char *file)
{
	char dir[PATH_MAX];
	char *e;
	if ((e = strrchr(file, '/'))) {
		snprintf(dir, sizeof dir, "%.*s", (int)(e-file), file);
		file = e+1;
	} else {
		snprintf(dir, sizeof dir, ".");
	}

	if (!namefind(dir))
		namescan(dir);

	if ((e = strstr(file, MAILDIR_COLON_SPEC_VER_COMMA)))
		*e = 0;

	return namefind(file);
}

/* strategy to find a Maildir file name:
   - if file exists, all good
   - try a few common different flags
   - index the containing dir, try to search for the id
   - if its in new/, try the same in cur/
 */
int
fix(FILE *out, char *file)
{
	int i;
	for (i = 0; *file == ' '; i++, file++)
		;

	char buf[PATH_MAX];
	char *bufptr = buf;

	if (*file == '<' || access(file, F_OK) == 0) {
		bufptr = file;
		goto ok;
	}

	char *e;
	char *sep;

	if ((e = strstr(file, MAILDIR_COLON_SPEC_VER_COMMA))) {
		sep = "";
		e[3] = 0;
	} else {
		sep = MAILDIR_COLON_SPEC_VER_COMMA;
	}
	snprintf(buf, sizeof buf, "%s%s", file, sep);
	if (access(buf, F_OK) == 0) goto ok;
	snprintf(buf, sizeof buf, "%s%sS", file, sep);
	if (access(buf, F_OK) == 0) goto ok;
	snprintf(buf, sizeof buf, "%s%sRS", file, sep);
	if (access(buf, F_OK) == 0) goto ok;
	snprintf(buf, sizeof buf, "%s%sFS", file, sep);
	if (access(buf, F_OK) == 0) goto ok;

	if ((bufptr = search(file))) goto ok;

	char *ee = strrchr(file, '/');
	if (ee >= file + 3 && ee[-3] == 'n' && ee[-2] == 'e' && ee[-1] == 'w') {
		ee[-3] = 'c'; ee[-2] = 'u'; ee[-1] = 'r';
		return fix(out, file-i);
	}

	return 0;
ok:
	while (i--)
		putc(' ', out);
	fprintf(out, "%s\n", bufptr);
	return 1;
}

void
cat(FILE *src, FILE *dst)
{
	char buf[4096];
	size_t rd;

	rewind(src);
	while ((rd = fread(buf, 1, sizeof buf, src)) > 0)
		fwrite(buf, 1, rd, dst);
	if (!feof(src)) {
		perror("mseq: fread");
		exit(2);
	}
}

int
stdinmode()
{
	char *line = 0;
	char *l;
	size_t linelen = 0;
	ssize_t rd;
	FILE *outfile;

	char tmpfile[PATH_MAX];
	char oldfile[PATH_MAX];
	char *seqfile = 0;

	if (Sflag) {
		seqfile = getenv("MAILSEQ");
		if (!seqfile)
			seqfile = blaze822_home_file("seq");
		snprintf(tmpfile, sizeof tmpfile, "%s-", seqfile);
		snprintf(oldfile, sizeof oldfile, "%s.old", seqfile);
		int fd = open(tmpfile, O_RDWR | O_EXCL | O_CREAT, 0666);
		if (fd < 0) {
			fprintf(stderr,
			    "mseq: Could not create temporary sequence file '%s': %s.\n",
			    tmpfile, strerror(errno));
			fprintf(stderr,
			    "mseq: Ensure %s exists and is writable.\n",
			    blaze822_home_file(""));
			exit(2);
		}
		outfile = fdopen(fd, "w+");
		if (Aflag) {
			FILE *seq = fopen(seqfile, "r");
			if (seq) {
				cat(seq, outfile);
				fclose(seq);
			}
		}
	} else {
		outfile = stdout;
	}

	while ((rd = getdelim(&line, &linelen, '\n', stdin)) != -1) {
		if (line[rd-1] == '\n')
			line[rd-1] = 0;

		l = line;
		if (rflag)
			while (*l == ' ' || *l == '\t')
				l++;
		if (fflag)
			fix(outfile, l);
		else
			fprintf(outfile, "%s\n", l);
	}

	if (Sflag) {
		fflush(outfile);
		if (rename(seqfile, oldfile) < 0 && errno != ENOENT) {
			perror("mseq: rename");
			exit(2);
		}
		if (rename(tmpfile, seqfile) < 0) {
			perror("mseq: rename");
			exit(2);
		}

		if (!isatty(1))
			cat(outfile, stdout);

		fclose(outfile);
	}

	free(line);
	return 0;
}

void
overridecur(char *file)
{
	static int once = 0;
	if (once++)
		return;
	while (*file == ' ')
		file++;
	setenv("MAILDOT", file, 1);
}

void
setcur(char *file)
{
	static int once = 0;
	if (once++)
		return;
	while (*file == ' ')
		file++;
	unsetenv("MAILDOT");
	blaze822_seq_setcur(file);
}

int
main(int argc, char *argv[])
{
	int c;
	while ((c = getopt(argc, argv, "c:frAC:S")) != -1)
		switch (c) {
		case 'c': cflag = optarg; break;
		case 'f': fflag = 1; break;
		case 'r': rflag = 1; break;
		case 'A': Sflag = Aflag = 1; break;
		case 'C': Cflag = optarg; break;
		case 'S': Sflag = 1; break;
		default:
usage:
			fprintf(stderr,
			    "Usage: mseq [-fr] [-c msg] [msgs...]\n"
			    "	    mseq -S [-fr] < sequence\n"
			    "	    mseq -A [-fr] < sequence\n"
			    "	    mseq -C msg\n"
			);
			exit(1);
		}

	xpledge("stdio rpath wpath cpath", "");

	if (cflag)
		blaze822_loop1(cflag, overridecur);

	if (Cflag) {
		blaze822_loop1(Cflag, setcur);
		return 0;
	}

	if (Sflag && optind != argc) {
		fprintf(stderr, "error: -S/-A doesn't take arguments.\n");
		goto usage;
	}

	if (optind == argc && !isatty(0))
		return stdinmode();

	int i;
	char *f;
	char *a;
	char *seq = 0;

	if (optind == argc) {
		a = ":";
		i = argc;
		goto hack;
	}
	for (i = optind; i < argc; i++) {
		a = argv[i];
hack:
		if (strchr(a, '/')) {
			printf("%s\n", a);
			continue;
		}

		if (!seq) {
			seq = blaze822_seq_open(0);
			if (!seq)
				return 1;
		}

		struct blaze822_seq_iter iter = { 0 };
		while ((f = blaze822_seq_next(seq, a, &iter))) {
			char *s = f;
			if (rflag)
				while (*s == ' ' || *s == '\t')
					s++;
			if (fflag)
				fix(stdout, s);
			else
				printf("%s\n", s);
			free(f);
		}
	}

	return 0;
}
