
#ifndef NSPING_INCLUDED
#define NSPING_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "dns-lib.h"
#include "dns-rr.h"


#ifdef sys5
typedef unsigned long u_int32_t;
typedef unsigned short u_int16_t;
#define INADDR_NONE -1
#endif

#define QUERY_BACKLOG	1024
#define DNS_PORT		"53"
#define DEFAULT_SECOND_INTERVAL	1
#define DEFAULT_USECOND_INTERVAL	0

int guess_zone(char *dns_server_name);
struct timeval *set_timer(char *timearg);
void probe(int sig);
int dns_packet(u_char **qp, int id);
void handle_incoming(void);
void update(u_char *bp, int l);
void summarize(int);
double trip_time(struct timeval *send_time, struct timeval *rcv);
struct timeval *timeval_subtract(struct timeval *out, struct timeval *in);
int bind_udp_socket(char *port);
void dprintf(char *fmt, ...);
void usage(void);
char *xstrdup(char *v);
struct addrinfo *resolve(char *name, char *port);

#endif
