/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)tcp_input.c	8.12 (Berkeley) 5/24/95
 */

#include <sys/bsd_cdefs.h>
//__FBSDID("$FreeBSD$");

#include "bsd_opt_inet.h"
#include "bsd_opt_inet6.h"
#include "bsd_opt_tcpdebug.h"

#include <sys/bsd_param.h>
#include <sys/bsd_kernel.h>
#include <sys/bsd_malloc.h>
#include <sys/bsd_mbuf.h>
#include <sys/bsd_socket.h>
#include <sys/bsd_socketvar.h>
//baoyg//#include <sys/bsd_sysctl.h>
#include <sys/bsd_syslog.h>
#include <sys/bsd_systm.h>

#include <vm/bsd_uma.h>

#include <net/bsd_if.h>
#include <net/bsd_route.h>
#include <net/bsd_vnet.h>

#include <netinet/bsd_in.h>
#include <netinet/bsd_in_pcb.h>
#include <netinet/bsd_in_systm.h>
#include <netinet/bsd_in_var.h>
#include <netinet/bsd_ip.h>
#include <netinet/bsd_ip_var.h>
#include <netinet/bsd_ip_options.h>
#include <netinet/bsd_ip6.h>
#include <netinet6/bsd_in6_pcb.h>
#include <netinet6/bsd_ip6_var.h>
#include <netinet6/bsd_nd6.h>
#include <netinet/bsd_tcp.h>
#include <netinet/bsd_tcp_fsm.h>
#include <netinet/bsd_tcp_seq.h>
#include <netinet/bsd_tcp_timer.h>
#include <netinet/bsd_tcp_var.h>
#include <netinet6/bsd_tcp6_var.h>
#include <netinet/bsd_tcpip.h>
#ifdef TCPDEBUG
#include <netinet/bsd_tcp_debug.h>
#endif /* TCPDEBUG */

static VNET_DEFINE(int, tcp_reass_maxseg);
VNET_DEFINE(int, tcp_reass_qsize);
static VNET_DEFINE(int, tcp_reass_maxqlen);
static VNET_DEFINE(int, tcp_reass_overflows);

#define	V_tcp_reass_maxseg		VNET(tcp_reass_maxseg)
#define	V_tcp_reass_maxqlen		VNET(tcp_reass_maxqlen)
#define	V_tcp_reass_overflows		VNET(tcp_reass_overflows)
/*
SYSCTL_NODE(_net_inet_tcp, OID_AUTO, reass, CTLFLAG_RW, 0,
    "TCP Segment Reassembly Queue");

SYSCTL_VNET_INT(_net_inet_tcp_reass, OID_AUTO, maxsegments, CTLFLAG_RDTUN,
    &VNET_NAME(tcp_reass_maxseg), 0,
    "Global maximum number of TCP Segments in Reassembly Queue");

SYSCTL_VNET_INT(_net_inet_tcp_reass, OID_AUTO, cursegments, CTLFLAG_RD,
    &VNET_NAME(tcp_reass_qsize), 0,
    "Global number of TCP Segments currently in Reassembly Queue");

SYSCTL_VNET_INT(_net_inet_tcp_reass, OID_AUTO, maxqlen, CTLFLAG_RW,
    &VNET_NAME(tcp_reass_maxqlen), 0,
    "Maximum number of TCP Segments per individual Reassembly Queue");

SYSCTL_VNET_INT(_net_inet_tcp_reass, OID_AUTO, overflows, CTLFLAG_RD,
    &VNET_NAME(tcp_reass_overflows), 0,
    "Global number of TCP Segment Reassembly Queue Overflows");
*/
#if 0
/* Initialize TCP reassembly queue */
static void
tcp_reass_zone_change(void *tag)
{

	V_tcp_reass_maxseg = nmbclusters / 16;
	uma_zone_set_max(V_tcp_reass_zone, V_tcp_reass_maxseg);
}
#endif

VNET_DEFINE(uma_zone_t, tcp_reass_zone);

