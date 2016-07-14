#define _GNU_SOURCE

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

#include "blaze822.h"

void
recmime(struct message *msg, int depth)
{
	struct message *imsg = 0;
	char *ct, *body, *bodychunk;
	size_t bodylen;

	if (blaze822_mime_body(msg, &ct, &body, &bodylen, &bodychunk)) {
		printf("%*.sbody %s len %zd\n", depth*2, "", ct, bodylen);
		if (strncmp(ct, "multipart/", 10) == 0) {
			while (blaze822_multipart(msg, &imsg))
				recmime(imsg, depth+1);
		} else if (strncmp(ct, "text/", 5) == 0) {
			printf("---\n");
			fwrite(body, bodylen, 1, stdout);
			printf("---\n");
		}
		free(bodychunk);
	}
}

void
unmime(char *file)
{
	struct message *msg;

	msg = blaze822_file(file);
	if (!msg)
		return;

	if (blaze822_check_mime(msg))
		printf("a mime message\n");
	else
		return;

	recmime(msg, 0);

	blaze822_free(msg);
}

int
main(int argc, char *argv[])
{
	blaze822_loop(argc-1, argv+1, unmime);

	return 0;
}
