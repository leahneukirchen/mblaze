#ifndef PLEDGE_H
#define PLEDGE_H

#ifdef __OpenBSD__

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include <err.h>
#include <unistd.h>

static void
xpledge(const char *promises, const char *execpromises)
{
	if (pledge(promises, execpromises) == -1)
		err(1, "pledge");
}

#else

#define xpledge(promises, execpromises) do { } while(0)

#endif /* __OpenBSD__ */

#endif /* PLEDGE_H */
