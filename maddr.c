#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blaze822.h"

static char defaulthflags[] = "from:sender:reply-to:to:cc:bcc:"
    "resent-from:resent-sender:resent-to:resent-cc:resent-bcc:";
static char *hflag = defaulthflags;

void
addr(char *file)
{
	while (*file == ' ' || *file == '\t')
		file++;
	
	struct message *msg = blaze822(file);
        if (!msg)
		return;

	char *h = hflag;
	char *v;
	while (*h) {
		char *n = strchr(h, ':');
		if (n)
			*n = 0;
		v = blaze822_chdr(msg, h);
		if (v) {
			char *disp, *addr;
			while ((v = blaze822_addr(v, &disp, &addr))) {
				if (disp && addr && strcmp(disp, addr) == 0)
					disp = 0;
				if (disp && addr) {
					char dispdec[1024];
					blaze822_decode_rfc2047(dispdec, disp,
					    sizeof dispdec - 1, "UTF-8");
					dispdec[sizeof dispdec - 1] = 0;

					printf("%s <%s>\n", dispdec, addr);
				} else if (addr) {
					printf("%s\n", addr);
				}
			}
		}
		if (n) {
			*n = ':';
			h = n + 1;
		} else {
			break;
		}
	}
}

int
main(int argc, char *argv[])
{
        int c;
        while ((c = getopt(argc, argv, "h:")) != -1)
                switch(c) {
                case 'h': hflag = optarg; break;
                default:
                        // XXX usage
                        exit(1);
                }			

        if (argc == optind && isatty(0))
                blaze822_loop1(":", addr);
        else
		blaze822_loop(argc-optind, argv+optind, addr);

        return 0;
}
