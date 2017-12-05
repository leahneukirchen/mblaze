#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <fcntl.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "blaze822.h"

void
printb36(uint64_t x)
{
	static char const base36[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	char outbuf[16];
	char *o = outbuf + sizeof outbuf;

	*--o = 0;
	do { *--o = base36[x % 36]; } while (x /= 36);

	fputs(o, stdout);
}

int main()
{
	char hostbuf[1024];
	char *host = 0;

	char *f = blaze822_home_file("profile");
	struct message *config = blaze822(f);
#if defined(__OpenBSD__)
	if (pledge("stdio rpath tty", NULL) == -1)
	  err(1, "pledge");
#endif
	if (config) // try FQDN: first
		host = blaze822_hdr(config, "fqdn");

	if (!host && gethostname(hostbuf, sizeof hostbuf) == 0) {
		// termination not posix guaranteed
		hostbuf[sizeof hostbuf - 1] = 0;

		struct addrinfo hints = { .ai_family = AF_UNSPEC,
					  .ai_socktype = SOCK_STREAM,
					  .ai_flags = AI_CANONNAME };
		struct addrinfo *info;
		if (getaddrinfo(hostbuf, 0, &hints, &info) == 0) {
			// sanity checks: no (null), at least one dot,
			// doesn't start with localhost.

			if (info &&
			    info->ai_canonname &&
			    strchr(info->ai_canonname, '.') &&
			    strncmp(info->ai_canonname, "localhost.", 10) != 0)
				host = info->ai_canonname;
		}
	}

	if (!host && config) {
		// get address part of Local-Mailbox:
		char *disp, *addr;
		char *from = blaze822_hdr(config, "local-mailbox");
		while (from && (from = blaze822_addr(from, &disp, &addr)))
			if (addr) {
				host = strchr(addr, '@');
				if (host) {
					host++;
					break;
				}
			}
	}

	if (!host) {
		fprintf(stderr,
		    "mgenmid: failed to find a FQDN for the Message-ID.\n"
		    " Define 'FQDN:' or 'Local-Mailbox:' in"
		    " ${MBLAZE:-$HOME/.mblaze}/profile\n"
		    " or add a FQDN to /etc/hosts.\n");
		exit(1);
	}

	struct timeval tp;
	gettimeofday(&tp, (struct timezone *)0);

	uint64_t rnd;

	int rndfd = open("/dev/urandom", O_RDONLY);
	if (rndfd >= 0) {
		unsigned char rndb[8];
		if (read(rndfd, rndb, sizeof rndb) != sizeof rndb)
			goto fallback;
		close(rndfd);

		int i;
		for (i = 0, rnd = 0; i < 8; i++)
			rnd = rnd*256 + rndb[i];
	} else {
fallback:
		srand48(tp.tv_sec ^ tp.tv_usec ^ getpid());
		rnd = ((uint64_t)lrand48() << 32) + lrand48();
	}

	rnd |= (1ULL << 63);  // set highest bit to force full width

	putchar('<');
	printb36(((uint64_t)tp.tv_sec * 1000000LL + tp.tv_usec));
	putchar('.');
	printb36(rnd);
	putchar('@');
	fputs(host, stdout);
	putchar('>');
	putchar('\n');

	return 0;
}
