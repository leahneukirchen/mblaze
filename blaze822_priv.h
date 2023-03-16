struct message {
	char *msg;
	char *end;
	char *body;
	char *bodyend;
	char *bodychunk;
	char *orig_header;
};

//          WSP            =  SP / HTAB
#define iswsp(c)  (((c) == ' ' || (c) == '\t'))

#define isfws(c)  (((unsigned char)(c) == ' ' || (unsigned char)(c) == '\t' || (unsigned char)(c) == '\n' || (unsigned char)(c) == '\r'))

// 7bit-ASCII only lowercase/uppercase
#define lc(c) (((unsigned)(c)-'A') < 26 ? ((c) | 0x20) : (c))
#define uc(c) (((unsigned)(c)-'a') < 26 ? ((c) & 0xdf) : (c))

// dirent type that can be a mail/dir (following symlinks)
#if defined(DT_REG) && defined(DT_LNK) && defined(DT_UNKNOWN)
#define MAIL_DT(t) (t == DT_REG || t == DT_LNK || t == DT_UNKNOWN)
#define DIR_DT(t) (t == DT_DIR || t == DT_UNKNOWN)
#else
#define MAIL_DT(t) (1)
#define DIR_DT(t) (1)
#endif

void *mymemmem(const void *h0, size_t k, const void *n0, size_t l);
