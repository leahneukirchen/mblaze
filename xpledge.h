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

#endif /* __OpenBSD__ */

#elif

#define xpledge(promises, execpromises)) 0

#endif /* PLEDGE_H */
