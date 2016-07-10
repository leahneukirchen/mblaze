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
	if (!argv[1][0] || !s) {
		// default to first line
		s = map;
	} else {
		while (*s && *s != '\n')
			s++;
		s++;
	}
	char *t = s;
	while (*t && *t != '\n')
		t++;
	if (!*t)
		exit(1);
	t++;
	write(1, s, t-s);

	return 0;
}
