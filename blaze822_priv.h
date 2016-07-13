struct message {
	char *msg;
	char *end;
	char *body;
	char *bodyend;
	char *bodychunk;
};

//          WSP            =  SP / HTAB
#define iswsp(c)  (((c) == ' ' || (c) == '\t'))

// ASCII lowercase without alpha check (wrong for "@[\]^_")
#define lc(c) ((c) | 0x20)
