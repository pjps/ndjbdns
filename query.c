/*
 * query.c: This file is part of the `djbdns' project, originally written
 * by Dr. D J Bernstein and later released under public-domain since late
 * December 2007 (http://cr.yp.to/distributors.html).
 *
 * Copyright (C) 2009 - 2012 Prasad J Pandit
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

#include "dd.h"
#include "cdb.h"
#include "dns.h"
#include "log.h"
#include "byte.h"
#include "case.h"
#include "cache.h"
#include "alloc.h"
#include "query.h"
#include "error.h"
#include "roots.h"
#include "uint64.h"
#include "uint32.h"
#include "uint16.h"
#include "response.h"

extern short debug_level;
static int flagforwardonly = 0;

struct cdb bl;      /* dns block list */

void
query_forwardonly (void)
{
    flagforwardonly = 1;
}

static void
cachegeneric (const char type[2], const char *d,
                    const char *data, unsigned int datalen, uint32 ttl)
{
    char key[257];
    unsigned int len = 0;

    len = dns_domain_length (d);
    if (len > 255)
      return;

    byte_copy (key, 2, type);
    byte_copy (key + 2, len, d);
    case_lowerb (key + 2, len);

    cache_set (key, len + 2, data, datalen, ttl);
}


static char save_buf[8192];
static unsigned int save_ok;
static unsigned int save_len;

static void
save_start (void)
{
    save_ok = 1;
    save_len = 0;
}

static void
save_data (const char *buf, unsigned int len)
{
    if (!save_ok)
        return;
    if (len > (sizeof save_buf) - save_len)
    {
        save_ok = 0;
        return;
    }
    byte_copy (save_buf + save_len, len, buf);
    save_len += len;
}

static void
save_finish (const char type[2], const char *d, uint32 ttl)
{
    if (!save_ok)
        return;
    cachegeneric (type, d, save_buf, save_len, ttl);
}

static int
typematch (const char rtype[2], const char qtype[2])
{
    return byte_equal (qtype, 2, rtype) || byte_equal (qtype, 2, DNS_T_ANY);
}

static uint32
ttlget (char buf[4])
{
    uint32 ttl = 0;

    uint32_unpack_big (buf, &ttl);
    if (ttl > 1000000000)
        return 0;
    if (ttl > 604800)
        return 604800;

    return ttl;
}


static void
cleanup (struct query *z)
{
    int j = 0, k = 0;

    dns_transmit_free (&z->dt);
    for (j = 0; j < QUERY_MAXALIAS; ++j)
        dns_domain_free (&z->alias[j]);

    for (j = 0; j < QUERY_MAXLEVEL; ++j)
    {
        dns_domain_free (&z->name[j]);
        for (k = 0; k < QUERY_MAXNS; ++k)
            dns_domain_free (&z->ns[j][k]);
    }
}


static int
rqa (struct query *z)
{
    int i = 0;

    for (i = QUERY_MAXALIAS - 1; i >= 0; --i)
    {
        if (z->alias[i])
        {
            if (!response_query (z->alias[i], z->type, z->class))
                return 0;
            while (i > 0)
            {
                if (!response_cname (z->alias[i],
                                     z->alias[i - 1], z->aliasttl[i]))
                    return 0;
                --i;
            }
            if (!response_cname (z->alias[0], z->name[0], z->aliasttl[0]))
                return 0;

            return 1;
        }
    }

    if (!response_query (z->name[0], z->type, z->class))
        return 0;

    return 1;
}

static int
globalip (char *d, char ip[4])
{
    if (dns_domain_equal (d, "\011localhost\0"))
    {
        byte_copy (ip, 4, "\177\0\0\1");
        return 1;
    }
    if (dd (d, "", ip) == 4)
        return 1;

    return 0;
}

static char *t1 = 0;
static char *t2 = 0;
static char *t3 = 0;
static char *cname = 0;
static char *referral = 0;
static unsigned int *records = 0;

static int
smaller (char *buf, unsigned int len, unsigned int pos1, unsigned int pos2)
{
    int r = 0;
    char header1[12], header2[12];
    unsigned int len1 = 0, len2 = 0;

    pos1 = dns_packet_getname (buf, len, pos1, &t1);
    dns_packet_copy (buf, len, pos1, header1, 10);
    pos2 = dns_packet_getname (buf, len, pos2, &t2);
    dns_packet_copy (buf, len, pos2, header2, 10);

    r = byte_diff (header1, 4, header2);
    if (r < 0)
        return 1;
    if (r > 0)
        return 0;

    len1 = dns_domain_length (t1);
    len2 = dns_domain_length (t2);
    if (len1 < len2)
        return 1;
    if (len1 > len2)
        return 0;

    r = case_diffb (t1, len1, t2);
    if (r < 0)
        return 1;
    if (r > 0)
        return 0;

    if (pos1 < pos2)
        return 1;

    return 0;
}

