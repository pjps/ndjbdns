/*
 * dns_transmit.c: This file is part of the `djbdns' project, originally
 * written by Dr. D J Bernstein and later released under public-domain since
 * late December 2007 (http://cr.yp.to/distributors.html).
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

#include <err.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "dns.h"
#include "byte.h"
#include "alloc.h"
#include "error.h"
#include "uint16.h"
#include "socket.h"

static int merge_enable;
static void (*merge_logger)(const char *, const char *, const char *);

void
dns_enable_merge (void (*f)(const char *, const char *, const char *))
{
    merge_enable = 1;
    merge_logger = f;
}

static int
merge_equal (struct dns_transmit *a, struct dns_transmit *b)
{
    const char *ip1 = a->servers + 4 * a->curserver;
    const char *ip2 = b->servers + 4 * b->curserver;

    return byte_equal (ip1, 4, ip2)
            && byte_equal (a->qtype, 2, b->qtype)
            && dns_domain_equal(a->query + 14, b->query + 14);
}

struct dns_transmit *inprogress[MAXUDP];

static int
try_merge (struct dns_transmit *d)
{
    int i = 0;

    for (i = 0; i < MAXUDP; i++)
    {
        if (!inprogress[i])
            continue;
        if (!merge_equal(d, inprogress[i]))
            continue;

        d->master = inprogress[i];
        inprogress[i]->slaves[inprogress[i]->nslaves++] = d;
        return 1;
    }

    return 0;
}

static void
register_inprogress (struct dns_transmit *d)
{
    int i = 0;

    for (i = 0; i < MAXUDP; i++)
    {
        if (!inprogress[i])
        {
            inprogress[i] = d;
            return;
        }
    }

    errx (-1, "BUG: out of in progress slots");
}

static void
unregister_inprogress (struct dns_transmit *d)
{
    int i = 0;

    for (i = 0; i < MAXUDP; i++)
    {
        if (inprogress[i] == d)
            inprogress[i] = 0;
    }
}

static int
serverwantstcp (const char *buf, unsigned int len)
{
    char out[12];

    if (!dns_packet_copy (buf, len, 0, out, 12))
        return 1;
    if (out[2] & 2)
        return 1;

    return 0;
}

static int
serverfailed (const char *buf, unsigned int len)
{
    char out[12];
    unsigned int rcode = 0;

    if (!dns_packet_copy (buf, len, 0, out, 12))
        return 1;

    rcode = out[3];
    rcode &= 15;
    if (rcode && (rcode != 3))
    {
        errno = error_again;
        return 1;
    }

    return 0;
}

static int
irrelevant (const struct dns_transmit *d, const char *buf, unsigned int len)
{
    unsigned int pos = 0;
    char out[12], *dn = NULL;

    if (!(pos = dns_packet_copy (buf, len, 0, out, 12)))
        return 1;
    if (byte_diff (out, 2, d->query + 2))
        return 1;
    if (out[4] != 0)
        return 1;
    if (out[5] != 1)
        return 1;

    if (!(pos = dns_packet_getname (buf, len, pos, &dn)))
        return 1;
    if (!dns_domain_equal (dn, d->query + 14))
    {
        alloc_free (dn);
        return 1;
    }
    alloc_free (dn);

    if (!(pos = dns_packet_copy (buf, len, pos, out, 4)))
        return 1;
    if (byte_diff (out, 2, d->qtype))
        return 1;
    if (byte_diff (out + 2, 2, DNS_C_IN))
        return 1;

    return 0;
}

static void
packetfree (struct dns_transmit *d)
{
    if (!d->packet)
        return;
    alloc_free (d->packet);
    d->packet = 0;
}

static void
mergefree (struct dns_transmit *d)
{
    int i = 0;

    if (merge_enable)
        unregister_inprogress (d);

    /* unregister us from our master */
    if (d->master)
    {
        for (i = 0; i < d->master->nslaves; i++)
            if (d->master->slaves[i] == d)
                d->master->slaves[i] = 0;
        d->master = 0;
    }

    /* and unregister all of our slaves from us */
    for (i = 0; i < d->nslaves; i++)
    {
        if (d->slaves[i])
            d->slaves[i]->master = 0;
    }

    d->nslaves = 0;
}

static void
queryfree (struct dns_transmit *d)
{
    mergefree (d);
    if (!d->query)
      return;
    alloc_free (d->query);
    d->query = 0;
}

static void
socketfree (struct dns_transmit *d)
{
    if (!d->s1)
      return;
    close (d->s1 - 1);
    d->s1 = 0;
}

void
dns_transmit_free (struct dns_transmit *d)
{
    queryfree (d);
    socketfree (d);
    packetfree (d);
}

static int
randombind (struct dns_transmit *d)
{
    int j = 0;

    for (j = 0; j < 10; ++j)
        if (!socket_bind4 (d->s1 - 1, d->localip, 1025 + dns_random (64510)))
            return 0;

    if (!socket_bind4 (d->s1 - 1, d->localip, 0))
        return 0;

    return -1;
}

