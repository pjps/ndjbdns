/*
 * socket.h: This file is part of the `djbdns' project, originally
 * written by Dr. D J Bernstein and later released under public-domain
 * since late December 2007 (http://cr.yp.to/distributors.html).
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

#include "uint16.h"

extern int socket_tcp (void);

extern int socket_udp (void);

extern int socket_connected (int);

extern int socket_listen (int, int);

extern void socket_tryreservein (int, int);

extern int socket_bind4 (int, char *, uint16);

extern int socket_local4 (int, char *, uint16 *);

extern int socket_accept4 (int, char *, uint16 *);

extern int socket_remote4 (int, char *, uint16 *);

extern int socket_bind4_reuse (int, char *, uint16);

extern int socket_connect4 (int, const char *, uint16);

extern int socket_recv4 (int, char *, int, char *, uint16 *, void *);

extern int socket_send4 (int, char *, int, const char *, uint16, void *);
