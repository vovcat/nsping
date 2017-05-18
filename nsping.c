/*
 * Nameservice "Ping" - 1997 Thomas H. Ptacek
 *
 * Measure reachability of DNS servers and latency of DNS transactions by sending
 * random DNS queries and measuring response time.
 */

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#define dprintf _dprintf
#define MAX_ID	65536

#include "nsping.h"

/* store state on sent queries */

struct nsq {
	int id;
	int found;
	struct timeval sent;
} Queries[QUERY_BACKLOG];

/* like BSD ping, this is signal-driven, so we wind up communicating way
 * too much stuff through globals. Sorry.
 */

#if 0
u_int32_t Target_Address 	= INADDR_NONE;
#endif
struct in_addr 		sin_addr; /* XXX = INADDR_NONE; */
struct in6_addr 	sin6_addr; /* XXX = ; IN6ADDR_ANY_INIT */
#if 0
u_int16_t Target_Port 	= DNS_PORT;
#endif
char *Target_Port	= NULL;
char addr_string[255];
struct addrinfo		*ainfo;
char *Zone 		= NULL;
char *Hostname 		= NULL;

int Max_Sends 		= 0;
int Type 			= T_A;
int Recurse 		=1;

int Sockfd 		= -1;
int Missed 		= 0;
int Lagged 		= 0;
int Count 			= 0;
int Sent 			= 0;
double Ave 		= 0.0;
double Max 		= 0.0;
double Min 		= 0.0;

int Debug = 0;

char *type_int2string(int type);
int type_string2int(char *string);

/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t strlcpy(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return s - src - 1;	/* count does not include NUL */
}

/* -------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
	struct timeval *tvp;
	struct itimerval itv;
	u_int32_t address = INADDR_ANY;
	char Local_Port[6];
	char *timearg = NULL;
	char c;
	int i;

	for (i = 0; i < QUERY_BACKLOG; i++) {
		Queries[i].id = -1;
		Queries[i].found = 1;
	}

	Target_Port = xstrdup(DNS_PORT);
	/* XXX check for result */
	snprintf(Local_Port, sizeof(Local_Port), "%d", getpid() + 1024);

#define OPTS "z:h:t:p:dP:a:c:T:rR"

	while ((c = getopt(argc, argv, OPTS)) != EOF) {
		switch(c) {
		case 'c':
			Max_Sends = atoi(optarg);
			break;

		case 'd':
			Debug = 1;
			break;

		case 'z':
			Zone = xstrdup(optarg);
			break;

		case 'h':
			Hostname = xstrdup(optarg);
			break;

		case 'T':
			Type = type_string2int(optarg);
			if (Type == T_NULL)
				Type = atoi(optarg);
			break;

		case 'r':
			Recurse = 1;
			break;

		case 'R':
			Recurse = 0;
			break;

		case 't':
			timearg = optarg;
			break;

		case 'p':
			Target_Port = xstrdup(optarg);
			break;

		case 'P':
			memset(Local_Port, 0, sizeof(Local_Port));
			strncpy(Local_Port, optarg, sizeof(Local_Port)-1);
			break;

		case 'a':
#if 0
			address = resolve(optarg, port);
#endif
			if (address == INADDR_NONE) {
				fprintf(stderr, "Unable to resolve local address.\n");
				exit(1);
			}
			break;

		default:
			usage();
			exit(1);
		}
	}

	argc -= optind;
	argv += optind;

	if (!*argv) {
		usage();
		exit(1);
	}

	if ((ainfo = resolve(*argv, Target_Port)) == 0) {
		fprintf(stderr, "Unable to resolve target server address.\n");
		fprintf(stderr, "Fatal error, exiting.\n");
		exit(1);
	}

	if (!Hostname && !Zone && !guess_zone()) {
	       	fprintf(stderr, "Unable to determine local DNS zone.\n");
		fprintf(stderr, "Fatal error, exiting.\n");
	       	exit(1);
	}

	if ((Sockfd = bind_udp_socket(Local_Port)) < 0) {
		fprintf(stderr, "Fatal error, exiting.\n");
		exit(1);
	}

	if (!(tvp = set_timer(timearg))) {
	      	fprintf(stderr, "Fatal error, exiting.\n");
		exit(1);
	}

	memcpy(&itv.it_interval, tvp, sizeof(*tvp));
	memcpy(&itv.it_value, tvp, sizeof(*tvp));

	signal(SIGINT, summarize);
	signal(SIGALRM, probe);
	setitimer(ITIMER_REAL, &itv, NULL);

	/* start the fun */

	printf("NSPING %s (%s): %s = \"%s\", Type = \"IN %s\"\n",
			*argv, addr_string,
			Hostname ? "Hostname" : "Domain",
			Hostname ? Hostname : Zone,
			type_int2string(Type));

	probe(0);
       	handle_incoming();

	/* should never get here */

	fprintf(stderr, "Fatal error, exiting.\n");
	exit(1);
}

