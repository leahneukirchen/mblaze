#include <stdint.h>

struct message;

struct message *blaze822(char *file);
void blaze822_free(struct message *mesg);
char *blaze822_hdr_(struct message *mesg, const char *hdr, size_t len);
#define blaze822_hdr(mesg, hdr) blaze822_hdr_(mesg, "\0" hdr ":", 2+strlen((hdr)))
int blaze822_body(struct message *mesg, char *file);

int blaze822_loop(int, char **, void (*)(char *));

time_t blaze822_date(char *);
char *blaze822_addr(char *, char **, char **);


int blaze822_decode_rfc2047(char *, char *, size_t, char *);