static const int timeouts[4] = { 1, 3, 11, 45 };

static int
thisudp (struct dns_transmit *d)
{
    const char *ip = NULL;

    socketfree (d);
    mergefree (d);

    while (d->udploop < 4)
    {
        for (; d->curserver < 16; ++d->curserver)
        {
            ip = d->servers + 4 * d->curserver;
            if (byte_diff (ip, 4, "\0\0\0\0"))
            {
                if (merge_enable && try_merge (d))
                {
                    if (merge_logger)
                        merge_logger (ip, d->qtype, d->query + 14);
                    return 0;
                }

                d->query[2] = dns_random (256);
                d->query[3] = dns_random (256);

                d->s1 = 1 + socket_udp ();
                if (!d->s1)
                {
                    dns_transmit_free (d);
                    return -1;
                }
                if (randombind (d) == -1)
                {
                    dns_transmit_free (d);
                    return -1;
                }

                if (socket_connect4 (d->s1 - 1, ip, 53) == 0)
                {
                    if (send (d->s1 - 1, d->query + 2, d->querylen - 2, 0)
                            == d->querylen - 2)
                    {
                        struct taia now;

                        taia_now (&now);
                        taia_uint (&d->deadline, timeouts[d->udploop]);
                        taia_add (&d->deadline, &d->deadline, &now);
                        d->tcpstate = 0;
                        if (merge_enable)
                            register_inprogress (d);
                        return 0;
                    }
                }
                socketfree (d);
            }
        }

        ++d->udploop;
        d->curserver = 0;
    }

    dns_transmit_free (d);
    return -1;
}

static int
firstudp (struct dns_transmit *d)
{
    d->curserver = 0;
    return thisudp (d);
}

static int
nextudp (struct dns_transmit *d)
{
    ++d->curserver;
    return thisudp (d);
}

static int
thistcp (struct dns_transmit *d)
{
    struct taia now;
    const char *ip = NULL;

    socketfree (d);
    packetfree (d);

    for (; d->curserver < 16; ++d->curserver)
    {
        ip = d->servers + 4 * d->curserver;
        if (byte_diff (ip, 4, "\0\0\0\0"))
        {
            d->query[2] = dns_random (256);
            d->query[3] = dns_random (256);

            d->s1 = 1 + socket_tcp ();
            if (!d->s1)
            {
                dns_transmit_free (d);
                return -1;
            }
            if (randombind (d) == -1)
            {
                dns_transmit_free (d);
                return -1;
            }

            taia_now (&now);
            taia_uint (&d->deadline, 10);
            taia_add (&d->deadline, &d->deadline, &now);
            if (socket_connect4 (d->s1 - 1, ip, 53) == 0)
            {
                d->pos = 0;
                d->tcpstate = 2;
                return 0;
            }
            if (errno == error_inprogress || errno == error_wouldblock)
            {
                d->tcpstate = 1;
                return 0;
            }
            socketfree(d);
        }
    }

    dns_transmit_free(d);
    return -1;
}

static int
firsttcp (struct dns_transmit *d)
{
    d->curserver = 0;
    return thistcp (d);
}

static int
nexttcp (struct dns_transmit *d)
{
    ++d->curserver;
    return thistcp (d);
}

int
dns_transmit_start (struct dns_transmit *d, const char servers[64],
                    int flagrecursive, const char *q, const char qtype[2],
                    const char localip[4])
{
    unsigned int len = 0;

    const char s1[] = "\0\0\1\0\0\1\0\0\0\0\0\0";
    const char s2[] = "\0\0\0\0\0\1\0\0\0\0\0\0gcc-bug-workaround";

    dns_transmit_free (d);
    errno = error_io;

    len = dns_domain_length (q);
    d->querylen = len + 18;
    d->query = alloc (d->querylen);
    if (!d->query)
        return -1;

    uint16_pack_big (d->query, len + 16);
    byte_copy (d->query + 2, 12, flagrecursive ? s1 : s2);
    byte_copy (d->query + 14, len, q);
    byte_copy (d->query + 14 + len, 2, qtype);
    byte_copy (d->query + 16 + len, 2, DNS_C_IN);

    byte_copy (d->qtype, 2, qtype);
    d->servers = servers;
    byte_copy (d->localip, 4, localip);

    d->udploop = flagrecursive ? 1 : 0;

    if ((len + 16 > 512) || byte_equal (qtype, 2, DNS_T_ANY))
        return firsttcp (d);

    return firstudp (d);
}