/* -------------------------------------------------------------------------- */

/* If we can't ascertain the zone to query in from the information we get on
 * the command line, try to get it from our local host name.
 */

int guess_zone()
{
	char lhn[MAXDNAME];
	struct hostent *hp;
	char *cp;

	if (gethostname(lhn, MAXDNAME) < 0)
		return 0;
	if ((hp = gethostbyname(lhn)) == NULL)
		return 0;
	strlcpy(lhn, hp->h_name, sizeof(lhn));

	cp = strchr(lhn, '.');
	if (!cp || !(*(++cp)))
		return 0;

	Zone = xstrdup(cp);
	return 1;
}

/* -------------------------------------------------------------------------- */

/* parse the timeout (really interval) string we're given on the command line */

struct timeval *set_timer(char *timearg)
{
	static struct timeval tv;
	char *cp;

	memset(&tv, 0, sizeof(tv));

	/* 1 second interval */

	if (!timearg) {
		tv.tv_sec = DEFAULT_SECOND_INTERVAL;
		tv.tv_usec = DEFAULT_USECOND_INTERVAL;
		return &tv;
	}

	if (!(cp = strchr(timearg, '.'))) {
		tv.tv_sec = atoi(timearg);
		return &tv;
	}

	*cp++ = '\0';

	/* get the seconds */

	if (*timearg)
		tv.tv_sec = atoi(timearg);

	/* figure out how many usec the user meant; everything on the RHS of the
	 * decimal is a fraction of a second
	 */

	if (*cp) {
		int ss = 0;
		int m = 100000;
		int i = 0;

		for (; *cp && i < 6; cp++, i++) {
			ss += (*cp - '0') * m;
			m /= 10;
		}

		tv.tv_usec = ss;
	}

	return &tv;
}

/* -------------------------------------------------------------------------- */

/* send the DNS queries; this is called as the SIGALRM handler. */

void probe(int sig)
{
	static int Start = 0;
	static int Pos = 0;

	int l = sig;
	u_char *qp;

	signal(SIGALRM, probe);

	if (!Start) {
		Start = getpid() % MAX_ID;
		dprintf("Start = %d\n", Start);
	}

	/* we're overwriting state from a query we never got a response
	 * to, so at least note that we missed it.
	 */

	if (!Queries[Pos].found)
		Missed++;

	/* get the DNS request */

	dprintf("sending with id = %d\n", (Start + Sent) % MAX_ID);
	l = dns_packet(&qp, (Start + Sent) % MAX_ID);

	do {
		if (sendto(Sockfd, qp, l, 0,
			(struct sockaddr *)ainfo->ai_addr,
			ainfo->ai_addrlen) < 0) {

			if (errno != EINTR) {
				perror("sendto");
				return;
			}
		}
	} while (errno == EINTR);

	/* if it was sent successfully, update state */

	Queries[Pos].id = (Start + Sent) % MAX_ID;
	gettimeofday(&Queries[Pos].sent, NULL);
	Queries[Pos].found = 0;

	Sent += 1;
	if (Max_Sends && Sent > Max_Sends)
		summarize(0);

	if (++Pos == QUERY_BACKLOG)
		Pos = 0;
}

/* -------------------------------------------------------------------------- */

/* create a DNS query for the probe */

int dns_packet(u_char **qp, int id)
{
	HEADER *hp;
	u_char *qqp;
	char hname[MAXDNAME];
	char *name;
	int l;

	if (Hostname) {
		/* single static piece of data */
		name = Hostname;
	} else {
		/* random queries (avoid caching) */
		static int seed = 0;
		if (!seed) seed = getpid() ^ time(0);
		snprintf(hname, MAXDNAME, "%ld.%s", random(), Zone);
		name = hname;
	}
	dprintf("using name %s\n",name);

	/* build the thing */
	l = dns_query(name, Type, Recurse, &qqp);
	*qp = qqp;

	/* fix the ID */
	hp = (HEADER *) qqp;
	hp->id = htons(id);

	/* return the length */
	return l;
}

/* -------------------------------------------------------------------------- */

