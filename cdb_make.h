/*
 * cdb_make.h: This file is part of the `djbdns' project, originally written
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

#include "buffer.h"
#include "uint32.h"

#define CDB_HPLIST 1000

struct cdb_hp
{
    uint32 h;
    uint32 p;
};

struct cdb_hplist
{
    struct cdb_hp hp[CDB_HPLIST];
    struct cdb_hplist *next;
    int num;
};

struct cdb_make
{
    char bspace[8192];
    char final[2048];
    uint32 count[256];
    uint32 start[256];
    struct cdb_hplist *head;
    struct cdb_hp *split; /* includes space for hash */
    struct cdb_hp *hash;
    uint32 numentries;
    buffer b;
    uint32 pos;
    int fd;
};

extern int cdb_make_start (struct cdb_make *, int);

extern int cdb_make_addbegin (struct cdb_make *, unsigned int, unsigned int);

extern int cdb_make_addend (struct cdb_make *, unsigned int,
                                    unsigned int, uint32);

extern int cdb_make_add (struct cdb_make *, const char *,
                                    unsigned int, const char *, unsigned int);

extern int cdb_make_finish (struct cdb_make *);
