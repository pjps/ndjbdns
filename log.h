/*
 * log.h: This file is part of the `djbdns' project, originally written
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

#include "uint64.h"

extern void log_startup(void);

extern void log_query(uint64, const char *, unsigned int,
                        const char *, const char *, const char *);

extern void log_querydrop(uint64);

extern void log_querydone(uint64, const char *, unsigned int);

extern void log_tcpopen(const char *, unsigned int);

extern void log_tcpclose(const char *, unsigned int);

extern void log_cachedanswer(const char *, const char *);

extern void log_cachedcname(const char *, const char *);

extern void log_cachednxdomain(const char *);

extern void log_cachedns(const char *, const char *);

extern void log_tx(const char *, const char *,
                    const char *, const char *, unsigned int);

extern void log_merge(const char *, const char *, const char *);

extern void log_nxdomain(const char *, const char *, unsigned int);

extern void log_nodata(const char *, const char *, const char *, unsigned int);

extern void log_servfail(const char *);

extern void log_lame(const char *, const char *, const char *);

extern void log_rr(const char *, const char *, const char *,
                        const char *, unsigned int, unsigned int);

extern void log_rrns(const char *, const char *, const char *, unsigned int);

extern void log_rrcname(const char *, const char *, const char *, unsigned int);

extern void log_rrptr(const char *, const char *, const char *, unsigned int);

extern void log_rrmx(const char *, const char *,
                        const char *, const char *, unsigned int);

extern void log_rrsoa(const char *, const char *, const char *,
                        const char *, const char *, unsigned int);

extern void log_stats(int, int, uint64, uint64);
