/*
 * stralloc.c: This file is part of the `djbdns' project, originally written
 * by Dr. D J Bernstein and later released under public-domain since late
 * December 2007 (http://cr.yp.to/distributors.html).
 *
 * Copyright (C) 2009 - 2013 Prasad J Pandit
 *
 * This program is a free software; you can redistribute it and/or modify
 * it under the terms of GNU General Public License as published by Free
 * Software Foundation; either version 2 of the license or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * of FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


#include "str.h"
#include "byte.h"
#include "alloc.h"
#include "stralloc.h"
#include "gen_allocdefs.h"

GEN_ALLOC_ready(stralloc, char, s, len, a, i, n, x, 30, stralloc_ready)

GEN_ALLOC_readyplus(stralloc, char, s, len, a, i, n, x, 30, stralloc_readyplus)

GEN_ALLOC_append(stralloc,char,s,len,a,i,n,x,30,stralloc_readyplus,stralloc_append)

int
stralloc_catb (stralloc *sa, const char *s, unsigned int n)
{
    if (!sa->s)
        return stralloc_copyb (sa, s, n);
    if (!stralloc_readyplus (sa, n + 1))
        return 0;

    byte_copy (sa->s + sa->len, n, s);
    sa->len += n;
    sa->s[sa->len] = 'Z'; /* ``offensive programming'' */

    return 1;
}

int
stralloc_cat (stralloc *sato, const stralloc *safrom)
{
    return stralloc_catb (sato, safrom->s, safrom->len);
}

int
stralloc_cats (stralloc *sa, const char *s)
{
    return stralloc_catb (sa, s, str_len (s));
}

int
stralloc_copy (stralloc *sato, const stralloc *safrom)
{
    return stralloc_copyb (sato, safrom->s, safrom->len);
}

int
stralloc_catulong0 (stralloc *sa, unsigned long u, unsigned int n)
{
    char *s = (char *)0;
    unsigned long q = 0;
    unsigned int len = 0;

    q = u;
    len = 1;
    while (q > 9)
    {
        ++len;
        q /= 10;
    }
    if (len < n)
        len = n;

    if (!stralloc_readyplus (sa, len))
      return 0;

    s = sa->s + sa->len;
    sa->len += len;
    while (len)
    {
        s[--len] = '0' + (u % 10);
        u /= 10;
    }

    return 1;
}

int
stralloc_catlong0 (stralloc *sa, long l, unsigned int n)
{
    if (l < 0)
    {
        if (!stralloc_append (sa, "-"))
            return 0;
        l = -l;
    }
    return stralloc_catulong0 (sa, l, n);
}

int
stralloc_catulong_ashex0 (stralloc *sa, unsigned long u, unsigned int n)
{
    char *s = (char *)0;
    unsigned long q = 0;
    unsigned int len = 0;

    q = u;
    len = 1;
    while (q > 0xF)
    {
        ++len;
        q /= 0x10;
    }
    if (len < n)
        len = n;

    if (!stralloc_readyplus (sa, len))
      return 0;

    s = sa->s + sa->len;
    sa->len += len;
    while (len)
    {
	if ( (u % 0x10) < 10) {
	    s[--len] = '0' + (u % 0x10);
	} else {
	    s[--len] = 'a' + (u % 0x10 - 10);
	}
        u /= 0x10;
    }

    return 1;
}

int
stralloc_copyb (stralloc *sa, const char *s, unsigned int n)
{
    if (!stralloc_ready (sa, n + 1))
      return 0;

    byte_copy (sa->s, n, s);
    sa->len = n;
    sa->s[n] = 'Z'; /* ``offensive programming'' */

    return 1;
}

int
stralloc_copys (stralloc *sa, const char *s)
{
    return stralloc_copyb (sa, s, str_len (s));
}
