/* 1996 Thomas H. Ptacek, the RDIST organization */

#include "dns-rr.h"

/* --------------------------------------------------------------------- */

/* A DNS query is a resource record with no data (and hence no length specifier).
 * All queries share the same format.
 */

int dns_rr_query_len(char *name, int type, u_char *buf)
{
	return strlen(name) + 1 + 4;
}

int dns_rr_query(char *name, int type, u_char *buf)
{
	int len;

	u_char *dp = buf;

	len = dns_string(name, dp, MAXDNAME);
	if (len < 0)
		return 0;

	dp += len;

	PUTSHORT(type, dp);
	len += 2;

	PUTSHORT(C_IN, dp);
	len += 2;

	return len;
}

/* --------------------------------------------------------------------- */

/* skip over the compressed name in "buf", returning an error if it's badly encoded
 * and runs over the end-of-packet specified by "eop".
 */

u_char *dns_skip(u_char *buf, u_char *eop)
{
	int l;

	/* jump from length byte to length byte */

	for (l = *buf; l; buf += (l + 1)) {
		l = *buf;
		if ((buf + l + 1) > eop)
			return NULL;
	}

	return buf;
}

/* --------------------------------------------------------------------- */

int dns_string(char *string, u_char *buf, int size)
{
	return dn_comp(string, buf, size, NULL, NULL);
}
