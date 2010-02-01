
#ifndef DNS_RR_INCLUDED
#define DNS_RR_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>

int dns_rr_a(char *, u_long, int, u_char *);
int dns_rr_a_len(char *, u_long, int, u_char *);

int dns_rr_ptr(char *, char *, int, u_char *);
int dns_rr_ptr_len(char *, char *, int, u_char *);

int dns_rr_ns(char *, char *, int, u_char *);
int dns_rr_ns_len(char *, char *, int, u_char *);

int dns_rr_mx(char *, char *, int, int, u_char *);
int dns_rr_mx_len(char *, char *, int, int, u_char *);

int dns_rr_cname(char *, char *, int, u_char *);
int dns_rr_cname_len(char *, char *, int, u_char *);

int dns_rr_soa(char *, char *, char *, long, long, long, long, int, u_char *);
int dns_rr_soa_len(char *, char *, char *, long, long, long, long, int, u_char *);

int dns_rr_query(char *, int, u_char *);
int dns_rr_query_len(char *, int, u_char *);

int dns_rr_x_a(char *, u_long *, u_char *, u_char *, u_char *);
int dns_rr_x_mx(char *, int *, char *, u_char *, u_char *, u_char *);
int dns_rr_x_ptr(char *, char *, u_char *, u_char *, u_char *);
int dns_rr_x_cname(char *, char *, u_char *, u_char *, u_char *);
int dns_rr_x_ns(char *, char *, u_char *, u_char *, u_char *);
int dns_rr_x_soa(char *, char *, char *, 
	                u_long *, u_long *, u_long*, u_long *, u_long *, 
	                u_char *, u_char *, u_char *);

int dns_rr_x_query(char *, int *, u_char *, u_char *, u_char *);

int dns_rr_x_type(u_char *, u_char *);
int dns_rr_x_ttl(u_char *, u_char *);

u_char *dns_skip(u_char *, u_char *);

int dns_string(char *, u_char *, int);

#endif

