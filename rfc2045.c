#include <sys/types.h>

#include <strings.h>
#include <string.h>

#include "blaze822.h"
#include "blaze822_priv.h"

int
blaze822_check_mime(struct message *msg)
{
        char *v = blaze822_hdr(msg, "mime-version");
	return (v &&
	    v[0] && v[0] == '1' &&
	    v[1] && v[1] == '.' &&
	    v[2] && v[2] == '0' &&
	    (!v[3] || iswsp(v[3])));
}

int
blaze822_mime_body(struct message *msg,
    char **cto, char **bodyo, size_t *bodyleno, char **bodychunko)
{
	if (!msg->body || !msg->bodyend) {
		*bodyo = 0;
		*bodyleno = 0;
		*bodychunko = 0;
		return 0;
        }

	char *ct = blaze822_hdr(msg, "content-type");
	char *cte = blaze822_hdr(msg, "content-transfer-encoding");

	if (!ct)
		ct = "text/plain; charset=US-ASCII";

	char *s = ct;
	while (*s && *s != ';') {
		*s = lc(*s);
		s++;
	}

	*cto = ct;

	if (cte) {
		if (strncasecmp(cte, "quoted-printable", 16) == 0) {
			blaze822_decode_qp(msg->body, msg->bodyend, bodyo, bodyleno);
			*bodychunko = *bodyo;
		} else if (strncasecmp(cte, "base64", 6) == 0) {
			blaze822_decode_b64(msg->body, msg->bodyend, bodyo, bodyleno);
			*bodychunko = *bodyo;
		} else {
			cte = 0;
		}
	}
	if (!cte) {
		*bodyo = msg->body;
		*bodyleno = msg->bodyend - msg->body;
		*bodychunko = 0;
	}

	return 1;
}

int
blaze822_mime_parameter(char *s, char *name, char **starto, char **stopo)
{
	if (!s)
		return 0;
	s = strchr(s, ';');
	if (!s)
		return 0;
	s++;

	size_t namelen = strlen(name);

	while (*s) {
		while (iswsp(*s))
			s++;
		if (strncasecmp(s, name, namelen) == 0 && s[namelen] == '=') {
			s += namelen + 1;
			break;
		}
		s = strchr(s+1, ';');
		if (!s)
			return 0;
		s++;
	}
	if (!s || !*s)
		return 0;
	char *e;
	if (*s == '"') {
		s++;
		e = strchr(s, '"');
		if (!e)
			return 0;
	} else {
		e = s;
		while (*e && !iswsp(*e) && *e != ';')
			e++;
	}

	*starto = s;
	*stopo = e;
	return 1;
}

int
blaze822_multipart(struct message *msg, struct message **imsg)
{
	char *s = blaze822_hdr(msg, "content-type");
	if (!s)
		return 0;
	while (*s && *s != ';')
		s++;
	if (!*s)
		return 0;

	char *boundary, *boundaryend;
	if (!blaze822_mime_parameter(s, "boundary", &boundary, &boundaryend))
		return 0;
	char mboundary[256];
	int boundarylen = boundaryend-boundary+2;

	if (boundarylen >= 256)
		return 0;
	mboundary[0] = '-';
	mboundary[1] = '-';
	memcpy(mboundary+2, boundary, boundarylen-2);
	mboundary[boundarylen] = 0;

	char *prevpart;
	if (*imsg)
		prevpart = (*imsg)->bodyend;
	else
		prevpart = msg->body;

	char *part = mymemmem(prevpart, msg->bodyend - prevpart, mboundary, boundarylen);
	if (!part)
		return 0;

	part += boundarylen;
	if (*part == '\r')
		part++;
	if (*part == '\n')
		part++;
	else if (*part == '-' && part < msg->bodyend && *(part+1) == '-')
		return 0;
	else
		return 0;   // XXX error condition?

	char *nextpart = mymemmem(part, msg->bodyend - part, mboundary, boundarylen);
	if (!nextpart)
		return 0;   // XXX error condition

	*imsg = blaze822_mem(part, nextpart-part);

	return 1;
}
