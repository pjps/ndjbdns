#pragma once 

#include "uint32.h"

extern char response[];
extern unsigned int response_len;

extern int response_query(const char *,const char *,const char *);
extern void response_nxdomain(void);
extern void response_servfail(void);
extern void response_id(const char *);
extern void response_tc(void);

extern int response_addbytes(const char *,unsigned int);
extern int response_addname(const char *);
extern void response_hidettl(void);
extern int response_rstart(const char *,const char *,uint32);
extern void response_rfinish(int);

#define RESPONSE_ANSWER 6
#define RESPONSE_AUTHORITY 8
#define RESPONSE_ADDITIONAL 10

extern int response_cname(const char *,const char *,uint32);
