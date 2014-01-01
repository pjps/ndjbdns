#pragma once 

#include "buffer.h"

extern void init(const char *,const char *);

extern void makedir(const char *);

extern void start(const char *);
extern void outs(const char *);
extern void out(const char *,unsigned int);
extern void copyfrom(buffer *);
extern void finish(void);

extern void perm(int);
extern void owner(int,int);
extern void makelog(const char *,int,int);
