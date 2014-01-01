/*
 * cdb.h: This file is part of the `djbdns' project, originally written
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

#pragma once 

#include "uint32.h"

#define CDB_HASHSTART 5381

struct cdb {
    char *map;      /* 0 if no map is available */
    int fd;
    uint32 size;    /* initialized if map is nonzero */
    uint32 loop;    /* number of hash slots searched under this key */
    uint32 khash;   /* initialized if loop is nonzero */
    uint32 kpos;    /* initialized if loop is nonzero */
    uint32 hpos;    /* initialized if loop is nonzero */
    uint32 hslots;  /* initialized if loop is nonzero */
    uint32 dpos;    /* initialized if cdb_findnext() returns 1 */
    uint32 dlen;    /* initialized if cdb_findnext() returns 1 */
};

extern uint32 cdb_hashadd (uint32, unsigned char);

extern uint32 cdb_hash (const char *, unsigned int);

extern void cdb_free (struct cdb *);

extern void cdb_init (struct cdb *, int fd);

extern int cdb_read (struct cdb *, char *, unsigned int, uint32);

/* extern void __inline__ cdb_findstart (struct cdb *); */

extern int cdb_findnext (struct cdb *, const char *, unsigned int);

extern int cdb_find (struct cdb *, const char *, unsigned int);

#define cdb_datapos(c)      ((c)->dpos)
#define cdb_datalen(c)      ((c)->dlen)
#define cdb_findstart(c)    ((c)->loop = 0)
