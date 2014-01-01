#pragma once 

#include "uint32.h"
#include "uint64.h"

extern uint64 cache_motion;
extern int cache_init(unsigned int);
extern void cache_set(const char *,unsigned int,const char *,unsigned int,uint32);
extern char *cache_get(const char *,unsigned int,unsigned int *,uint32 *);