/* deal with incoming DNS response packets */

void handle_incoming()
{
	u_char buffer[1024];
#if 0
	struct sockaddr_in si;
#endif
	struct sockaddr_storage si;
	int sil = sizeof(si);
	int l;

	for (;;) {
		do {
			if ((l = recvfrom(Sockfd, buffer, 1024, 0,
				(struct sockaddr *)&si, &sil)) < 0) {
				if (errno != EINTR) {
					perror("recvfrom");
					continue;
				}
			}
		} while (errno == EINTR);

		/* descriminate real responses from spurious crud */
#if 0
		if (si.sin_addr.s_addr != Target_Address) {
			dprintf("Received packet from unexpected address %s.\n",
				inet_ntoa(si.sin_addr));
			continue;
		}

		if (si.sin_port != htons(Target_Port)) {
			dprintf("Received packet from unexpected port %d.\n",
				ntohs(si.sin_port));
			continue;
		}
#endif

		if (l < sizeof(HEADER)) {
			dprintf("Short packet.\n");
			continue;
		}

		/* track the response */
		update(buffer, l);
	}
}

/* -------------------------------------------------------------------------- */

/* figure out if this is one of our queries, figure out how long it took, and update
 * latency stats.
 */

void update(u_char *bp, int l)
{
	static int Start = 0;
	static int Stuck = 0;

	HEADER *hp = (HEADER *) bp;
	struct timeval tv;
	int i;
	int delta;
	double triptime;

	if (!Start)
		Start = getpid() % MAX_ID;

	gettimeofday(&tv, NULL);

	/* see if it's one of ours... */

	for (i = 0; i < QUERY_BACKLOG; i++)
		if (ntohs(hp->id) == Queries[i].id)
			break;

	if (i == QUERY_BACKLOG) {
		dprintf("Packet with id %d not ours.\n", ntohs(hp->id));
		return;
	} else
		Queries[i].found = 1;

	/* figure out which query this was, using the DNS query ID */
	dprintf("received with id = %d\n", ntohs(hp->id));
	delta = ntohs(hp->id) - Start;
	dprintf("delta = %d - %d = %d\n", ntohs(hp->id), Start, delta);

	/* figure out how long it took */

	triptime = trip_time(&Queries[i].sent, &tv);

	/* update Ave/Max/Min */

	if (triptime > Max)
		Max = triptime;

	if (!Count || triptime < Min)
		Min = triptime;

	Count++;

	/* This is wacky. The intent is to avoid skewing the average with
	 * anomalous samples (dropped packets, etc), and also to get rid
	 * of outlying result from the first sample, which is going to be
	 * abnormally large due to caching (if we're not using random
	 * queries).
	 */

	if (!Ave) {
		Ave = triptime;
	} else {
		double n;

		/* Lose the highest sample after 10 queries */

		if (delta == 10 && Stuck != 2) {
			Ave = ((Ave * 10) - Max) / 9;
			Count--;
			Stuck++;
		}

		/* discard queries that are twice as large as the
		 * average - assume these to be anomalies caused
		 * by network instability
		 */

		if (delta > 10 && triptime > (Ave * 2)) {
			Count--;
			Lagged++;
		} else {
			n = (double) Ave * (Count - 1);
			n += triptime;

			Ave = n / Count;
		}
	}

	printf("%s [ %3d ] %5d bytes from %s: %8.3f ms [ %8.3f san-avg ]\n",
	       hp->rcode == NOERROR ? "+" : "-",
	       delta,
	       l,
	       addr_string,
	       triptime,
	       delta ? Ave : 0.0);
}

/* -------------------------------------------------------------------------- */
/* print the final results */

void summarize(int sig)
{
	printf("\n"
	       "Total Sent: [ %3d ] Total Received: [ %3d ] Missed: [ %3d ] Lagged [ %3d ]\n"
	       "Ave/Max/Min: %8.3f / %8.3f / %8.3f\n",
	       Sent, Count, Missed ? Missed : Sent - Count, Lagged, Ave, Max, Min);

#if 0
	freeaddrinfo();
#endif
	exit(0);
}

/* -------------------------------------------------------------------------- */
/* wrap timeval_subtract so it returns an answer in milliseconds */

double trip_time(struct timeval *send_time, struct timeval *rcv)
{
	struct timeval *tvp;
	double ttime;

	tvp = timeval_subtract(rcv, send_time);

	ttime  = ((double)tvp->tv_sec) * 1000.0 +
		((double)tvp->tv_usec) / 1000.0;

	return ttime;
}

