
/*
 * Nameservice "Ping" - 1997 Thomas H. Ptacek
 *
 * Measure reachability of DNS servers and latency of DNS transactions by sending
 * random DNS queries and measuring response time.
 */

#ifdef sys5
int snprintf(char *, int, char *, ...);
#endif

#include "nsping.h"
#include <stdarg.h>
#include <assert.h>

/* store state on sent queries */

struct nsq {
	int id;
	int found;
	struct timeval sent;
} Queries[QUERY_BACKLOG];

/* like BSD ping, this is signal-driven, so we wind up communicating way
 * too much stuff through globals. Sorry.
 */

u_int32_t Target_Address 	= INADDR_NONE;
u_int16_t Target_Port 	= DNS_PORT;
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

/* -------------------------------------------------------------------------- */

int main(int argc, char **argv) {
	struct timeval *tvp;
	struct itimerval itv;
	u_int32_t address = INADDR_ANY;
	u_int32_t port = getpid() + 1024;
	char *timearg = NULL;
	char c;
	int i;

	for(i = 0; i < QUERY_BACKLOG; i++) {
		Queries[i].id = -1;
		Queries[i].found = 1;
	}

#define OPTS "z:h:t:p:dP:a:c:T:rR"

	while((c = getopt(argc, argv, OPTS)) != EOF) {
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
			if(Type == T_NULL)
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
			Target_Port = atoi(optarg);
			break;

		case 'P':
			port = atoi(optarg);
			break;

		case 'a':
			address = resolve(optarg);
			if(address == INADDR_NONE) {
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

	if(!*argv) {
		usage();
		exit(1);
	}

	if((Target_Address = resolve(*argv)) == INADDR_NONE) {
		fprintf(stderr, "Unable to resolve target server address.\n");
		fprintf(stderr, "Fatal error, exiting.\n");
		exit(1);
	}

	if(!Hostname && !Zone && !guess_zone()) {
	       	fprintf(stderr, "Unable to determine local DNS zone.\n");
		fprintf(stderr, "Fatal error, exiting.\n");
	       	exit(1);
	}

	if((Sockfd = bind_udp_socket(address, port)) < 0) {
		fprintf(stderr, "Fatal error, exiting.\n");
		exit(1);
	}

      	if(!(tvp = set_timer(timearg))) {
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
			*argv, addr_string(Target_Address), 
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

/* If we can't ascertain the zone to query in from the information we get on the 
 * command line, try to get it from our local host name.
 */

int guess_zone() {
	char lhn[MAXDNAME];
	char *cp;

	if(gethostname(lhn, MAXDNAME) < 0) 
		return(0);

	cp = strchr(lhn, '.');
	if(!cp || !(*(++cp)))
		return(0);

	Zone = xstrdup(cp);

	return(1);
}

/* -------------------------------------------------------------------------- */

/* parse the timeout (really interval) string we're given on the command line */

struct timeval *set_timer(char *timearg) {
	static struct timeval tv;
	char *cp;

	memset(&tv, 0, sizeof(tv));

	/* 1 second interval */

	if(!timearg) {
		tv.tv_sec = DEFAULT_SECOND_INTERVAL;
		tv.tv_usec = DEFAULT_USECOND_INTERVAL;
		return(&tv);
	}

	if(!(cp = strchr(timearg, '.'))) {
		tv.tv_sec = atoi(timearg);
		return(&tv);
	}

	*cp++ = '\0';
	
	/* get the seconds */

	if(*timearg) 
		tv.tv_sec = atoi(timearg);

	/* figure out how many usec the user meant; everything on the RHS of the
	 * decimal is a fraction of a second 
	 */

	if(*cp) {
		int ss = 0;
		int m = 100000;
		int i = 0;

		for(; *cp && i < 6; cp++, i++) {
			ss += (*cp - '0') * m;
			m /= 10;
		}

		tv.tv_usec = ss;
	}

	return(&tv);			
}

/* -------------------------------------------------------------------------- */

/* send the DNS queries; this is called as the SIGALRM handler. */

void probe(int sig) {
	static int Start = 0;
	static int Pos    = 0;      

	struct sockaddr_in si;
	int l;
	int id;
	u_char *qp;

	signal(SIGALRM, probe);

	if(!Start)
		Start = getpid();

	/* we're overwriting state from a query we never got a response
	 * to, so at least note that we missed it.
	 */

	if(!Queries[Pos].found)
		Missed++;

	memset(&si, 0, sizeof(si));
	si.sin_addr.s_addr = Target_Address;
	si.sin_port = htons(Target_Port);
	si.sin_family = AF_INET;

	/* get the DNS request */

	l = dns_packet(&qp, Start + Sent);

	do {
		if(sendto(Sockfd, qp, l, 0, 
			(struct sockaddr *)&si, sizeof(si)) < 0) {
			if(errno != EINTR) {		
				perror("sendto");
				return;
			}
		}
	} while(errno == EINTR);

	/* if it was sent successfully, update state */

	Queries[Pos].id = Start + Sent;
	gettimeofday(&Queries[Pos].sent, NULL);
	Queries[Pos].found = 0;

	Sent += 1;
	if(Max_Sends && Sent > Max_Sends) 
		summarize(0);

	if(++Pos == QUERY_BACKLOG) 
		Pos = 0;

	return;	
}

/* -------------------------------------------------------------------------- */

/* create a DNS query for the probe */

int dns_packet(u_char **qp, int id) {
	HEADER *hp;
	u_char *qqp;
	char hname[MAXDNAME];
	char *name;
	int l;

	if(Hostname) 
		/* single static piece of data */

		name = Hostname;
	else {
		/* random queries (avoid caching) */

		static int seed = 0;
		
		if(!seed) 
			seed = getpid() ^ time(0);
		
		snprintf(hname, MAXDNAME, "%d.%s", random(), Zone);
		name = hname;
	}
	
	/* build the thing */

	l = dns_query(name, Type, Recurse, &qqp);
	*qp = qqp;
	
	/* fix the ID */

	hp = (HEADER *) qqp;
	hp->id = htons(id);

	/* return the length */

	return(l);
}

/* -------------------------------------------------------------------------- */

/* deal with incoming DNS response packets */

void handle_incoming() {
	u_char buffer[1024];
	struct sockaddr_in si;
	int sil = sizeof(si);
	int l;

	for(;;) {
		do {
			if((l = recvfrom(Sockfd, buffer, 1024, 0, 
				(struct sockaddr *)&si, &sil)) < 0) {
				if(errno != EINTR) {
					perror("recvfrom");
					continue;
				}
			}
		} while(errno == EINTR);

		/* descriminate real responses from spurious crud */

		if(si.sin_addr.s_addr != Target_Address) {
			dprintf("Received packet from unexpected address %s.\n",
				inet_ntoa(si.sin_addr));
			continue;
		}

		if(si.sin_port != htons(Target_Port)) {
			dprintf("Received packet from unexpected port %d.\n",
				ntohs(si.sin_port));
			continue;
		}

		if(l < sizeof(HEADER)) {
			dprintf("Short packet.\n");
			continue;
		}
		
		/* track the response */

		update(buffer, l);
	}

	return;
}

/* -------------------------------------------------------------------------- */

/* figure out if this is one of our queries, figure out how long it took, and update
 * latency stats.
 */

void update(u_char *bp, int l) {
	static int Start = 0;
	static int Stuck = 0;

	HEADER *hp = (HEADER *) bp;
	struct timeval tv;
	int i;
	int delta;
	double triptime;

	if(!Start)
		Start = getpid();

	gettimeofday(&tv, NULL);

	/* see if it's one of ours... */
	
	for(i = 0; i < QUERY_BACKLOG; i++) 
		if(ntohs(hp->id) == Queries[i].id)
			break;

	if(i == QUERY_BACKLOG) {
		dprintf("Packet with id %d not ours.\n", ntohs(hp->id));
		return;
	} else 
		Queries[i].found = 1;	

	/* figure out which query this was, using the DNS query ID */

	delta = ntohs(hp->id) - Start;
	
	/* figure out how long it took */

	triptime = trip_time(&Queries[i].sent, &tv);

	/* update Ave/Max/Min */

	if(triptime > Max)
		Max = triptime;
	
	if(!Count || triptime < Min)
		Min = triptime;            

	Count++;

	/* This is wacky. The intent is to avoid skewing the average with
	 * anomalous samples (dropped packets, etc), and also to get rid
	 * of outlying result from the first sample, which is going to be
	 * abnormally large due to caching (if we're not using random
	 * queries).
	 */

	if(!Ave) 
		Ave = triptime;
	else {
		double n;

		/* Lose the highest sample after 10 queries */

		if(delta == 10 && Stuck != 2) {
			Ave = ((Ave * 10) - Max) / 9;
			Count--;
			Stuck++;
		}

		/* discard queries that are twice as large as the 
		 * average - assume these to be anomalies caused
		 * by network instability
		 */

		if(delta > 10 && triptime > (Ave * 2)) {
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
	       addr_string(Target_Address),
	       triptime,
	       delta ? Ave : 0.0);

	return;
}

/* -------------------------------------------------------------------------- */

/* print the final results */

void summarize(int sig) {
	printf(
	       "\n"
	       "Total Sent: [ %3d ] Total Received: [ %3d ] Missed: [ %3d ] Lagged [ %3d ]\n"
	       "Ave/Max/Min: %8.3f / %8.3f / %8.3f\n",
	       Sent, Count, Missed ? Missed : Sent - Count, Lagged, Ave, Max, Min);

	exit(0);
}

/* -------------------------------------------------------------------------- */

/* wrap timeval_subtract so it returns an answer in milliseconds */

double trip_time(struct timeval *send_time, struct timeval *rcv) {
	struct timeval tv, *tvp;
	double ttime;

	tvp = timeval_subtract(rcv, send_time);
        
	ttime  = ((double)tvp->tv_sec) * 1000.0 +
		((double)tvp->tv_usec) / 1000.0;
        
	return(ttime);
}

/* -------------------------------------------------------------------------- */

/* return a timeval struct representing the difference between "out" and "in" */

struct timeval *timeval_subtract(struct timeval *out, struct timeval *in) {
	static struct timeval tm;       
	long diff;

	diff = out->tv_usec - in->tv_usec;
        
	if(diff < 0) {
		diff = diff + 1000000;
		out->tv_sec = out->tv_sec - 1;
	}
        
	tm.tv_usec = diff;
	diff = out->tv_sec - in->tv_sec;
	tm.tv_sec = diff;

	return(&tm);
}

/* -------------------------------------------------------------------------- */

/* binary address -> dotted quad string */

char *addr_string(u_int32_t address) {
	static char as[20];
	u_char *cp = (u_char *) &address;

	sprintf(as, "%d.%d.%d.%d", cp[0], cp[1], cp[2], cp[3]);
	return(as);
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

char *type_int2string(int type) {
	struct type2str *ts = Typetable;
	int i;
	
	for(i = 0; ts[i].name; i++) 
		if(ts[i].type == type)
			return(ts[i].name);

	return("unknown");
}

int type_string2int(char *string) {
	struct type2str *ts = Typetable;
	int i;

	for(i = 0; ts[i].name; i++)
		if(!strcasecmp(string, ts[i].name))
			return(ts[i].type);

	return(T_NULL);
}

/* -------------------------------------------------------------------------- */

/* don't print if we're not in debug mode */

void dprintf(char *fmt, ...) {
	va_list ap;

	if(!Debug)
		return;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);

	return;
}


/* return a bound UDP socket */

int bind_udp_socket(u_int32_t address, u_int16_t port) {
	struct sockaddr_in si;
	int sockfd;
	
	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sockfd < 0) {
		perror("socket");
		return(-1);
	}

	memset(&si, 0, sizeof(si));
	si.sin_addr.s_addr = address;
	si.sin_port = htons(port);
	si.sin_family = AF_INET;

	if(bind(sockfd, (struct sockaddr *)&si, sizeof(si)) < 0) {
		perror("bind");
		return(-1);
	}

	return(sockfd);
}

/* -------------------------------------------------------------------------- */

/* wrap hostname resolution */

u_int32_t resolve(char *name) {
	u_long addr;

	addr = inet_addr(name);
	if(addr == INADDR_NONE) {
		struct hostent *hp = gethostbyname(name);
		if(!hp)
			return(INADDR_NONE);

		memcpy(&addr, hp->h_addr, 4);
	}

	return(addr);
}


/* don't ever return NULL */

char *xstrdup(char *v) {
	char *c = strdup(v);
	assert(c);
	return(c);
}

/* -------------------------------------------------------------------------- */

 void usage() {
	 fprintf(stderr, "nsping [ -z <zone> | -h <hostname> ] -p <port> -t <timeout>\n"
		   "\t\t-a <local address> -P <local port>\n"
		   "\t\t-T <type> <-r | -R, recurse?>\n");
	 return;
 }

#ifdef sys5
#warning "YOUR OPERATING SYSTEM SUCKS."

int snprintf(char *str, int count, char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	return(vsprintf(str, fmt, ap));
}

#endif

