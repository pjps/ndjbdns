/*
 * common.c: This file is part of the `djbdns' project, originally written
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

#define _GNU_SOURCE

#include <err.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <netinet/in.h>

#include "taia.h"
#include "uint32.h"

#define free(ptr)   free ((ptr)); (ptr) = NULL

extern short mode, debug_level;

#ifndef __USE_GNU

#include <sys/stat.h>

ssize_t
extend_buffer (char **buf)
{
    ssize_t n = 128;
    char *newbuf = NULL;

    if (*buf)
        n += strlen (*buf);

    if (!(newbuf = calloc (n, sizeof (char))))
        err (-1, "could not allocate enough memory");

    if (*buf)
    {
        strncpy (newbuf, *buf, n);
        free (*buf);
    }

    *buf = newbuf;
    return n;
}

#if defined(__APPLE__) && defined(__MACH__)

/*      $OpenBSD: memrchr.c,v 1.2 2007/11/27 16:22:12 martynas Exp $    */

/*
 * Copyright (c) 2007 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $FreeBSD$
 */

#include <string.h>

/*
 * Reverse memchr()
 * Find the last occurrence of 'c' in the buffer 's' of size 'n'.
 */
void *
memrchr(const void *s, int c, size_t n)
{
        const unsigned char *cp;

        if (n != 0) {
                cp = (unsigned char *)s + n;
                do {
                        if (*(--cp) == (unsigned char)c)
                                return((void *)cp);
                } while (--n != 0);
        }
        return(NULL);
}

#elif

size_t
getline (char **lineptr, ssize_t *n, FILE *stream)
{
    assert (stream != NULL);

    int i = 0, c = 0;
    char *buf = *lineptr;

    while ((c = fgetc (stream)) != EOF)
    {
        if (!buf || i + 1 == *n)
            *n = extend_buffer (&buf);

        buf[i++] = c;
        if (c == '\n' || c == '\0')
            break;
    }
    *lineptr = buf;

    if (c == EOF)
        i = -1;

    return i;
}

#endif

#endif      /* #ifndef __USE_GNU */


uint32 seed[32];
int seedpos = 0;

void
seed_adduint32 (uint32 u)
{
    int i = 0;

    seed[seedpos] += u;
    if (++seedpos == 32)
    {
        for (i = 0; i < 32; ++i)
        {
            u = ((u ^ seed[i]) + 0x9e3779b9) ^ (u << 7) ^ (u >> 25);
            seed[i] = u;
        }
        seedpos = 0;
    }
}

void
seed_addtime (void)
{
    int i = 0;
    struct taia t;
    char tpack[TAIA_PACK];

    taia_now (&t);
    taia_pack (tpack, &t);
    for (i = 0; i < TAIA_PACK; ++i)
        seed_adduint32 (tpack[i]);
}


/*
 * strtrim: removes leading & trailing white spaces(space, tab, new-line, etc)
 * from a given character string and returns a pointer to the new string.
 * Do free(3) it later.
 */
char *
strtrim (const char *s)
{
    if (s == NULL)
        return NULL;

    const char *e = &s[strlen(s) - 1];

    while (*s)
    {
        if (isspace (*s))
            s++;
        else
            break;
    }

    while (*e)
    {
        if (isspace (*e))
            e--;
        else
            break;
    }
    e++;

    return strndup (s, e - s);
}


/* checks if the given variable is valid & used by dnscache. */
int
check_variable (const char *var)
{
    assert (var != NULL);

    int i = 0, l = 0;
    const char *known_variable[] = \
    {
        "AXFR", "DATALIMIT", "CACHESIZE", "IP", "IPSEND",
        "UID", "GID", "ROOT", "HIDETTL", "FORWARDONLY",
        "MERGEQUERIES", "DEBUG_LEVEL", "BASE", "TCPREMOTEIP",
        "TCPREMOTEPORT"
    };

    l = sizeof (known_variable) / sizeof (*known_variable);
    for (i = 0; i < l; i++)
    {
        if (strlen (var) != strlen (known_variable[i]))
            continue;
        if (!memcmp (var, known_variable[i], strlen (var)))
            return 1;
    }

    return 0;
}


void
read_conf (const char *file)
{
    assert (file != NULL);

    int lcount = 0;
    FILE *fp = NULL;
    size_t l = 0, n = 0;
    char *line = NULL, *key = NULL, *val = NULL;

    if (!(fp = fopen (file, "r")))
        err (-1, "could not open file `%s'", file);

    while ((signed)(n = getline (&line, &l, fp)) != -1)
    {
        lcount++;
        line[n - 1] = '\0';
        char *s = strtrim (line);
        if (*s && *s != '#' && *s != '\n')
        {
            key = strtrim (strtok (s, "="));
            if (!check_variable (key))
                errx (-1, "%s: %d: unknown variable `%s'", file, lcount, key);

            val = strtrim (strtok (NULL, "="));
            if (debug_level)
                warnx ("%s: %s", key, val);

            if (val)
            {
                setenv (key, val, 1);
                free (val);
            }

            free (s);
            free (key);
            free (line);
        }
        seed_addtime ();
    }

    fclose (fp);
}

