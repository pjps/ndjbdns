#pragma once 

#include "buffer.h"
#include "stralloc.h"

extern int getln(buffer *,stralloc *,int *,int);
extern int getln2(buffer *,stralloc *,char **,unsigned int *,int);