static int
doit (struct query *z, int state)
{
    char key[257];
    char misc[20], header[12];
    char *buf = 0, *cached = 0;
    const char *whichserver = 0;

    unsigned int rcode = 0;
    unsigned int posanswers = 0;
    unsigned int len = 0, cachedlen = 0;

    uint16 numanswers = 0;
    uint16 numauthority = 0;
    unsigned int posauthority = 0;

    uint16 numglue = 0;
    unsigned int pos = 0, pos2 = 0;

    uint16 datalen = 0;
    char *control = 0, *d = 0;

    const char *dtype = 0;
    unsigned int dlen = 0;

    int flagout = 0, flagcname = 0;
    int flagreferral = 0, flagsoa = 0;

    int i = 0, j = 0, k = 0, p = 0, q = 0;
    uint32 ttl = 0, soattl = 0, cnamettl = 0;

    extern int uactive;
    extern int tactive;
    extern uint64 numqueries;
    extern uint64 cache_motion;

    errno = error_io;
    if (state == 1)
        goto HAVEPACKET;
    if (state == -1)
    {
        if (debug_level > 1)
            log_servfail (z->name[z->level]);
        goto SERVFAIL;
    }


NEWNAME:
    if (++z->loop == QUERY_MAXLOOP)
        goto DIE;
    d = z->name[z->level];
    dtype = z->level ? DNS_T_A : z->type;
    dlen = dns_domain_length (d);

    if (globalip (d, misc))
    {
        if (z->level)
        {
            for (k = 0; k < 64; k += 4)
            {
                if (byte_equal (z->servers[z->level - 1] + k, 4, "\0\0\0\0"))
                {
                    byte_copy (z->servers[z->level - 1] + k, 4, misc);
                    break;
                }
            }
            goto LOWERLEVEL;
        }
        if (!rqa (z))
            goto DIE;
        if (typematch (DNS_T_A, dtype))
        {
            if (!response_rstart (d, DNS_T_A, 655360))
                goto DIE;
            if (!response_addbytes (misc, 4))
                goto DIE;
            response_rfinish (RESPONSE_ANSWER);
        }
        cleanup (z);

        return 1;
    }

    if (dns_domain_equal (d, "\0011\0010\0010\003127\7in-addr\4arpa\0"))
    {
        if (z->level)
            goto LOWERLEVEL;
        if (!rqa (z))
            goto DIE;
        if (typematch (DNS_T_PTR, dtype))
        {
            if (!response_rstart (d, DNS_T_PTR, 655360))
                goto DIE;
            if (!response_addname ("\011localhost\0"))
                goto DIE;

            response_rfinish (RESPONSE_ANSWER);
        }
        cleanup (z);
        if (debug_level > 2)
            log_stats (uactive, tactive, numqueries, cache_motion);

        return 1;
    }

    if (dlen <= 255)
    {
        byte_copy (key, 2, DNS_T_ANY);
        byte_copy (key + 2, dlen, d);
        case_lowerb (key + 2, dlen);
        cached = cache_get (key, dlen + 2, &cachedlen, &ttl);
        if (cached)
        {
            if (debug_level > 2)
                log_cachednxdomain (d);
            goto NXDOMAIN;
        }

        byte_copy (key, 2, DNS_T_CNAME);
        cached = cache_get (key, dlen + 2, &cachedlen, &ttl);
        if (cached)
        {
            if (typematch (DNS_T_CNAME, dtype))
            {
                if (debug_level > 2)
                    log_cachedanswer (d, DNS_T_CNAME);

                if (!rqa (z))
                    goto DIE;
                if (!response_cname (z->name[0], cached, ttl))
                    goto DIE;
                cleanup (z);

                return 1;
            }
            if (debug_level > 2)
                log_cachedcname (d, cached);

            if (!dns_domain_copy (&cname, cached))
                goto DIE;

            goto CNAME;
        }

        if (typematch (DNS_T_NS, dtype))
        {
            byte_copy (key, 2, DNS_T_NS);
            cached = cache_get (key, dlen + 2, &cachedlen, &ttl);
            if (cached && (cachedlen || byte_diff (dtype, 2, DNS_T_ANY)))
            {
                if (debug_level > 2)
                    log_cachedanswer (d, DNS_T_NS);
                if (!rqa (z))
                    goto DIE;

                pos = 0;
                while ((pos=dns_packet_getname (cached, cachedlen, pos, &t2)))
                {
                    if (!response_rstart (d, DNS_T_NS, ttl))
                        goto DIE;
                    if (!response_addname (t2))
                        goto DIE;

                    response_rfinish (RESPONSE_ANSWER);
                }
                cleanup (z);

                return 1;
            }
        }

        if (typematch (DNS_T_PTR, dtype))
        {
            byte_copy (key, 2, DNS_T_PTR);
            cached = cache_get (key, dlen + 2, &cachedlen, &ttl);
            if (cached && (cachedlen || byte_diff(dtype, 2, DNS_T_ANY)))
            {
                if (debug_level > 2)
                    log_cachedanswer (d, DNS_T_PTR);
                if (!rqa (z))
                    goto DIE;

                pos = 0;
                while ((pos=dns_packet_getname (cached, cachedlen, pos, &t2)))
                {
                    if (!response_rstart (d, DNS_T_PTR, ttl))
                        goto DIE;
                    if (!response_addname (t2))
                        goto DIE;

                    response_rfinish (RESPONSE_ANSWER);
                }
                cleanup(z);

                return 1;
            }
        }

        if (typematch (DNS_T_MX, dtype))
        {
            byte_copy (key, 2, DNS_T_MX);
            cached = cache_get (key, dlen + 2, &cachedlen, &ttl);
            if (cached && (cachedlen || byte_diff (dtype, 2, DNS_T_ANY)))
            {
                if (debug_level > 2)
                    log_cachedanswer (d, DNS_T_MX);
                if (!rqa (z))
                    goto DIE;

                pos = 0;
                while ((pos=dns_packet_copy (cached, cachedlen, pos, misc, 2)))
                {
                    pos = dns_packet_getname (cached, cachedlen, pos, &t2);
                    if (!pos)
                        break;
                    if (!response_rstart (d, DNS_T_MX, ttl))
                        goto DIE;
                    if (!response_addbytes (misc, 2))
                        goto DIE;
                    if (!response_addname (t2))
                        goto DIE;

                    response_rfinish (RESPONSE_ANSWER);
                }
                cleanup (z);

                return 1;
            }
        }

        if (typematch (DNS_T_SOA, dtype))
        {
            byte_copy (key, 2, DNS_T_SOA);
            cached = cache_get (key, dlen + 2, &cachedlen, &ttl);
            if (cached && (cachedlen || byte_diff (dtype, 2, DNS_T_ANY)))
            {
                log_cachedanswer (d, DNS_T_SOA);
                if (!rqa (z))
                    goto DIE;

                pos = 0;
                while ((pos=dns_packet_copy(cached, cachedlen, pos, misc, 20)))
                {
                    pos = dns_packet_getname (cached, cachedlen, pos, &t2);
                    if (!pos)
                        break;

                    pos = dns_packet_getname (cached, cachedlen, pos, &t3);
                    if (!pos)
                        break;

                    if (!response_rstart (d, DNS_T_SOA, ttl))
                        goto DIE;
                    if (!response_addname (t2))
                        goto DIE;
                    if (!response_addname (t3))
                        goto DIE;
                    if (!response_addbytes(misc, 20))
                        goto DIE;

                    response_rfinish (RESPONSE_ANSWER);
                }
                cleanup (z);
                return 1;
            }
        }

        if (typematch (DNS_T_A, dtype))
        {
            byte_copy (key, 2, DNS_T_A);
            cached = cache_get (key, dlen + 2, &cachedlen, &ttl);
            if (cached && (cachedlen || byte_diff (dtype, 2, DNS_T_ANY)))
            {
                if (z->level)
                {
                    if (debug_level > 2)
                        log_cachedanswer (d, DNS_T_A);
                    while (cachedlen >= 4)
                    {
                        for (k = 0; k < 64; k += 4)
                        {
                            if (byte_equal (z->servers[z->level - 1] + k,
                                            4, "\0\0\0\0"))
                            {
                                byte_copy (z->servers[z->level - 1] + k,
                                            4, cached);
                                break;
                            }
                        }
                        cached += 4;
                        cachedlen -= 4;
                    }
                    goto LOWERLEVEL;
                }

                if (debug_level > 2)
                    log_cachedanswer (d, DNS_T_A);
                if (!rqa (z))
                    goto DIE;
                while (cachedlen >= 4)
                {
                    if (!response_rstart (d, DNS_T_A, ttl))
                        goto DIE;
                    if (!response_addbytes (cached, 4))
                        goto DIE;
                    response_rfinish (RESPONSE_ANSWER);
                    cached += 4;
                    cachedlen -= 4;
                }
                cleanup (z);

                return 1;
            }
        }

        if (!typematch (DNS_T_ANY, dtype)
            && !typematch (DNS_T_AXFR, dtype)
            && !typematch (DNS_T_CNAME, dtype)
            && !typematch (DNS_T_NS, dtype)
            && !typematch (DNS_T_PTR, dtype)
            && !typematch (DNS_T_A, dtype)
            && !typematch (DNS_T_MX, dtype)
            && !typematch (DNS_T_SOA, dtype))
        {
            byte_copy (key, 2, dtype);
            cached = cache_get (key, dlen + 2, &cachedlen, &ttl);
            if (cached && (cachedlen || byte_diff (dtype, 2, DNS_T_ANY)))
            {
                if (debug_level > 2)
                    log_cachedanswer (d, dtype);
                if (!rqa (z))
                    goto DIE;
                while (cachedlen >= 2)
                {
                    uint16_unpack_big (cached, &datalen);
                    cached += 2;
                    cachedlen -= 2;
                    if (datalen > cachedlen)
                        goto DIE;
                    if (!response_rstart (d, dtype, ttl))
                        goto DIE;
                    if (!response_addbytes (cached, datalen))
                        goto DIE;
                    response_rfinish (RESPONSE_ANSWER);
                    cached += datalen;
                    cachedlen -= datalen;
                }
                cleanup (z);

                return 1;
            }
        }
    }

    for (;;)
    {
        if (roots (z->servers[z->level], d))
        {
            for (j = 0; j < QUERY_MAXNS; ++j)
                dns_domain_free (&z->ns[z->level][j]);
            z->control[z->level] = d;
            break;
        }

        if (!flagforwardonly && (z->level < 2))
        {
            if (dlen < 255)
            {
                byte_copy (key,2,DNS_T_NS);
                byte_copy (key + 2,dlen,d);
                case_lowerb (key + 2,dlen);
                cached = cache_get (key, dlen + 2, &cachedlen, &ttl);
                if (cached && cachedlen)
                {
                    z->control[z->level] = d;
                    byte_zero (z->servers[z->level],64);
                    for (j = 0; j < QUERY_MAXNS; ++j)
                        dns_domain_free (&z->ns[z->level][j]);

                    j = pos = 0;
                    pos = dns_packet_getname (cached, cachedlen, pos, &t1);
                    while (pos)
                    {
                        if (debug_level > 2)
                            log_cachedns (d, t1);
                        if (j < QUERY_MAXNS)
                            if (!dns_domain_copy (&z->ns[z->level][j++], t1))
                                goto DIE;

                        pos = dns_packet_getname (cached, cachedlen, pos, &t1);
                    }
                    break;
                }
            }
        }

        if (!*d)
            goto DIE;
        j = 1 + (unsigned int) (unsigned char) *d;
        dlen -= j;
        d += j;
    }


HAVENS:
    for (j = 0; j < QUERY_MAXNS; ++j)
    {
        if (z->ns[z->level][j])
        {
            if (z->level + 1 < QUERY_MAXLEVEL)
            {
                int dc = dns_domain_copy (&z->name[z->level + 1],
                                                    z->ns[z->level][j]);
                if (!dc)
                    goto DIE;

                dns_domain_free (&z->ns[z->level][j]);
                ++z->level;
                goto NEWNAME;
            }
            dns_domain_free (&z->ns[z->level][j]);
        }
    }

    for (j = 0; j < 64; j += 4)
        if (byte_diff (z->servers[z->level] + j, 4, "\0\0\0\0"))
            break;
    if (j == 64)
        goto SERVFAIL;

    dns_sortip (z->servers[z->level], 64);
    if (z->level)
    {
        if (debug_level > 2)
            log_tx (z->name[z->level], DNS_T_A,
                        z->control[z->level], z->servers[z->level],z->level);

        if (dns_transmit_start (&z->dt, z->servers[z->level], flagforwardonly,
                                z->name[z->level], DNS_T_A, z->localip) == -1)
            goto DIE;
    }
    else
    {
        if (bl.map
            && cdb_find (&bl, z->name[0], dns_domain_length (z->name[0])) > 0)
        {
            errno = error_blockedbydbl;
            goto DIE;
        }
        if (debug_level > 2)
            log_tx (z->name[0], z->type, z->control[0], z->servers[0], 0);

        if (dns_transmit_start (&z->dt, z->servers[0], flagforwardonly,
                                z->name[0], z->type, z->localip) == -1)
            goto DIE;
    }
    return 0;


LOWERLEVEL:
    dns_domain_free (&z->name[z->level]);
    for (j = 0; j < QUERY_MAXNS; ++j)
        dns_domain_free (&z->ns[z->level][j]);
    --z->level;
    goto HAVENS;


HAVEPACKET:
    if (++z->loop == QUERY_MAXLOOP)
        goto DIE;
    buf = z->dt.packet;
    len = z->dt.packetlen;

    whichserver = z->dt.servers + 4 * z->dt.curserver;
    control = z->control[z->level];
    d = z->name[z->level];
    dtype = z->level ? DNS_T_A : z->type;

    if (!(pos = dns_packet_copy (buf, len, 0, header, 12)))
        goto DIE;
    if (!(pos = dns_packet_skipname (buf, len, pos)))
        goto DIE;
    pos += 4;
    posanswers = pos;

    uint16_unpack_big (header + 6, &numanswers);
    uint16_unpack_big (header + 8, &numauthority);
    uint16_unpack_big (header + 10, &numglue);

    rcode = header[3] & 15;
    if (rcode && (rcode != 3))
        goto DIE; /* impossible; see irrelevant() */

    flagsoa = soattl = cnamettl = 0;
    flagout = flagcname = flagreferral = 0;
    for (j = 0; j < numanswers; ++j)
    {
        pos = dns_packet_getname (buf, len, pos, &t1);
        if (!pos)
            goto DIE;
        pos = dns_packet_copy (buf, len, pos, header, 10);
        if (!pos)
            goto DIE;

        if (dns_domain_equal (t1, d))
        {
            if (byte_equal (header + 2, 2, DNS_C_IN))
            {
                /* should always be true */
                if (typematch (header, dtype))
                    flagout = 1;
                else if (typematch (header, DNS_T_CNAME))
                {
                    if (!dns_packet_getname (buf, len, pos, &cname))
                        goto DIE;
                    flagcname = 1;
                    cnamettl = ttlget (header + 4);
                }
            }
        }
        uint16_unpack_big (header + 8, &datalen);
        pos += datalen;
    }
    posauthority = pos;

    for (j = 0; j < numauthority; ++j)
    {
        pos = dns_packet_getname (buf, len, pos, &t1);
        if (!pos)
            goto DIE;
        pos = dns_packet_copy (buf, len, pos, header, 10);
        if (!pos)
            goto DIE;

        if (typematch (header, DNS_T_SOA))
        {
            flagsoa = 1;
            soattl = ttlget (header + 4);
            if (soattl > 3600)
                soattl = 3600;
        }
        else if (typematch (header, DNS_T_NS))
        {
            flagreferral = 1;
            if (!dns_domain_copy (&referral, t1))
                goto DIE;
        }

        uint16_unpack_big (header + 8, &datalen);
        pos += datalen;
    }

    if (!flagcname && !rcode && !flagout && flagreferral && !flagsoa)
    {
        if (dns_domain_equal (referral, control)
            || !dns_domain_suffix (referral, control))
        {
            if (debug_level > 2)
                log_lame (whichserver, control, referral);
            byte_zero (whichserver, 4);

            goto HAVENS;
        }
    }

    if (records)
    {
        alloc_free (records);
        records = 0;
    }

    k = numanswers + numauthority + numglue;
    records = (unsigned int *) alloc (k * sizeof (unsigned int));
    if (!records)
        goto DIE;

    pos = posanswers;
    for (j = 0; j < k; ++j)
    {
        records[j] = pos;
        pos = dns_packet_getname (buf, len, pos, &t1);
        if (!pos)
            goto DIE;
        pos = dns_packet_copy (buf, len, pos, header, 10);
        if (!pos)
            goto DIE;
        uint16_unpack_big (header + 8, &datalen);
        pos += datalen;
    }

    i = j = k;
    while (j > 1)
    {
        if (i > 1)
        {
            --i;
            pos = records[i - 1];
        }
        else
        {
            pos = records[j - 1];
            records[j - 1] = records[i - 1];
            --j;
        }

        q = i;
        while ((p = q * 2) < j)
        {
            if (!smaller (buf, len, records[p], records[p - 1]))
                ++p;
            records[q - 1] = records[p - 1];
            q = p;
        }
        if (p == j)
        {
            records[q - 1] = records[p - 1];
            q = p;
        }
        while ((q > i) && smaller (buf, len, records[(p = q/2) - 1], pos))
        {
            records[q - 1] = records[p - 1];
            q = p;
        }
        records[q - 1] = pos;
    }

    i = 0;
    while (i < k)
    {
        char type[2];

        if (!(pos = dns_packet_getname (buf, len, records[i], &t1)))
            goto DIE;
        if (!(pos = dns_packet_copy (buf, len, pos, header, 10)))
            goto DIE;
        ttl = ttlget (header + 4);

        byte_copy (type, 2, header);
        if (byte_diff (header + 2, 2, DNS_C_IN))
        {
            ++i;
            continue;
        }

        for (j = i + 1; j < k; ++j)
        {
            pos = dns_packet_getname (buf, len, records[j], &t2);
            if (!pos)
                goto DIE;
            pos = dns_packet_copy (buf, len, pos, header, 10);
            if (!pos)
                goto DIE;
            if (!dns_domain_equal (t1, t2))
                break;
            if (byte_diff (header, 2, type))
                break;
            if (byte_diff (header + 2, 2, DNS_C_IN))
                break;
        }

        if (!dns_domain_suffix (t1, control))
        {
            i = j;
            continue;
        }
        if (!flagforwardonly && byte_equal (type, 2, DNS_T_NS)
            && dns_domain_equal (t1, control))
        {
            char dummy[256];
            if (!roots (dummy, control))
            {
                i = j;
                continue;
            }
        }
        if (!roots_same (t1, control))
        {
            i = j;
            continue;
        }
        if (byte_equal (type, 2, DNS_T_ANY))
            ;
        else if (byte_equal(type, 2, DNS_T_AXFR))
            ;
        else if (byte_equal (type, 2, DNS_T_SOA))
        {
            int non_authority = 0;
            save_start ();
            while (i < j)
            {
                pos = dns_packet_skipname (buf, len, records[i]);
                if (!pos)
                    goto DIE;
                pos = dns_packet_getname (buf, len, pos + 10, &t2);
                if (!pos)
                    goto DIE;
                pos = dns_packet_getname (buf, len, pos, &t3);
                if (!pos)
                    goto DIE;
                pos = dns_packet_copy (buf, len, pos, misc, 20);
                if (!pos)
                    goto DIE;
                if (records[i] < posauthority)
                {
                      if (debug_level > 2)
                          log_rrsoa (whichserver, t1, t2, t3, misc, ttl);
                      save_data (misc, 20);
                      save_data (t2, dns_domain_length (t2));
                      save_data (t3, dns_domain_length (t3));
                      non_authority++;
                }
                ++i;
            }
            if (non_authority)
                save_finish (DNS_T_SOA, t1, ttl);
        }
        else if (byte_equal (type, 2, DNS_T_CNAME))
        {
            pos = dns_packet_skipname (buf, len, records[j - 1]);
            if (!pos)
                goto DIE;
            pos = dns_packet_getname (buf, len, pos + 10, &t2);
            if (!pos)
                goto DIE;

            if (debug_level > 2)
                log_rrcname (whichserver, t1, t2, ttl);

            cachegeneric (DNS_T_CNAME, t1, t2, dns_domain_length (t2), ttl);
        }
        else if (byte_equal (type, 2, DNS_T_PTR))
        {
            save_start ();
            while (i < j)
            {
                pos = dns_packet_skipname (buf, len, records[i]);
                if (!pos)
                    goto DIE;
                pos = dns_packet_getname (buf, len, pos + 10, &t2);
                if (!pos)
                    goto DIE;
                if (debug_level > 2)
                    log_rrptr (whichserver, t1, t2, ttl);

                save_data (t2, dns_domain_length (t2));
                ++i;
            }
            save_finish (DNS_T_PTR, t1, ttl);
        }
        else if (byte_equal (type, 2, DNS_T_NS))
        {
            save_start ();
            while (i < j)
            {
                pos = dns_packet_skipname (buf, len, records[i]);
                if (!pos)
                    goto DIE;
                pos = dns_packet_getname (buf, len, pos + 10, &t2);
                if (!pos)
                    goto DIE;
                if (debug_level > 2)
                    log_rrns (whichserver, t1, t2, ttl);
                save_data (t2, dns_domain_length (t2));
                ++i;
            }
            save_finish (DNS_T_NS, t1, ttl);
        }
        else if (byte_equal (type, 2, DNS_T_MX))
        {
            save_start ();
            while (i < j)
            {
                pos = dns_packet_skipname (buf, len, records[i]);
                if (!pos)
                    goto DIE;
                pos = dns_packet_copy (buf, len, pos + 10, misc, 2);
                if (!pos)
                    goto DIE;
                pos = dns_packet_getname (buf, len, pos, &t2);
                if (!pos)
                    goto DIE;
                if (debug_level > 2)
                    log_rrmx (whichserver, t1, t2, misc, ttl);
                save_data (misc, 2);
                save_data (t2, dns_domain_length (t2));
                ++i;
            }
            save_finish (DNS_T_MX, t1, ttl);
        }
        else if (byte_equal (type, 2, DNS_T_A))
        {
            save_start ();
            while (i < j)
            {
                pos = dns_packet_skipname (buf, len, records[i]);
                if (!pos)
                    goto DIE;
                pos = dns_packet_copy (buf, len, pos, header, 10);
                if (!pos)
                    goto DIE;
                if (byte_equal (header + 8, 2, "\0\4"))
                {
                    pos = dns_packet_copy (buf, len, pos, header, 4);
                    if (!pos)
                        goto DIE;
                    save_data (header, 4);

                    if (debug_level > 2)
                        log_rr (whichserver, t1, DNS_T_A, header, 4, ttl);
                }
                ++i;
            }
            save_finish (DNS_T_A, t1, ttl);
        }
        else
        {
            save_start ();
            while (i < j)
            {
                pos = dns_packet_skipname (buf, len, records[i]);
                if (!pos)
                    goto DIE;
                pos = dns_packet_copy (buf, len, pos, header, 10);
                if (!pos)
                    goto DIE;
                uint16_unpack_big (header + 8, &datalen);
                if (datalen > len - pos)
                    goto DIE;
                save_data (header + 8, 2);
                save_data (buf + pos, datalen);

                if (debug_level > 2)
                    log_rr (whichserver, t1, type, buf + pos, datalen, ttl);

                ++i;
            }
            save_finish (type, t1, ttl);
        }
        i = j;
    }
    alloc_free (records);
    records = 0;

    if (flagcname)
    {
        ttl = cnamettl;
CNAME:
        if (!z->level)
        {
            if (z->alias[QUERY_MAXALIAS - 1])
                goto DIE;

            for (j = QUERY_MAXALIAS - 1; j > 0; --j)
                z->alias[j] = z->alias[j - 1];
            for (j = QUERY_MAXALIAS - 1; j > 0; --j)
                z->aliasttl[j] = z->aliasttl[j - 1];

            z->alias[0] = z->name[0];
            z->aliasttl[0] = ttl;
            z->name[0] = 0;
        }
        if (!dns_domain_copy (&z->name[z->level], cname))
            goto DIE;

        goto NEWNAME;
    }

    if (rcode == 3)
    {
        if (debug_level > 2)
            log_nxdomain (whichserver, d, soattl);
        cachegeneric (DNS_T_ANY, d, "", 0, soattl);

NXDOMAIN:
        if (z->level)
            goto LOWERLEVEL;
        if (!rqa (z))
            goto DIE;

        response_nxdomain ();
        cleanup (z);

        return 1;
    }

    if (!flagout && flagsoa)
        if (byte_diff (DNS_T_ANY, 2, dtype))
            if (byte_diff (DNS_T_AXFR, 2, dtype))
                if (byte_diff (DNS_T_CNAME, 2, dtype))
                {
                    save_start ();
                    save_finish (dtype, d, soattl);
                    if (debug_level > 2)
                        log_nodata (whichserver, d, dtype, soattl);
                }

    if (debug_level > 2)
        log_stats (uactive, tactive, numqueries, cache_motion);

    if (flagout || flagsoa || !flagreferral)
    {
        if (z->level)
        {
            pos = posanswers;
            for (j = 0; j < numanswers; ++j)
            {
                pos = dns_packet_getname (buf, len, pos, &t1);
                if (!pos)
                    goto DIE;
                pos = dns_packet_copy (buf, len, pos, header, 10);
                if (!pos)
                    goto DIE;
                uint16_unpack_big (header + 8, &datalen);
                if (dns_domain_equal (t1, d))
                    if (typematch (header, DNS_T_A))
                        if (byte_equal (header + 2, 2, DNS_C_IN))
                            /* should always be true */
                            if (datalen == 4)
                                for (k = 0; k < 64; k += 4)
                                {
                                    if (byte_equal (z->servers[z->level - 1]
                                                     + k, 4, "\0\0\0\0"))
                                    {
                                        if (!dns_packet_copy (buf, len, pos,
                                             z->servers[z->level - 1] + k, 4))
                                            goto DIE;
                                        break;
                                    }
                                }
                pos += datalen;
            }
            goto LOWERLEVEL;
        }

        if (!rqa (z))
            goto DIE;

        pos = posanswers;
        for (j = 0; j < numanswers; ++j)
        {
            pos = dns_packet_getname (buf, len, pos, &t1);
            if (!pos)
                goto DIE;
            pos = dns_packet_copy (buf, len, pos, header, 10);
            if (!pos)
                goto DIE;
            ttl = ttlget (header + 4);
            uint16_unpack_big (header + 8, &datalen);
            if (dns_domain_equal (t1, d))
            {
                if (byte_equal (header + 2, 2, DNS_C_IN))
                {   /* should always be true */
                    if (typematch (header, dtype))
                    {
                        if (!response_rstart (t1, header, ttl))
                            goto DIE;

                        if (typematch (header, DNS_T_NS)
                            || typematch (header, DNS_T_CNAME)
                            || typematch (header, DNS_T_PTR))
                        {
                            if (!dns_packet_getname (buf, len, pos, &t2))
                                goto DIE;
                            if (!response_addname (t2))
                                goto DIE;
                        }
                        else if (typematch (header, DNS_T_MX))
                        {
                            pos2 = dns_packet_copy (buf, len, pos, misc, 2);
                            if (!pos2)
                                goto DIE;
                            if (!response_addbytes (misc, 2))
                                goto DIE;
                            if (!dns_packet_getname (buf, len, pos2, &t2))
                                goto DIE;
                            if (!response_addname (t2))
                                goto DIE;
                        }
                        else if (typematch (header, DNS_T_SOA))
                        {
                            pos2 = dns_packet_getname (buf, len, pos, &t2);
                            if (!pos2)
                                goto DIE;
                            if (!response_addname (t2))
                                goto DIE;
                            pos2 = dns_packet_getname (buf, len, pos2, &t3);
                            if (!pos2)
                                goto DIE;
                            if (!response_addname (t3))
                                goto DIE;
                            pos2 = dns_packet_copy (buf, len, pos2, misc, 20);
                            if (!pos2)
                                goto DIE;
                            if (!response_addbytes (misc, 20))
                                goto DIE;
                        }
                        else
                        {
                            if (pos + datalen > len)
                                goto DIE;
                            if (!response_addbytes (buf + pos, datalen))
                                goto DIE;
                        }
                        response_rfinish(RESPONSE_ANSWER);
                    }
                }
            }
            pos += datalen;
        }
        cleanup (z);

        return 1;
    }

    if (!dns_domain_suffix (d, referral))
        goto DIE;
    control = d + dns_domain_suffixpos (d, referral);
    z->control[z->level] = control;
    byte_zero (z->servers[z->level], 64);
    for (j = 0; j < QUERY_MAXNS; ++j)
        dns_domain_free (&z->ns[z->level][j]);
    k = 0;

    pos = posauthority;
    for (j = 0; j < numauthority; ++j)
    {
        pos = dns_packet_getname (buf, len, pos, &t1);
        if (!pos)
            goto DIE;
        pos = dns_packet_copy (buf, len, pos, header, 10);
        if (!pos)
            goto DIE;

        uint16_unpack_big (header + 8, &datalen);
        if (dns_domain_equal (referral, t1))    /* should always be true */
            if (typematch (header, DNS_T_NS))   /* should always be true */
                /* should always be true */
                if (byte_equal (header + 2, 2, DNS_C_IN))
                    if (k < QUERY_MAXNS)
                        if (!dns_packet_getname (buf, len, pos,
                                                 &z->ns[z->level][k++]))
                            goto DIE;
        pos += datalen;
    }

    goto HAVENS;


SERVFAIL:
    if (z->level)
        goto LOWERLEVEL;
    if (!rqa (z))
        goto DIE;
    response_servfail ();
    cleanup (z);

    return 1;


DIE:
    cleanup (z);
    if (records)
    {
        alloc_free (records);
        records = 0;
    }

    return -1;
}

int
query_start (struct query *z, char *dn, char type[2],
                              char class[2], char localip[4])
{
    if (byte_equal (type, 2, DNS_T_AXFR))
    {
        errno = error_perm;
        return -1;
    }

    cleanup (z);
    z->level = 0;
    z->loop = 0;

    if (!dns_domain_copy (&z->name[0], dn))
        return -1;

    byte_copy (z->type, 2, type);
    byte_copy (z->class, 2, class);
    byte_copy (z->localip, 4, localip);

    return doit (z, 0);
}

int
query_get (struct query *z, iopause_fd *x, struct taia *stamp)
{
    switch (dns_transmit_get (&z->dt, x, stamp))
    {
    case 1:
        return doit (z, 1);
    case -1:
        return doit (z, -1);
    }

    return 0;
}

void
query_io (struct query *z, iopause_fd *x, struct taia *deadline)
{
    dns_transmit_io (&z->dt, x, deadline);
}
