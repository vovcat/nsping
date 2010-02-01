#ifndef DNS_LIB_INCLUDED
#define DNS_LIB_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

int dns_isnxdomain(u_char *, int);
int dns_query(char *, int, int, u_char **);
int dns_req_ptr(char *, u_char **, int);
int dns_req_a(char *, u_char **, int);
int dns_req_cname(char *, u_char **, int);
int dns_req_ns(char *, u_char **, int);
int dns_req_mx(char *, u_char **, int);
int dns_req_soa(char *, u_char **, int);

#endif

