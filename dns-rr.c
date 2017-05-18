/* 1996 Thomas H. Ptacek, the RDIST organization */

#include "dns-rr.h"

/* --------------------------------------------------------------------- */

/* A DNS query is a resource record with no data (and hence no length specifier).
 * All queries share the same format.
 */

int dns_rr_query_len(char *name, int type, u_char *buf)
{
	(void)type;
	(void)buf;
	return (int)strlen(name) + 1 + 4;
}

#undef NS_PUT16
#define NS_PUT16(s, cp) do { \
        u_int16_t t_s = (u_int16_t)(s); \
        u_char *t_cp = (u_char *)(cp); \
        *t_cp++ = t_s >> 8 & 255U; \
        *t_cp   = t_s & 255U; \
        (cp) += 2; \
} while (0)

#undef NS_PUT32
#define NS_PUT32(l, cp) do { \
        u_int32_t t_l = (u_int32_t)(l); \
        u_char *t_cp = (u_char *)(cp); \
        *t_cp++ = t_l >> 24 & 255U; \
        *t_cp++ = t_l >> 16 & 255U; \
        *t_cp++ = t_l >> 8 & 255U; \
        *t_cp   = t_l & 255U; \
        (cp) += 4; \
} while (0)

int dns_rr_query(char *name, int type, u_char *buf)
{
	int len;

	u_char *dp = buf;

	len = dns_string(name, dp, MAXDNAME);
	if (len < 0)
		return 0;

	dp += len;

	NS_PUT16(type, dp);
	len += 2;

	NS_PUT16(C_IN, dp);
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
