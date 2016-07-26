struct message {
	char *msg;
	char *end;
	char *body;
	char *bodyend;
	char *bodychunk;
};

//          WSP            =  SP / HTAB
#define iswsp(c)  (((c) == ' ' || (c) == '\t'))

#define isfws(c)  (((unsigned char)(c) == ' ' || (unsigned char)(c) == '\t' || (unsigned char)(c) == '\n' || (unsigned char)(c) == '\r'))

// ASCII lowercase without alpha check (wrong for "@[\]^_")
#define lc(c) ((c) | 0x20)

void *mymemmem(const void *h0, size_t k, const void *n0, size_t l);
