#include <sys/types.h>

#include <stdint.h>
#include <time.h>
#include <limits.h>

#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif

// maildir filename suffix: use a semicolon on winnt/ntfs, otherwise a colon
#ifdef __winnt__
#define MAILDIR_COLON                ';'
#define MAILDIR_COLON_SPEC_VER       ";2"
#define MAILDIR_COLON_SPEC_VER_COMMA ";2,"
#else
#define MAILDIR_COLON                ':'
#define MAILDIR_COLON_SPEC_VER       ":2"
#define MAILDIR_COLON_SPEC_VER_COMMA ":2,"
#endif

struct message;

// blaze822.c

struct message *blaze822(char *file);       // just header
struct message *blaze822_file(char *file);  // header + body (read(2))
struct message *blaze822_mem(char *buf, size_t len);  // header + body

char *blaze822_hdr_(struct message *mesg, const char *hdr, size_t len);
#define blaze822_hdr(mesg, hdr) blaze822_hdr_(mesg, "\0" hdr ":", 2+strlen((hdr)))
char *blaze822_chdr(struct message *mesg, const char *chdr);

char *blaze822_next_header(struct message *mesg, char *prev);

void blaze822_free(struct message *mesg);

time_t blaze822_date(char *);
char *blaze822_addr(char *, char **, char **);
char *blaze822_body(struct message *mesg);
size_t blaze822_bodylen(struct message *mesg);
size_t blaze822_headerlen(struct message *mesg);

char *blaze822_orig_header(struct message *mesg);

// rfc2047.c

int blaze822_decode_rfc2047(char *, char *, size_t, char *);
int blaze822_decode_qp(char *start, char *stop, char **deco, size_t *decleno, int underscore);
int blaze822_decode_b64(char *start, char *stop, char **deco, size_t *decleno);

// rfc2045.c

int blaze822_check_mime(struct message *msg);
int blaze822_mime_body(struct message *msg, char **cto, char **bodyo, size_t *bodyleno, char **bodychunko);
int blaze822_multipart(struct message *msg, struct message **imsg);
int blaze822_mime_parameter(char *s, char *name, char **starto, char **stopo);

typedef enum { MIME_CONTINUE, MIME_STOP, MIME_PRUNE } blaze822_mime_action;
typedef blaze822_mime_action (*blaze822_mime_callback)(int, struct message *, char *, size_t);
blaze822_mime_action blaze822_walk_mime(struct message *, int, blaze822_mime_callback);

// rfc2231.c

int blaze822_mime2231_parameter(char *, char *, char *, size_t, char *);

// seq.c

char *blaze822_seq_open(char *file);
int blaze822_seq_load(char *map);
long blaze822_seq_find(char *ref);


char *blaze822_seq_cur(void);
int blaze822_seq_setcur(char *s);

struct blaze822_seq_iter {
	long lines;
	long cur;
	long start;
	long stop;

	long line;
	char *s;
};

char *blaze822_seq_next(char *map, char *range, struct blaze822_seq_iter *iter);
long blaze822_loop(int, char **, void (*)(char *));
long blaze822_loop1(char *arg, void (*cb)(char *));
char *blaze822_home_file(char *basename);

// filter.c

int filter(char *input, size_t inlen, char *cmd, char **outputo, size_t *outleno);

// mygmtime.c

time_t mytimegm(const struct tm *tm);


// slurp.c

int slurp(char *filename, char **bufo, off_t *leno);

// safe_u8putstr.c

#include <stdio.h>
void safe_u8putstr(char *s0, size_t l, int oneline, FILE *stream);

// pipeto.c

pid_t pipeto(const char *cmdline);
int pipeclose(pid_t pid);

// squeeze_slash.c

void squeeze_slash(char *);
