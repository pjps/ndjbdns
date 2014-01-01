/*
 * error.h: This file is part of the `djbdns' project, originally written
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

#pragma once 

#if defined (linux) || defined (__linux)
    #include <errno.h>
#endif

extern int errno;

extern int error_io;
extern int error_intr;
extern int error_pipe;
extern int error_perm;
extern int error_acces;
extern int error_proto;
extern int error_isdir;
extern int error_nomem;
extern int error_noent;
extern int error_exist;
extern int error_again;
extern int error_txtbsy;
extern int error_timeout;
extern int error_nodevice;
extern int error_inprogress;
extern int error_wouldblock;
extern int error_connrefused;
extern int error_blockedbydbl;

extern int error_temp (int);

extern const char *error_str (int);
