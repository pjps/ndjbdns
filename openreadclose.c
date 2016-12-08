/*
 * openreadclose.c: This file is part of the `djbdns' project, originally
 * written by Dr. D J Bernstein and later released under public-domain since
 * late December 2007 (http://cr.yp.to/distributors.html).
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

#include "open.h"
#include "error.h"
#include "readclose.h"
#include "openreadclose.h"

int
openreadclose (const char *fn, stralloc *sa, unsigned int bufsize)
{
    int fd = open_read (fn);
    if (fd == -1)
    {
        if (errno == error_noent)
            return 0;
        return -1;
    }
    if (readclose (fd, sa, bufsize) == -1)
        return -1;
    return 1;
}
