/**************************************************************************

Copyright (c) 2008-2009, BitGravity Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the BitGravity Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

$FreeBSD: src/sys/net/flowtable.h,v 1.10.2.3.2.1 2009/10/25 01:10:29 kensmith Exp $

***************************************************************************/

#ifndef	_BSD_NET_FLOWTABLE_H_
#define	_BSD_NET_FLOWTABLE_H_

#ifdef	_FREEBSD_KERNEL

#define	FL_HASH_PORTS	(1<<0)	/* hash 4-tuple + protocol */
#define	FL_PCPU		(1<<1)	/* pcpu cache */

struct flowtable;
VNET_DECLARE(struct flowtable *, ip_ft);
#define	V_ip_ft			VNET(ip_ft)

struct flowtable *flowtable_alloc(int nentry, int flags);

/*
 * Given a flow table, look up the L3 and L2 information and
 * return it in the route.
 *
 */
int flowtable_lookup(struct flowtable *ft, struct mbuf *m,
    struct route *ro, uint32_t fibnum);

void flowtable_route_flush(struct flowtable *ft, struct rtentry *rt);

#endif /* _FREEBSD_KERNEL */
#endif
