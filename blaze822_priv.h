struct message {
	char *msg;
	char *end;
	char *body;
	char *bodyend;
	char *bodychunk;
};

#define iswsp(c)  (((c) == ' ' || (c) == '\t'))
