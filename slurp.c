#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

int
slurp(char *filename, char **bufo, off_t *leno)
{
	int fd;
	struct stat st;
	ssize_t nread = 0;
	ssize_t n;
	int r = 0;
	
	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		r = errno;
		goto out;
	}
	if (fstat(fd, &st) < 0) {
		r = errno;
		goto out;
	}
	if (st.st_size == 0) {
		*bufo = "";
		*leno = 0;
		return 0;
	}
	*bufo = malloc(st.st_size + 1);
	if (!*bufo) {
		r = ENOMEM;
		goto out;
	}

	do {
		if ((n = read(fd, *bufo + nread, st.st_size - nread)) < 0) {
			if (errno == EINTR) {
				continue;
			} else {
				r = errno;
				goto out;
			}
		}
		if (!n)
			break;
		nread += n;
	} while (nread < st.st_size);

	*leno = nread;
	(*bufo)[st.st_size] = 0;

out:
	close(fd);
	return r;
}
