#define _GNU_SOURCE
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
blaze822_mime_body(struct message *msg, char **cto, char **bodyo, size_t *bodyleno)
{
	if (!msg->body || !msg->bodyend) {
		*bodyo = 0;
		*bodyleno = 0;
		return -1;
        }

	char *ct = blaze822_hdr(msg, "content-type");
	char *cte = blaze822_hdr(msg, "content-transfer-encoding");

	if (!ct)
		ct = "text/plain; charset=US-ASCII";

	char *s = ct;
	while (*s && *s != ';')
		s++;

	*cto = ct;

	if (cte) {
		if (strncasecmp(cte, "quoted-printable", 16) == 0)
			blaze822_decode_qp(msg->body, msg->bodyend, bodyo, bodyleno);
		else if (strncasecmp(cte, "base64", 6) == 0)
			blaze822_decode_b64(msg->body, msg->bodyend, bodyo, bodyleno);
		else
			cte = 0;
	}
	if (!cte) {
		*bodyo = msg->body;
		*bodyleno = msg->bodyend - msg->body;
	}

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

	// XXX scan boundary only once
	char *boundary = s+1;
	while (*boundary) {
		while (iswsp(*boundary))
			boundary++;
		if (strncasecmp(boundary, "boundary=", 9) == 0) {
			boundary += 9;
			break;
		}
		boundary = strchr(boundary+1, ';');
		if (!boundary)
			break;
		boundary++;
	}
	if (!boundary || !*boundary)
		return 0;
	char *e;
	if (*boundary == '"') {
		boundary++;
		e = strchr(boundary, '"');
		if (!e)
			return 0;
	} else {
		e = boundary;
		// XXX    bchars := bcharsnospace / " "
		// bcharsnospace := DIGIT / ALPHA / "'" / "(" / ")" /
		//              "+" / "_" / "," / "-" / "." /
		//              "/" / ":" / "=" / "?"
		while (*e && !iswsp(*e) && *e != ';')
			e++;
		e++;
	}

	char mboundary[256];
	mboundary[0] = '-';
	mboundary[1] = '-';
	memcpy(mboundary+2, boundary, e-boundary);  // XXX overflow
	mboundary[e-boundary+2] = 0;

	int boundarylen = e-boundary+2;

//	printf("boundary: %s %s %s\n", ct, cte, boundary);

	char *prevpart;
	if (*imsg)
		prevpart = (*imsg)->bodyend;
	else
		prevpart = msg->body;

	char *part = memmem(prevpart, msg->bodyend - prevpart, mboundary, boundarylen);
	if (!part)
		return 0;
	/// XXX access to stuff before first boundary?
	part += boundarylen;
	if (*part == '\r')
		part++;
	if (*part == '\n')
		part++;
	else if (*part == '-' && part < msg->bodyend && *(part+1) == '-')
		return 0;
	else
		return 0;   // XXX error condition?

	char *nextpart = memmem(part, msg->bodyend - part, mboundary, boundarylen);
	if (!nextpart)
		return 0;   // XXX error condition

	*imsg = blaze822_mem(part, nextpart-part);

	return 1;
}
