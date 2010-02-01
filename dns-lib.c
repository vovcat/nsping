/* !!! Lobotomized for public release */

#include <string.h>

#include "dns-lib.h"

/* create a comple C_IN DNS query packet, suitable for output directly from sendto(),
 * returns the length of the packet and the actual packet (static data) via the "cp" arg
 */

int dns_query(char *name, int type, int recurse, u_char **cp) {
	static u_char buffer[BUFSIZ];
	static int id = 0;

	HEADER *h;
	u_char *buf;
	int i;

      	h = (HEADER *) buffer;

	if(!id) {
		id = getpid();

		memset(h, 0, sizeof(h));
	    
		h->rd = recurse ? 1 : 0;

		h->opcode = QUERY;
		h->qdcount = htons(1);
	} else {
		id++;
	}

       	h->id = htons(id);

	buf = buffer + sizeof(HEADER);
	
	i = dns_rr_query(name, type, buf);

	*cp = buffer;
	return(i + sizeof(HEADER));
}
	