void
dns_transmit_io (struct dns_transmit *d, iopause_fd *x, struct taia *deadline)
{
    x->fd = d->s1 - 1;

    switch (d->tcpstate)
    {
    case 0:
        if (d->master)
            return;
        if (d->packet)
        {
            taia_now (deadline);
            return;
        }
        /* otherwise, fall through */
    case 3:
    case 4:
    case 5:
        x->events = IOPAUSE_READ;
        break;

    case 1:
    case 2:
        x->events = IOPAUSE_WRITE;
    }

    if (taia_less (&d->deadline, deadline))
        *deadline = d->deadline;
}

int
dns_transmit_get (struct dns_transmit *d, const iopause_fd *x,
                                          const struct taia *when)
{
    char udpbuf[4097];
    unsigned char ch = 0;
    int r = 0, fd = 0, i = 0;

    fd = d->s1 - 1;
    errno = error_io;

    if (d->tcpstate == 0 && d->master)
        return 0;
    if (d->tcpstate == 0 && d->packet)
        return 1;

    if (!x->revents)
    {
        if (taia_less (when, &d->deadline))
            return 0;
        errno = error_timeout;
        if (d->tcpstate == 0)
            return nextudp (d);

        return nexttcp (d);
    }

    if (d->tcpstate == 0)
    {
        /*
         * have attempted to send UDP query to each server udploop times
         * have sent query to curserver on UDP socket s
         */
        r = recv (fd, udpbuf, sizeof (udpbuf), 0);
        if (r <= 0)
        {
            if (errno == error_connrefused && d->udploop == 2)
                    return 0;

            return nextudp (d);
        }
        if ((unsigned)r + 1 > sizeof (udpbuf))
            return 0;

        if (irrelevant (d, udpbuf, r))
            return 0;
        if (serverwantstcp (udpbuf, r))
            return firsttcp (d);
        if (serverfailed (udpbuf, r))
        {
            if (d->udploop == 2)
                return 0;

            return nextudp (d);
        }
        socketfree (d);

        d->packetlen = r;
        d->packet = alloc (d->packetlen);
        if (!d->packet)
        {
            dns_transmit_free (d);
            return -1;
        }
        byte_copy (d->packet, d->packetlen, udpbuf);

        for (i = 0; i < d->nslaves; i++)
        {
            if (!d->slaves[i])
                continue;

            d->slaves[i]->packetlen = d->packetlen;
            d->slaves[i]->packet = alloc(d->packetlen);
            if (!d->slaves[i]->packet)
            {
                dns_transmit_free (d->slaves[i]);
                continue;
            }
            byte_copy (d->slaves[i]->packet, d->packetlen, udpbuf);
        }

        queryfree (d);
        return 1;
    }

    if (d->tcpstate == 1)
    {
        /*
         * have sent connection attempt to curserver on TCP socket s
         * pos not defined
         */
        if (!socket_connected (fd))
            return nexttcp (d);

        d->pos = 0;
        d->tcpstate = 2;

        return 0;
    }

    if (d->tcpstate == 2)
    {
        /* 
         * have connection to curserver on TCP socket s have sent pos bytes
         * of query
         */
        r = write (fd, d->query + d->pos, d->querylen - d->pos);
        if (r <= 0)
            return nexttcp (d);

        d->pos += r;
        if (d->pos == d->querylen)
        {
            struct taia now;

            taia_now (&now);
            taia_uint (&d->deadline, 10);
            taia_add (&d->deadline, &d->deadline, &now);
            d->tcpstate = 3;
        }

        return 0;
    }

    if (d->tcpstate == 3)
    {
        /*
         * have sent entire query to curserver on TCP socket s
         * pos not defined
         */
        r = read (fd, &ch, 1);
        if (r <= 0)
            return nexttcp (d);

        d->packetlen = ch;
        d->tcpstate = 4;

        return 0;
    }

    if (d->tcpstate == 4)
    {
        /*
         * have sent entire query to curserver on TCP socket s
         * pos not defined
         * have received one byte of packet length into packetlen
         */
        r = read (fd, &ch, 1);
        if (r <= 0)
            return nexttcp (d);

        d->pos = 0;
        d->tcpstate = 5;
        d->packetlen <<= 8;
        d->packetlen += ch;
        d->packet = alloc (d->packetlen);
        if (!d->packet)
        {
            dns_transmit_free (d);
            return -1;
        }

        return 0;
    }

    if (d->tcpstate == 5)
    {
        /*
         * have sent entire query to curserver on TCP socket s have received
         * entire packet length into packetlen packet is allocated have
         * received pos bytes of packet
         */
        r = read (fd, d->packet + d->pos, d->packetlen - d->pos);
        if (r <= 0)
            return nexttcp (d);

        d->pos += r;
        if (d->pos < d->packetlen)
            return 0;

        socketfree (d);
        if (irrelevant (d, d->packet, d->packetlen))
            return nexttcp(d);
        if (serverwantstcp (d->packet, d->packetlen))
            return nexttcp(d);
        if (serverfailed (d->packet, d->packetlen))
            return nexttcp(d);

        queryfree(d);
        return 1;
    }

    return 0;
}
