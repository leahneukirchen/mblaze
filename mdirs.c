#include <sys/stat.h>
#include <sys/types.h>

#include <dirent.h>
#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void
pwd()
{
	char cwd[PATH_MAX];
	if (getcwd(cwd, sizeof cwd))
		puts(cwd);
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
#if defined(DT_DIR) && defined(DT_UNKNOWN)
		if (d->d_type != DT_DIR && d->d_type != DT_UNKNOWN)
			continue;
#endif
		if (d->d_name[0] == '.' &&
		    d->d_name[1] == 0)
			continue;
		if (d->d_name[0] == '.' &&
		    d->d_name[1] == '.' &&
		    d->d_name[2] == 0)
			continue;

		if (dotonly && d->d_name[0] != '.')
			continue;
		
		mdirs(d->d_name);
	}
		
	if (chdir("..") < 0)
		exit(-1);

	closedir(dir);
}

int
main(int argc, char *argv[])
{
	int c, i;
	while ((c = getopt(argc, argv, "")) != -1)
		switch(c) {
		default:
		usage:
			fprintf(stderr, "Usage: mdirs dirs...\n");
			exit(1);
		}

	if (argc == optind)
		goto usage;

        if (pledge("stdio rpath tty", NULL) == -1)
          err(1, "pledge");

	for (i = 0; i < argc; i++)
		mdirs(argv[i]);

	return 0;
}
