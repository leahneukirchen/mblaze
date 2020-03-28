#include <time.h>

time_t
mytimegm(struct tm *tm)
{
	int mon = tm->tm_mon + 1 - 2;  /* put March first, Feb last */
	long long year = tm->tm_year + 1900;

	if (mon <= 0 || mon >= 12) {
		int adj = mon / 12;
		mon %= 12;
		if (mon <= 0) {
			adj--;
			mon += 12;
		}
		year += adj;
	}

	time_t t = 0;
	t +=            tm->tm_sec;
	t +=       60 * tm->tm_min;
	t +=    60*60 * tm->tm_hour;
	t += 24*60*60 * (tm->tm_mday - 1);
	t += 24*60*60 * (367*mon/12);
	t += 24*60*60 * (year/4 - year/100 + year/400);
	t += 24*60*60 * (365*year - 719498L);
	return t;
}
