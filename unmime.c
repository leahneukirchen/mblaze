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

struct message *filters;

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

void
recmime(struct message *msg, int depth)
{
	char *ct, *body, *bodychunk;
	size_t bodylen;

	if (blaze822_mime_body(msg, &ct, &body, &bodylen, &bodychunk)) {
		char *mt = mimetype(ct);
		char *tlmt = tlmimetype(ct);
		printf("%*.sbody %s len %zd\n", depth*2, "", mt, bodylen);

		char *cmd;

		if (filters && ((cmd = blaze822_chdr(filters, mt)) ||
				(cmd = blaze822_chdr(filters, tlmt)))) {
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
		} else {
nofilter:
			if (strncmp(ct, "multipart/", 10) == 0) {
				struct message *imsg = 0;
				while (blaze822_multipart(msg, &imsg))
					recmime(imsg, depth+1);
			} else if (strncmp(ct, "text/", 5) == 0) {
				printf("---\n");
				fwrite(body, bodylen, 1, stdout);
				printf("---\n");
			} else if (strncmp(ct, "message/rfc822", 14) == 0) {
				struct message *imsg = blaze822_mem(body, bodylen);
				char *h = 0;
				if (imsg) {
					while ((h = blaze822_next_header(imsg, h)))
						printf("%s\n", h);
					printf("\n");
					recmime(imsg, depth+1);
				}
			} else {
				printf("no filter or default handler\n");
			}
		}
		free(mt);
		free(tlmt);
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
	filters = blaze822("filters");
	blaze822_loop(argc-1, argv+1, unmime);

	return 0;
}
