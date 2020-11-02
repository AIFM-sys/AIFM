/*-
 * Copyright (c) 2007, Chelsio Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Neither the name of the Chelsio Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/bsd_cdefs.h>
//__FBSDID("$FreeBSD$");

#include <sys/bsd_param.h>
#include <sys/bsd_systm.h>
#include <sys/bsd_types.h>
#include <sys/bsd_malloc.h>
#include <sys/bsd_kernel.h>
//baoyg//#include <sys/bsd_sysctl.h>
#include <sys/bsd_mbuf.h>
#include <sys/bsd_socket.h>
#include <sys/bsd_socketvar.h>

#include <net/bsd_if.h>
#include <net/bsd_if_types.h>
#include <net/bsd_if_var.h>
#include <net/bsd_route.h>
#include <net/bsd_vnet.h>

#include <netinet/bsd_in.h>
#include <netinet/bsd_in_systm.h>
#include <netinet/bsd_in_pcb.h>
#include <netinet/bsd_tcp.h>
#include <netinet/bsd_tcp_var.h>
#include <netinet/bsd_tcp_offload.h>
#include <netinet/bsd_toedev.h>

uint32_t toedev_registration_count;

int
tcp_offload_connect(struct socket *so, struct sockaddr *nam)
{
	struct ifnet *ifp;
	struct toedev *tdev;
	struct rtentry *rt;
	int error;

	if (toedev_registration_count == 0)
		return (EINVAL);
	
	/*
	 * Look up the route used for the connection to 
	 * determine if it uses an interface capable of
	 * offloading the connection.
	 */
	rt = rtalloc1(nam, 0 /*report*/, 0 /*ignflags*/);
	if (rt) 
		RT_UNLOCK(rt);
	else 
		return (EHOSTUNREACH);

	ifp = rt->rt_ifp;
	if ((ifp->if_capenable & IFCAP_TOE) == 0) {
		error = EINVAL;
		goto fail;
	}
	
	tdev = TOEDEV(ifp);
	if (tdev == NULL) {
		error = EPERM;
		goto fail;
	}
	
	if (tdev->tod_can_offload(tdev, so) == 0) {
		error = EPERM;
		goto fail;
	}
	
	return (tdev->tod_connect(tdev, so, rt, nam));
fail:
	RTFREE(rt);
	return (error);
}


/*
 * This file contains code as a short-term staging area before it is moved in 
 * to sys/netinet/tcp_offload.c
 */

void
tcp_offload_twstart(struct tcpcb *tp)
{

	INP_INFO_WLOCK(&V_tcbinfo);
	INP_WLOCK(tp->t_inpcb);
	tcp_twstart(tp);
	INP_INFO_WUNLOCK(&V_tcbinfo);
}

struct tcpcb *
tcp_offload_close(struct tcpcb *tp)
{

	INP_INFO_WLOCK(&V_tcbinfo);
	INP_WLOCK(tp->t_inpcb);
	tp = tcp_close(tp);
	INP_INFO_WUNLOCK(&V_tcbinfo);
	if (tp)
		INP_WUNLOCK(tp->t_inpcb);

	return (tp);
}

struct tcpcb *
tcp_offload_drop(struct tcpcb *tp, int error)
{

	INP_INFO_WLOCK(&V_tcbinfo);
	INP_WLOCK(tp->t_inpcb);
	tp = tcp_drop(tp, error);
	INP_INFO_WUNLOCK(&V_tcbinfo);
	if (tp)
		INP_WUNLOCK(tp->t_inpcb);

	return (tp);
}

