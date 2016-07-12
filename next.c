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
	if (!map)
		exit(102);

	char *s = strstr(map, argv[1]);
	int a = 1;
	if (argc > 2)
		a = atoi(argv[2]);

	if (!argv[1][0] || !s) {
		// default to first line
		s = map;
	} else {
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
