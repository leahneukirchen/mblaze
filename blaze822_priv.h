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

// ASCII lowercase/uppercase without alpha check (wrong for "@[\]^_")
#define lc(c) ((c) | 0x20)
#define uc(c) ((c) & 0xdf)

// dirent type that can be a mail/dir (following symlinks)
#if defined(DT_REG) && defined(DT_LNK) && defined(DT_UNKNOWN)
#define MAIL_DT(t) (t == DT_REG || t == DT_LNK || t == DT_UNKNOWN)
#define DIR_DT(t) (t == DT_DIR || t == DT_UNKNOWN)
#else
#define MAIL_DT(t) (1)
#define DIR_DT(t) (1)
#endif

void *mymemmem(const void *h0, size_t k, const void *n0, size_t l);
