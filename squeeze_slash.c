void
squeeze_slash(char *arg) {
	char *s, *t;

	// squeeze slashes
	s = t = arg;
	while ((*s++ = *t))
		while (*t++ == '/' && *t == '/')
			;

	// remove trailing slashes
	s--;
	while (*--s == '/')
		*s = 0;
}
