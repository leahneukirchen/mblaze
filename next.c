#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int
main(int argc, char *argv[])
{
	int fd;
	struct stat st;

	fd = open("map", O_RDONLY);
	if (!fd)
		exit(101);

	fstat(fd, &st);
	char *map = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED)
		exit(102);

	char *s;
	char *arg;
	if (argc > 2)
		arg = argv[2];
	else
		arg = "+1";

	if (argc <= 1 || !argv[1][0]) {
		// default to first line
		arg = "1";
	}

	if (*arg == '+' || *arg == '-') {
		s = strstr(map, argv[1]);
		int a = atoi(arg);
		if (!s) {
			s = map;
			a = 0;
		}
		if (a > 0) {
			while (a-- > 0) {
				while (*s && *s != '\n')
					s++;
				s++;
			}
		} else {
			while (a++ <= 0 && map < s) {
				s--;
				while (map < s && *s != '\n')
					s--;
			}
			if (map < s && *s == '\n')
				s++;
		}
	} else {
		int a = atoi(arg);
		s = map;
		while (--a > 0) {
			char *n = strchr(s+1, '\n');
			if (!n)
				break;
			s = n;
		}
		if (s && *s)
			s++;
	}
	char *t = s;
	while (*t && *t != '\n')
		t++;
	if (!*t)
		return 1;
	t++;
	if (write(1, s, t-s) <= 0)
		return 2;
	return 0;
}
