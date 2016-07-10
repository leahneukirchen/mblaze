#include <string.h>
#include <stdio.h>
#include <stdint.h>

int
main(int argc, char *argv[])
{
	int i;
	unsigned int j;
	char *f, *e;

	int8_t flagmod[255] = { 0 };

	for (i = 1; i < argc; i++) {
		if (*argv[i] == '+') {
			f = argv[i] + 1;
			while (*f)
				flagmod[(int)*f++]++;
			continue;
		} else if (*argv[i] == '-') {
			f = argv[i] + 1;
			while (*f)
				flagmod[(int)*f++]--;
			continue;
		}

		int8_t flags[255] = { 0 };
		char flagstr[255];
		f = e = strstr(argv[i], ":2,");

		if (!f)
			continue;
		f += 3;
		while (*f)
			flags[(int)*f++] = 1;

		*e = 0;
		f = flagstr;
		for (j = 0; j < sizeof flags; j++)
			if (flags[j] + flagmod[j])
				*f++ = j;
		*f = 0;

		printf("%s:2,%s\n", argv[i], flagstr);
	}

	return 0;
}