/* -------------------------------------------------------------------------- */
/* return a timeval struct representing the difference between "out" and "in" */

struct timeval *timeval_subtract(struct timeval *out, struct timeval *in)
{
	static struct timeval tm;
	long diff;

	diff = out->tv_usec - in->tv_usec;

	if (diff < 0) {
		diff = diff + 1000000;
		out->tv_sec = out->tv_sec - 1;
	}

	tm.tv_usec = diff;
	diff = out->tv_sec - in->tv_sec;
	tm.tv_sec = diff;

	return &tm;
}

/* -------------------------------------------------------------------------- */
/* map integer type codes to names, v/vrsa. Add new types here if you must. */

struct type2str {
	char *name;
	int type;
} Typetable[] = {
	{ "A", 		T_A 		},
	{ "NS", 	T_NS 		},
	{ "CNAME", 	T_CNAME 	},
	{ "SOA", 	T_SOA 		},
	{ "NULL", 	T_NULL		},
	{ "HINFO", 	T_HINFO		},
	{ "MX", 	T_MX		},
	{ "TXT", 	T_TXT		},
	{ NULL, 	-1		},
};

char *type_int2string(int type)
{
	struct type2str *ts = Typetable;
	int i;

	for (i = 0; ts[i].name; i++)
		if (ts[i].type == type)
			return ts[i].name;

	return "unknown";
}

int type_string2int(char *string)
{
	struct type2str *ts = Typetable;
	int i;

	for (i = 0; ts[i].name; i++)
		if (!strcasecmp(string, ts[i].name))
			return ts[i].type;

	return T_NULL;
}

/* -------------------------------------------------------------------------- */
/* don't print if we're not in debug mode */

void dprintf(char *fmt, ...)
{
	va_list ap;
	if (!Debug) return;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}


/* return a bound UDP socket */

int bind_udp_socket(char *port)
{
	int sockfd;

	struct sockaddr_storage sss;
	struct in6_addr anyaddr = IN6ADDR_ANY_INIT;
	socklen_t               addrlen;

	sockfd = socket(ainfo->ai_family, ainfo->ai_socktype,
	                ainfo->ai_protocol);
	if (sockfd < 0) {
		perror("socket");
		return -1;
	}

	memset(&sss, 0, sizeof(sss));
	switch (ainfo->ai_family) {
	    case AF_INET:
		(((struct sockaddr_in *)(&sss))->sin_addr).s_addr = INADDR_ANY;
		((struct sockaddr_in *)(&sss))->sin_port = htons(atoi(port));
		((struct sockaddr_in *)(&sss))->sin_family = AF_INET;
		addrlen = sizeof(struct sockaddr_in);
		break;

	    case AF_INET6:
		((struct sockaddr_in6 *)(&sss))->sin6_addr = anyaddr;
		((struct sockaddr_in6 *)(&sss))->sin6_port = htons(atoi(port));
		((struct sockaddr_in6 *)(&sss))->sin6_family = AF_INET6;
		addrlen = sizeof(struct sockaddr_in6);
		break;
	}

	if (bind(sockfd, (struct sockaddr *)&sss, addrlen) < 0) {
		perror("bind");
		return -1;
	}

	return sockfd;
}

/* -------------------------------------------------------------------------- */
/* wrap hostname resolution */

struct addrinfo* resolve(char *name, char *port)
{
	struct addrinfo hints, *res, *res0;
	int error;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
        error = getaddrinfo(name, port, &hints, &res0);
	if (error) {
            errx(1, "%s", gai_strerror(error));
            return NULL;
	}

	res = res0;
	switch (res->ai_family) {
	    case AF_INET:
		inet_ntop(res->ai_family,
			&(((struct sockaddr_in *)(res->ai_addr))->sin_addr),
			addr_string, sizeof(addr_string));
		break;
	    case AF_INET6:
		inet_ntop(res->ai_family,
			&(((struct sockaddr_in6 *)(res->ai_addr))->sin6_addr),
			addr_string, sizeof(addr_string));
		break;
	    default:
		return NULL;
		break;
	}

	return res;
}


/* don't ever return NULL */

char *xstrdup(char *v)
{
	char *c = strdup(v);
	assert(c);
	return c;
}

/* -------------------------------------------------------------------------- */

void usage()
{
	fprintf(stderr, "Usage: nsping [-dR] [-c count] [-z zone | -h hostname] [-t timeout] [-p dport] [-P sport] [-a saddr] [-T querytype]\n");
}