void
tcp_reass_init(void)
{

	V_tcp_reass_maxseg = 0;
	V_tcp_reass_qsize = 0;
	V_tcp_reass_maxqlen = 48;
	V_tcp_reass_overflows = 0;

	V_tcp_reass_maxseg = nmbclusters / 16;
	//TUNABLE_INT_FETCH("net.inet.tcp.reass.maxsegments",
	//    &V_tcp_reass_maxseg);
	V_tcp_reass_zone = uma_zcreate("tcpreass", sizeof (struct tseg_qent),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	uma_zone_set_max(V_tcp_reass_zone, V_tcp_reass_maxseg);
//	EVENTHANDLER_REGISTER(nmbclusters_change,
//	    tcp_reass_zone_change, NULL, EVENTHANDLER_PRI_ANY);
}

int
tcp_reass(struct tcpcb *tp, struct tcphdr *th, int *tlenp, struct mbuf *m)
{
	struct tseg_qent *q;
	struct tseg_qent *p = NULL;
	struct tseg_qent *nq;
	struct tseg_qent *te = NULL;
	struct socket *so = tp->t_inpcb->inp_socket;
	int flags;

	INP_WLOCK_ASSERT(tp->t_inpcb);

	/*
	 * XXX: tcp_reass() is rather inefficient with its data structures
	 * and should be rewritten (see NetBSD for optimizations).
	 */

	/*
	 * Call with th==NULL after become established to
	 * force pre-ESTABLISHED data up to user socket.
	 */
	if (th == NULL)
		goto present;

	/*
	 * Limit the number of segments in the reassembly queue to prevent
	 * holding on to too many segments (and thus running out of mbufs).
	 * Make sure to let the missing segment through which caused this
	 * queue.  Always keep one global queue entry spare to be able to
	 * process the missing segment.
	 */
	if (th->th_seq != tp->rcv_nxt &&
	    (V_tcp_reass_qsize + 1 >= V_tcp_reass_maxseg ||
	     tp->t_segqlen >= V_tcp_reass_maxqlen)) {
		V_tcp_reass_overflows++;
		TCPSTAT_INC(tcps_rcvmemdrop);
		m_freem(m);
		*tlenp = 0;
		return (0);
	}

	/*
	 * Allocate a new queue entry. If we can't, or hit the zone limit
	 * just drop the pkt.
	 */
	te = uma_zalloc(V_tcp_reass_zone, M_NOWAIT);
	if (te == NULL) {
		TCPSTAT_INC(tcps_rcvmemdrop);
		m_freem(m);
		*tlenp = 0;
		return (0);
	}
	tp->t_segqlen++;
	V_tcp_reass_qsize++;

	/*
	 * Find a segment which begins after this one does.
	 */
	LIST_FOREACH(q, &tp->t_segq, tqe_q) {
		if (SEQ_GT(q->tqe_th->th_seq, th->th_seq))
			break;
		p = q;
	}

	/*
	 * If there is a preceding segment, it may provide some of
	 * our data already.  If so, drop the data from the incoming
	 * segment.  If it provides all of our data, drop us.
	 */
	if (p != NULL) {
		int i;
		/* conversion to int (in i) handles seq wraparound */
		i = p->tqe_th->th_seq + p->tqe_len - th->th_seq;
		if (i > 0) {
			if (i >= *tlenp) {
				TCPSTAT_INC(tcps_rcvduppack);
				TCPSTAT_ADD(tcps_rcvdupbyte, *tlenp);
				m_freem(m);
				uma_zfree(V_tcp_reass_zone, te);
				tp->t_segqlen--;
				V_tcp_reass_qsize--;
				/*
				 * Try to present any queued data
				 * at the left window edge to the user.
				 * This is needed after the 3-WHS
				 * completes.
				 */
				goto present;	/* ??? */
			}
			m_adj(m, i);
			*tlenp -= i;
			th->th_seq += i;
		}
	}
	TCPSTAT_INC(tcps_rcvoopack);
	TCPSTAT_ADD(tcps_rcvoobyte, *tlenp);

	/*
	 * While we overlap succeeding segments trim them or,
	 * if they are completely covered, dequeue them.
	 */
	while (q) {
		int i = (th->th_seq + *tlenp) - q->tqe_th->th_seq;
		if (i <= 0)
			break;
		if (i < q->tqe_len) {
			q->tqe_th->th_seq += i;
			q->tqe_len -= i;
			m_adj(q->tqe_m, i);
			break;
		}

		nq = LIST_NEXT(q, tqe_q);
		LIST_REMOVE(q, tqe_q);
		m_freem(q->tqe_m);
		uma_zfree(V_tcp_reass_zone, q);
		tp->t_segqlen--;
		V_tcp_reass_qsize--;
		q = nq;
	}

	/* Insert the new segment queue entry into place. */
	te->tqe_m = m;
	te->tqe_th = th;
	te->tqe_len = *tlenp;

	if (p == NULL) {
		LIST_INSERT_HEAD(&tp->t_segq, te, tqe_q);
	} else {
		LIST_INSERT_AFTER(p, te, tqe_q);
	}

present:
	/*
	 * Present data to user, advancing rcv_nxt through
	 * completed sequence space.
	 */
	if (!TCPS_HAVEESTABLISHED(tp->t_state))
		return (0);
	q = LIST_FIRST(&tp->t_segq);
	if (!q || q->tqe_th->th_seq != tp->rcv_nxt)
		return (0);
	SOCKBUF_LOCK(&so->so_rcv);
	do {
		tp->rcv_nxt += q->tqe_len;
		flags = q->tqe_th->th_flags & TH_FIN;
		nq = LIST_NEXT(q, tqe_q);
		LIST_REMOVE(q, tqe_q);
		if (so->so_rcv.sb_state & SBS_CANTRCVMORE)
			m_freem(q->tqe_m);
		else
			sbappendstream_locked(&so->so_rcv, q->tqe_m);
		uma_zfree(V_tcp_reass_zone, q);
		tp->t_segqlen--;
		V_tcp_reass_qsize--;
		q = nq;
	} while (q && q->tqe_th->th_seq == tp->rcv_nxt);
	ND6_HINT(tp);
	sorwakeup_locked(so);
	return (flags);
}