/*
 * redirect stdout & stderr to a log file. flag parameter decides which
 * descriptors to redirect; It is an 'OR' of STDOUT_FILENO & STDERR_FILENO.
 */
void
redirect_to_log (const char *logfile, unsigned char flag)
{
    assert (logfile != NULL);

    int fd = 0, perm = S_IRUSR | S_IWUSR;

    if ((fd = open (logfile, O_CREAT | O_WRONLY | O_APPEND, perm)) == -1)
        err (-1, "could not open logfile `%s'", logfile);

    if (flag & STDOUT_FILENO && dup2 (fd, STDOUT_FILENO) == -1)
        err (-1, "could not duplicate stdout");
    if (flag & STDERR_FILENO && dup2 (fd, STDERR_FILENO) == -1)
        err (-1, "could not duplicate stderr");
}

/*
 * wirets pid to a file under /var/run directory, which will be used by
 * /sbin/service to shut down the dns daemon.
 */
void
write_pid (const char *pidfile)
{
    int n = 0, fd = 0, perm = 0;
    char *pid = strdup (pidfile);

    perm = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    if ((fd = open (pid, O_CREAT | O_WRONLY | O_TRUNC, perm)) == -1)
        err (-1, "could not open file: `%s'", pid);

    memset (pid, '\0', strlen (pid));
    n = sprintf (pid, "%d\n", getpid ());
    write (fd, pid, n);

    close (fd);
    free (pid);
}

void
handle_term (int n)
{
    warnx ("going down with signal: %d ---\n", n);
    exit (0);
}

/* gettimezone: reads local timezone definition from '/etc/localtime'
 * and returns a pointer to a POSIX TZ environment variable string or
 * NULL in case of an error; See: tzfile(5), tzset(3).
 *
 *     std offset dst [offset],start[/time],end[/time]
 *
 * Ex: TZ="NZST-12:00:00NZDT-13:00:00,M10.1.0,M3.3.0"
 */
char *
gettimezone (void)
{
#define TZ_VERSION  '2'
#define TZ_MAGIC    "TZif"
#define TZ_FILE     "/etc/localtime"

    int32_t fd = 0;
    char *tz = NULL;

    struct stat st;
    char *tzbuf = NULL;

    if ((fd = open (TZ_FILE, O_RDONLY | O_NDELAY)) < 0)
    {
        warn ("could not access timezone: %s", TZ_FILE);
        return tz;
    }
    if (fstat (fd, &st) < 0)
        err (-1, "could not get file status: %s", TZ_FILE);

    tzbuf = mmap (NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (tzbuf == MAP_FAILED)
        err (-1, "could not mmap(2) file: %s", TZ_FILE);

    if (memcmp (tzbuf, TZ_MAGIC, 4))
        err (-1, "invalid timezone file: %s, see tzfile(5)", TZ_FILE);
    if (TZ_VERSION == *(tzbuf + 4) || TZ_VERSION == *(tzbuf + 4) - 1)
    {
        char *p1 = NULL, *p2 = NULL;

        p1 = p2 = tzbuf + st.st_size - 1;
        while (*--p1 != '\n');
        if (!(tz = calloc (abs (p2 - p1), sizeof (char))))
            err (-1, "could not allocate memory for tz");
        memcpy (tz, p1 + 1, abs (p2 - p1) - 1);
    }

    munmap (tzbuf, st.st_size);
    close (fd);

    return tz;
}

/*
 * set_timezone: set `TZ' environment variable to appropriate time zone value.
 * `TZ' environment variable is used by numerous - <time.h> - functions to
 * perform local time conversions. TZ: NAME[+-]HH:MM:SS[DST]
 * ex: IST-5:30:00, EST+5:00:00EDT etc.
 */
void
set_timezone (void)
{
    char *tzone = getenv ("TZ");
    if (tzone)
        return;

    tzone = gettimezone ();
    if (!tzone)
    {
        time_t t;
        struct tm *tt = NULL;
        char hh = 0, mm = 0, ss = 0;

        t = time (NULL);
        tt = localtime (&t);

        hh = timezone / (60 * 60);
        mm = abs (timezone % (60 * 60) / 60);
        ss = abs (timezone % (60 * 60) % 60);

        if (!(tzone = calloc (22, sizeof (char))))
            err (-1, "could not allocate memory for tzone");
        snprintf (tzone, 22, "%s%+02d:%02d:%02d%s", tzname[0],
                            hh, mm, ss, (tt->tm_isdst > 0) ? tzname[1] : "");
    }

    setenv ("TZ", tzone, 1);
    free (tzone);
}
