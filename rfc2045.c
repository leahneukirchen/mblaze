#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "blaze822.h"
#include "blaze822_priv.h"

// needs to be writable
char textplain[] = "text/plain; charset=US-ASCII";

int
blaze822_check_mime(struct message *msg)
{
	char *v = blaze822_hdr(msg, "mime-version");
	if (v &&
	    v[0] && v[0] == '1' &&
	    v[1] && v[1] == '.' &&
	    v[2] && v[2] == '0' &&
	    (!v[3] || iswsp(v[3])))
		return 1;
	v = blaze822_hdr(msg, "content-transfer-encoding");
	if (v)
		return 1;
	return 0;
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
		ct = textplain;

	char *s = ct;
	while (*s && *s != ';') {
		*s = lc(*s);
		s++;
	}

	*cto = ct;

	if (cte) {
		if (strncasecmp(cte, "quoted-printable", 16) == 0) {
			blaze822_decode_qp(msg->body, msg->bodyend, bodyo, bodyleno, 0);
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
		if (!*s)
			return 0;
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

// like mymemmem but check the match is followed by \r, \n or -.
static char *
mymemmemnl(const char *h0, size_t k, const char *n0, size_t l)
{
	char *r;

	while (k && (r = mymemmem(h0, k, n0, l))) {
		if (r + l < h0 + k &&   // check if r[l] safe to access
		    (r[l] == '\r' || r[l] == '\n' || r[l] == '-'))
			return r;
		else {
			// skip over this match
			k -= (r - h0) + 1;
			h0 = r + 1;
		}
	}

	return 0;
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
	size_t boundarylen = boundaryend-boundary+2;

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

	char *part = mymemmemnl(prevpart, msg->bodyend - prevpart, mboundary, boundarylen);
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

	char *nextpart = mymemmemnl(part, msg->bodyend - part, mboundary, boundarylen);
	if (!nextpart)
		nextpart = msg->bodyend;  // no boundary found, take all
	else if (nextpart == part)  // invalid empty MIME part
		return 0;   // XXX error condition

	if (*(nextpart-1) == '\n')
		nextpart--;
	if (*(nextpart-1) == '\r')
		nextpart--;

	*imsg = blaze822_mem(part, nextpart-part);

	return 1;
}

blaze822_mime_action
blaze822_walk_mime(struct message *msg, int depth, blaze822_mime_callback visit)
{
	char *ct, *body, *bodychunk;
	size_t bodylen;

	blaze822_mime_action r = MIME_CONTINUE;

	if (depth > 64)
		return MIME_PRUNE;

	if (blaze822_mime_body(msg, &ct, &body, &bodylen, &bodychunk)) {

		r = visit(depth, msg, body, bodylen);

		if (r == MIME_CONTINUE) {
			if (strncmp(ct, "multipart/", 10) == 0) {
				struct message *imsg = 0;
				while (blaze822_multipart(msg, &imsg)) {
					r = blaze822_walk_mime(imsg, depth+1, visit);
					if (r == MIME_STOP)
						break;
				}
			} else if (strncmp(ct, "message/rfc822", 14) == 0) {
				struct message *imsg = blaze822_mem(body, bodylen);
				if (imsg)
					blaze822_walk_mime(imsg, depth+1, visit);
			}
		}

		free(bodychunk);
	}

	return r;
}
