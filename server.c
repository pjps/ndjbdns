/*
 * server.c: This file is part of the `djbdns' project, originally written
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

#include <err.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/resource.h>

#include "version.h"

#include "env.h"
#include "ip4.h"
#include "dns.h"
#include "log.h"
#include "byte.h"
#include "case.h"
#include "buffer.h"
#include "strerr.h"
#include "uint16.h"
#include "ndelay.h"
#include "socket.h"
#include "common.h"
#include "iopause.h"
#include "droproot.h"
#include "response.h"

extern void initialize (void);
extern int respond (char *, char *, char *);

static char ip[4];
static uint16 port;
uint16 server_port = 53;

static int len;
static char *q;
static char buf[1024];

static char *prog = NULL;
short mode = 0, debug_level = 0;
char cfgfile[4096], logfile[4096], pidfile[4096];

void
usage (void)
{
    printf ("Usage: %s [OPTIONS]\n", prog);
}

void
printh (void)
{
    usage ();
    printf ("\n Options: \n");
    printf ("%-17s %s\n", "   -d <value>", "print debug messages");
    printf ("%-17s %s\n", "   -c <value>", "full path to config file");
    printf ("%-17s %s\n", "   -l <value>", "full path to log file");
    printf ("%-17s %s\n", "   -p <value>", "full path to pid file");
    printf ("%-17s %s\n", "   -w <value>", "UDP port for server listening. Default: 53");
    printf ("%-17s %s\n", "   -D", "run as daemon");
    printf ("%-17s %s\n", "   -h --help", "print this help");
    printf ("%-17s %s\n", "   -v --version", "print version information");
    printf ("\nReport bugs to <pj.pandit@yahoo.co.in>\n");
}

int
check_option (int argc, char *argv[])
{
    int n = 0, ind = 0;
    const char optstr[] = "+:d:c:l:p:w:Dhv";
    struct option lopt[] = \
    {
        { "help", no_argument, NULL, 'h' },
        { "version", no_argument, NULL, 'v' },
        { 0, 0, 0, 0 }
    };

    opterr = optind = mode = 0;
    while ((n = getopt_long (argc, argv, optstr, lopt, &ind)) != -1)
    {
        switch (n)
        {
        case 'd':
            mode |= DEBUG;
            debug_level = atoi (optarg);
            break;

        case 'D':
            mode |= DAEMON;
            break;

        case 'c':
            strcpy(cfgfile, optarg);
            break;

        case 'l':
            strcpy(logfile, optarg);
            break;

        case 'p':
            strcpy(pidfile, optarg);
            break;

        case 'w':
            server_port = atoi(optarg);
            break;

        case 'h':
            printh ();
            exit (0);
            break;

        case 'v':
            printf ("%s version %s\n", prog, VERSION);
            exit (0);
            break;

        case ':':
            errx (-1, "option `%c' takes an argument, see: --help", optopt);
            break;

        default:
            errx (-1, "unknown option `%c', see: --help", optopt);
            break;
        }
    }

    return optind;
}

static unsigned long long qnum = 0;

static int
doit (void)
{
    char qtype[2];
    char qclass[2];
    char header[12];
    unsigned int pos = 0;

    if ((unsigned)len >= sizeof buf)
        goto NOQ;
    if (!(pos = dns_packet_copy (buf, len, 0, header, 12)))
        goto NOQ;
    if (header[2] & 128)
        goto NOQ;
    if (header[4])
        goto NOQ;
    if (header[5] != 1)
        goto NOQ;

    if (!(pos = dns_packet_getname (buf, len, pos, &q)))
        goto NOQ;
    if (!(pos = dns_packet_copy (buf, len, pos, qtype, 2)))
        goto NOQ;
    if (!(pos = dns_packet_copy (buf, len, pos, qclass, 2)))
        goto NOQ;

    if (!response_query (q, qtype, qclass))
        goto NOQ;
    response_id (header);

    qnum++;
    if (byte_equal (qclass, 2, DNS_C_IN))
        response[2] |= 4;
    else if (byte_diff (qclass, 2, DNS_C_ANY))
            goto WEIRDCLASS;
    response[3] &= ~128;
    if (!(header[2] & 1))
        response[2] &= ~1;

    if (header[2] & 126)
        goto NOTIMP;
    if (byte_equal (qtype, 2, DNS_T_AXFR))
        goto NOTIMP;

    case_lowerb (q, dns_domain_length (q));
    if (!respond (q, qtype, ip))
    {
        log_query (qnum, ip, port, header, q, qtype);
        return 0;
    }
    log_query (qnum, ip, port, header, q, qtype);

    return 1;

NOTIMP:
    response[3] &= ~15;
    response[3] |= 4;
    log_query (qnum, ip, port, header, q, qtype);

    return 1;

WEIRDCLASS:
    response[3] &= ~15;
    response[3] |= 1;
    log_query (qnum, ip, port, header, q, qtype);

    return 1;

NOQ:
    log_query (qnum, ip, port, "\0\0", "", "\0\0");

    return 0;
}

int
main (int argc, char *argv[])
{
    time_t t = 0;
    char *x = NULL;
    struct sigaction sa;
    iopause_fd *iop = NULL;
    int i = 0, n = 0, *udp53 = NULL;

    prog = strdup ((x = strrchr (argv[0], '/')) != NULL ? x + 1 : argv[0]);

    sa.sa_handler = handle_term;
    sigaction (SIGINT, &sa, NULL);
    sigaction (SIGTERM, &sa, NULL);

    sa.sa_handler = SIG_IGN;
    sigaction (SIGPIPE, &sa, NULL);

    i = check_option (argc, argv);
    argc -= i;
    argv += i;

    if (mode & DAEMON)
    {
        i = fork ();
        if (i == -1)
            err (-1, "could not fork a daemon process");
        if (i > 0)
            return 0;
    }

    time (&t);
    memset (buf, 0, sizeof (buf));
    strftime (buf, sizeof (buf), "%b-%d %Y %T %Z", localtime (&t));
    warnx ("version %s: starting: %s\n", VERSION, buf);

    set_timezone ();
    if (debug_level)
        warnx ("TIMEZONE: %s", env_get ("TZ"));

    initialize ();
    if (!debug_level)
        if ((x = env_get ("DEBUG_LEVEL")))
            debug_level = atol (x);
    warnx ("DEBUG_LEVEL set to `%d'", debug_level);

    if ((x = env_get ("DATALIMIT")))
    {
        struct rlimit r;
        unsigned long dlimit = atol (x);

        if (getrlimit (RLIMIT_DATA,  &r) != 0)
            err (-1, "could not get resource RLIMIT_DATA");

        r.rlim_cur = (dlimit <= r.rlim_max) ? dlimit : r.rlim_max;

        if (setrlimit (RLIMIT_DATA, &r) != 0)
            err (-1, "could not set resource RLIMIT_DATA");

        if (debug_level)
            warnx ("DATALIMIT set to `%ld' bytes", r.rlim_cur);
    }

    if (!(x = env_get ("IP")))
        err (-1, "$IP not set");
    for (i = 0; (unsigned)i < strlen (x); i++)
        n = (x[i] == ',') ? n+1 : n;
    if (!(udp53 = calloc (n+1, sizeof (int))))
        err (-1, "could not allocate enough memory for udp53");
    if (!(iop = calloc (n+1, sizeof (iopause_fd))))
        err (-1, "could not allocate enough memory for iop");

    i = n = 0;
    while (x[i])
    {
        unsigned int l = 0;

        if (!(l = ip4_scan(x+i, ip)))
            errx (-1, "could not parse IP address `%s'", x + i);

        udp53[n] = socket_udp();
        if (udp53[n] == -1)
            errx (-1, "could not open UDP socket");
        if (socket_bind4_reuse (udp53[n], ip, server_port) == -1)
            errx (-1, "could not bind UDP socket");

        ndelay_off (udp53[n]);
        socket_tryreservein (udp53[n], 65536);

        iop[n].fd = udp53[n];
        iop[n].events = IOPAUSE_READ;

        n++;
        i += (x[i + l] == ',') ? l + 1 : l;
    }

    droproot ();
    while (1)
    {
        struct taia stamp;
        struct in_addr odst; /* original destination IP */
        struct taia deadline;

        taia_now (&stamp);
        taia_uint (&deadline, 300);
        taia_add (&deadline, &deadline, &stamp);
        iopause (iop, n, &deadline, &stamp);

        for (i = 0; i < n; i++)
        {
            if (!iop[i].revents)
                continue;

            len = socket_recv4 (udp53[i], buf, sizeof (buf), ip, &port, &odst);
            if (len < 0)
                continue;
            if (!doit ())
                continue;
            if (response_len > 512)
                response_tc ();

            /* may block for buffer space; if it fails, too bad */
            len = socket_send4 (udp53[i], response,
                                response_len, ip, port, &odst);
            if (len < 0)
                continue;
            if (debug_level > 1)
                log_querydone(qnum, response, response_len);
        }
    }

    return 0;
}
