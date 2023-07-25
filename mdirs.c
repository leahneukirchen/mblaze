#include <sys/stat.h>
#include <sys/types.h>

#include <dirent.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blaze822.h"
#include "blaze822_priv.h"
#include "xpledge.h"

static char sep = '\n';
int aflag;

void
pwd()
{
	char cwd[PATH_MAX];
	if (getcwd(cwd, sizeof cwd))
		printf("%s%c", cwd, sep);
}

void
mdirs(char *fpath)
{
	DIR *dir;
	struct dirent *d;
	struct stat st;

	dir = opendir(fpath);
	if (!dir)
		return;

	if (chdir(fpath) < 0) {
		closedir(dir);
		return;
	}

	int dotonly = 0;

	if (stat("cur", &st) == 0 &&
	    S_ISDIR(st.st_mode) &&
	    stat("new", &st) == 0 &&
	    S_ISDIR(st.st_mode)) {
		pwd();
		dotonly = 1;   // Maildir++
	}

	while ((d = readdir(dir))) {
		if (!DIR_DT(d->d_type))
			continue;

		if (d->d_name[0] == '.' &&
		    d->d_name[1] == 0)
			continue;
		if (d->d_name[0] == '.' &&
		    d->d_name[1] == '.' &&
		    d->d_name[2] == 0)
			continue;

		if (!aflag && dotonly && d->d_name[0] != '.')
			continue;

		mdirs(d->d_name);
	}

	if (chdir("..") < 0)
		exit(-1);

	closedir(dir);
}

char *
profile_maildir()
{
	char *f = blaze822_home_file("profile");
	struct message *config = blaze822(f);
	char *maildir;
	static char path[PATH_MAX];

	if (!config)
		return 0;

	if (!(maildir = blaze822_hdr(config, "maildir")))
		return 0;

	if (strncmp(maildir, "~/", 2) == 0) {
		const char *home = getenv("HOME");
		if (!home) {
			struct passwd *pw = getpwuid(getuid());
			home = pw ? pw->pw_dir : "/dev/null/homeless";
		}
		snprintf(path, sizeof path, "%s/%s", home, maildir+2);
		maildir = path;
	}

	return maildir;
}

int
main(int argc, char *argv[])
{
	int c, i;
	while ((c = getopt(argc, argv, "0a")) != -1)
		switch (c) {
		case '0': sep = '\0'; break;
		case 'a': aflag = 1; break;
		default:
usage:
			fprintf(stderr, "Usage: mdirs [-0a] dirs...\n");
			exit(1);
		}

	xpledge("stdio rpath", "");

	if (argc == optind) {
		char *maildir = profile_maildir();
		if (maildir) {
			mdirs(maildir);
			return 0;
		}
		goto usage;
	}

	char toplevel[PATH_MAX];
	if (!getcwd(toplevel, sizeof toplevel)) {
		perror("mdirs: getcwd");
		exit(-1);
	}

	for (i = 0; i < argc; i++) {
		mdirs(argv[i]);
		if (chdir(toplevel) < 0) {
			perror("mdirs: chdir");
			exit(-1);
		}
	}

	return 0;
}
