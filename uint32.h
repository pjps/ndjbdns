#pragma once 

#include <stdint.h>

typedef uint32_t uint32;

extern void uint32_pack(char *,uint32);
extern void uint32_pack_big(char *,uint32);
extern void uint32_unpack(const char *,uint32 *);
extern void uint32_unpack_big(const char *,uint32 *);
