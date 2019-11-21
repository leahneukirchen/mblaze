#include <time.h>
#include <unistd.h>

#include "xpledge.h"

int
main()
{
	char buf[64];
	time_t now;

	xpledge("stdio", "");

	now = time(0);

	ssize_t l = strftime(buf, sizeof buf,
	    "%a, %d %b %Y %T %z\n", localtime(&now));
	if (write(1, buf, l) == l)
		return 0;

	return 1;
}
