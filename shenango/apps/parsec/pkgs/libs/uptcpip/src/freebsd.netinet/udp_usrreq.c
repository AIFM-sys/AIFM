/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1995
 *	The Regents of the University of California.
 * Copyright (c) 2008 Robert N. M. Watson
 * All rights reserved.
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
 *	@(#)udp_usrreq.c	8.6 (Berkeley) 5/23/95
 */

#include <sys/bsd_cdefs.h>
//__FBSDID("$FreeBSD$");

#include "bsd_opt_ipfw.h"
#include "bsd_opt_inet6.h"
#include "bsd_opt_ipsec.h"

#include <sys/bsd_param.h>
#include <sys/bsd_domain.h>
//#include <sys/bsd_eventhandler.h>
//#include <sys/bsd_jail.h>
#include <sys/bsd_kernel.h>
#include <sys/bsd_lock.h>
#include <sys/bsd_malloc.h>
#include <sys/bsd_mbuf.h>
//#include <sys/bsd_priv.h>
////#include <sys/bsd_proc.h>
#include <sys/bsd_protosw.h>
//#include <sys/bsd_signalvar.h>
#include <sys/bsd_socket.h>
#include <sys/bsd_socketvar.h>
//#include <sys/bsd_sx.h>
//#include <sys/bsd_sysctl.h>
#include <sys/bsd_syslog.h>
#include <sys/bsd_systm.h>

#include <vm/bsd_uma.h>

#include <net/bsd_if.h>
#include <net/bsd_route.h>

#include <netinet/bsd_in.h>
#include <netinet/bsd_in_pcb.h>
#include <netinet/bsd_in_systm.h>
#include <netinet/bsd_in_var.h>
#include <netinet/bsd_ip.h>
#ifdef INET6
#include <netinet/bsd_ip6.h>
#endif
#include <netinet/bsd_ip_icmp.h>
#include <netinet/bsd_icmp_var.h>
#include <netinet/bsd_ip_var.h>
#include <netinet/bsd_ip_options.h>
#ifdef INET6
#include <netinet6/bsd_ip6_var.h>
#endif
#include <netinet/bsd_udp.h>
#include <netinet/bsd_udp_var.h>

#ifdef IPSEC
#include <netipsec/bsd_ipsec.h>
#include <netipsec/bsd_esp.h>
#endif

#include <machine/bsd_in_cksum.h>

#ifdef MAC
#include <security/mac/bsd_mac_framework.h>
#endif

/*
 * UDP protocol implementation.
 * Per RFC 768, August, 1980.
 */

VNET_DEFINE(int, udp_blackhole);

/*
 * BSD 4.2 defaulted the udp checksum to be off.  Turning off udp checksums
 * removes the only data integrity mechanism for packets and malformed
 * packets that would otherwise be discarded due to bad checksums, and may
 * cause problems (especially for NFS data blocks).
 */
static int	udp_cksum = 1;
/*SYSCTL_INT(_net_inet_udp, UDPCTL_CHECKSUM, checksum, CTLFLAG_RW, &udp_cksum,
    0, "compute udp checksum");
*/
int	udp_log_in_vain = 0;
/*
SYSCTL_INT(_net_inet_udp, OID_AUTO, log_in_vain, CTLFLAG_RW,
    &udp_log_in_vain, 0, "Log all incoming UDP packets");

SYSCTL_VNET_INT(_net_inet_udp, OID_AUTO, blackhole, CTLFLAG_RW,
    &VNET_NAME(udp_blackhole), 0,
    "Do not send port unreachables for refused connects");
*/
u_long	udp_sendspace = 9216;		/* really max datagram size */
					/* 40 1K datagrams */
/*SYSCTL_ULONG(_net_inet_udp, UDPCTL_MAXDGRAM, maxdgram, CTLFLAG_RW,
    &udp_sendspace, 0, "Maximum outgoing UDP datagram size");
*/
u_long	udp_recvspace = 40 * (1024 +
#ifdef INET6
				      sizeof(struct sockaddr_in6)
#else
				      sizeof(struct sockaddr_in)
#endif
				      );
/*
SYSCTL_ULONG(_net_inet_udp, UDPCTL_RECVSPACE, recvspace, CTLFLAG_RW,
    &udp_recvspace, 0, "Maximum space for incoming UDP datagrams");
*/
VNET_DEFINE(struct inpcbhead, udb);		/* from udp_var.h */
VNET_DEFINE(struct inpcbinfo, udbinfo);
static VNET_DEFINE(uma_zone_t, udpcb_zone);
VNET_DEFINE(struct udpstat, udpstat);		/* from udp_var.h */

#define	V_udpcb_zone			VNET(udpcb_zone)

#ifndef UDBHASHSIZE
#define	UDBHASHSIZE	128
#endif
/*
SYSCTL_VNET_STRUCT(_net_inet_udp, UDPCTL_STATS, stats, CTLFLAG_RW,
    &VNET_NAME(udpstat), udpstat,
    "UDP statistics (struct udpstat, netinet/udp_var.h)");
*/
static void	udp_detach(struct socket *so);
static int	udp_output(struct inpcb *, struct mbuf *, struct sockaddr *,
		    struct mbuf *, struct thread *);
#ifdef IPSEC
#ifdef IPSEC_NAT_T
#define	UF_ESPINUDP_ALL	(UF_ESPINUDP_NON_IKE|UF_ESPINUDP)
#ifdef INET
static struct mbuf *udp4_espdecap(struct inpcb *, struct mbuf *, int);
#endif
#endif /* IPSEC_NAT_T */
#endif /* IPSEC */

#if 0
static void
udp_zone_change(void *tag)
{

	uma_zone_set_max(V_udbinfo.ipi_zone, maxsockets);
	uma_zone_set_max(V_udpcb_zone, maxsockets);
}
#endif //0

static int
udp_inpcb_init(void *mem, int size, int flags)
{
	struct inpcb *inp;

	inp = mem;
	INP_LOCK_INIT(inp, "inp", "udpinp");
	return (0);
}

void
udp_init(void)
{

	V_udp_blackhole = 0;

	INP_INFO_LOCK_INIT(&V_udbinfo, "udp");
	LIST_INIT(&V_udb);
#ifdef VIMAGE
	V_udbinfo.ipi_vnet = curvnet;
#endif
	V_udbinfo.ipi_listhead = &V_udb;
	V_udbinfo.ipi_hashbase = hashinit(UDBHASHSIZE, M_PCB,
	    &V_udbinfo.ipi_hashmask);
	V_udbinfo.ipi_porthashbase = hashinit(UDBHASHSIZE, M_PCB,
	    &V_udbinfo.ipi_porthashmask);
	V_udbinfo.ipi_zone = uma_zcreate("udp_inpcb", sizeof(struct inpcb),
	    NULL, NULL, udp_inpcb_init, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	uma_zone_set_max(V_udbinfo.ipi_zone, maxsockets);

	V_udpcb_zone = uma_zcreate("udpcb", sizeof(struct udpcb),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	uma_zone_set_max(V_udpcb_zone, maxsockets);

//	EVENTHANDLER_REGISTER(maxsockets_change, udp_zone_change, NULL,
//	    EVENTHANDLER_PRI_ANY);
}

/*
 * Kernel module interface for updating udpstat.  The argument is an index
 * into udpstat treated as an array of u_long.  While this encodes the
 * general layout of udpstat into the caller, it doesn't encode its location,
 * so that future changes to add, for example, per-CPU stats support won't
 * cause binary compatibility problems for kernel modules.
 */
void
kmod_udpstat_inc(int statnum)
{

	(*((u_long *)&V_udpstat + statnum))++;
}

int
udp_newudpcb(struct inpcb *inp)
{
	struct udpcb *up;

	up = uma_zalloc(V_udpcb_zone, M_NOWAIT | M_ZERO);
	if (up == NULL)
		return (ENOBUFS);
	inp->inp_ppcb = up;
	return (0);
}

void
udp_discardcb(struct udpcb *up)
{

	uma_zfree(V_udpcb_zone, up);
}

#ifdef VIMAGE
void
udp_destroy(void)
{

	hashdestroy(V_udbinfo.ipi_hashbase, M_PCB,
	    V_udbinfo.ipi_hashmask);
	hashdestroy(V_udbinfo.ipi_porthashbase, M_PCB,
	    V_udbinfo.ipi_porthashmask);
	INP_INFO_LOCK_DESTROY(&V_udbinfo);
}
#endif

/*
 * Subroutine of udp_input(), which appends the provided mbuf chain to the
 * passed pcb/socket.  The caller must provide a sockaddr_in via udp_in that
 * contains the source address.  If the socket ends up being an IPv6 socket,
 * udp_append() will convert to a sockaddr_in6 before passing the address
 * into the socket code.
 */
static void
udp_append(struct inpcb *inp, struct ip *ip, struct mbuf *n, int off,
    struct sockaddr_in *udp_in)
{
	struct sockaddr *append_sa;
	struct socket *so;
	struct mbuf *opts = 0;
#ifdef INET6
	struct sockaddr_in6 udp_in6;
#endif
#ifdef IPSEC
#ifdef IPSEC_NAT_T
#ifdef INET
	struct udpcb *up;
#endif
#endif
#endif

	INP_RLOCK_ASSERT(inp);

#ifdef IPSEC
	/* Check AH/ESP integrity. */
	if (ipsec4_in_reject(n, inp)) {
		m_freem(n);
		V_ipsec4stat.in_polvio++;
		return;
	}
#ifdef IPSEC_NAT_T
#ifdef INET
	up = intoudpcb(inp);
	KASSERT(up != NULL, ("%s: udpcb NULL", __func__));
	if (up->u_flags & UF_ESPINUDP_ALL) {	/* IPSec UDP encaps. */
		n = udp4_espdecap(inp, n, off);
		if (n == NULL)				/* Consumed. */
			return;
	}
#endif /* INET */
#endif /* IPSEC_NAT_T */
#endif /* IPSEC */
#ifdef MAC
	if (mac_inpcb_check_deliver(inp, n) != 0) {
		m_freem(n);
		return;
	}
#endif
	if (inp->inp_flags & INP_CONTROLOPTS ||
	    inp->inp_socket->so_options & (SO_TIMESTAMP | SO_BINTIME)) {
#ifdef INET6
		if (inp->inp_vflag & INP_IPV6)
			(void)ip6_savecontrol_v4(inp, n, &opts, NULL);
		else
#endif
//			ip_savecontrol(inp, &opts, ip, n);
			;
	}
#ifdef INET6
	if (inp->inp_vflag & INP_IPV6) {
		bzero(&udp_in6, sizeof(udp_in6));
		udp_in6.sin6_len = sizeof(udp_in6);
		udp_in6.sin6_family = AF_INET6;
		in6_sin_2_v4mapsin6(udp_in, &udp_in6);
		append_sa = (struct sockaddr *)&udp_in6;
	} else
#endif
		append_sa = (struct sockaddr *)udp_in;
	m_adj(n, off);

	so = inp->inp_socket;
	SOCKBUF_LOCK(&so->so_rcv);
	if (sbappendaddr_locked(&so->so_rcv, append_sa, n, opts) == 0) {
		SOCKBUF_UNLOCK(&so->so_rcv);
		m_freem(n);
		if (opts)
			m_freem(opts);
		UDPSTAT_INC(udps_fullsock);
	} else
		sorwakeup_locked(so);
}

void
udp_input(struct mbuf *m, int off)
{
	int iphlen = off;
	struct ip *ip;
	struct udphdr *uh;
	struct ifnet *ifp;
	struct inpcb *inp;
	struct udpcb *up;
	int len;
	struct ip save_ip;
	struct sockaddr_in udp_in;
#ifdef IPFIREWALL_FORWARD
	struct m_tag *fwd_tag;
#endif

	ifp = m->m_pkthdr.rcvif;
	UDPSTAT_INC(udps_ipackets);

	/*
	 * Strip IP options, if any; should skip this, make available to
	 * user, and use on returned packets, but we don't yet have a way to
	 * check the checksum with options still present.
	 */
	if (iphlen > sizeof (struct ip)) {
		ip_stripoptions(m, (struct mbuf *)0);
		iphlen = sizeof(struct ip);
	}

	/*
	 * Get IP and UDP header together in first mbuf.
	 */
	ip = mtod(m, struct ip *);
	if (m->m_len < iphlen + sizeof(struct udphdr)) {
		if ((m = m_pullup(m, iphlen + sizeof(struct udphdr))) == 0) {
			UDPSTAT_INC(udps_hdrops);
			return;
		}
		ip = mtod(m, struct ip *);
	}
	uh = (struct udphdr *)((caddr_t)ip + iphlen);

	/*
	 * Destination port of 0 is illegal, based on RFC768.
	 */
	if (uh->uh_dport == 0)
		goto badunlocked;

	/*
	 * Construct sockaddr format source address.  Stuff source address
	 * and datagram in user buffer.
	 */
	bzero(&udp_in, sizeof(udp_in));
	udp_in.sin_len = sizeof(udp_in);
	udp_in.sin_family = AF_INET;
	udp_in.sin_port = uh->uh_sport;
	udp_in.sin_addr = ip->ip_src;

	/*
	 * Make mbuf data length reflect UDP length.  If not enough data to
	 * reflect UDP length, drop.
	 */
	len = ntohs((u_short)uh->uh_ulen);
	if (ip->ip_len != len) {
		if (len > ip->ip_len || len < sizeof(struct udphdr)) {
			UDPSTAT_INC(udps_badlen);
			goto badunlocked;
		}
		m_adj(m, len - ip->ip_len);
		/* ip->ip_len = len; */
	}

	/*
	 * Save a copy of the IP header in case we want restore it for
	 * sending an ICMP error message in response.
	 */
	if (!V_udp_blackhole)
		save_ip = *ip;
	else
		memset(&save_ip, 0, sizeof(save_ip));

	/*
	 * Checksum extended UDP header and data.
	 */
	if (uh->uh_sum) {
		u_short uh_sum;

		if (m->m_pkthdr.csum_flags & CSUM_DATA_VALID) {
			if (m->m_pkthdr.csum_flags & CSUM_PSEUDO_HDR)
				uh_sum = m->m_pkthdr.csum_data;
			else
				uh_sum = in_pseudo(ip->ip_src.s_addr,
				    ip->ip_dst.s_addr, htonl((u_short)len +
				    m->m_pkthdr.csum_data + IPPROTO_UDP));
			uh_sum ^= 0xffff;
		} else {
			char b[9];

			bcopy(((struct ipovly *)ip)->ih_x1, b, 9);
			bzero(((struct ipovly *)ip)->ih_x1, 9);
			((struct ipovly *)ip)->ih_len = uh->uh_ulen;
			uh_sum = in_cksum(m, len + sizeof (struct ip));
			bcopy(b, ((struct ipovly *)ip)->ih_x1, 9);
		}
		if (uh_sum) {
			UDPSTAT_INC(udps_badsum);
			m_freem(m);
			return;
		}
	} else
		UDPSTAT_INC(udps_nosum);

#ifdef IPFIREWALL_FORWARD
	/*
	 * Grab info from PACKET_TAG_IPFORWARD tag prepended to the chain.
	 */
	fwd_tag = m_tag_find(m, PACKET_TAG_IPFORWARD, NULL);
	if (fwd_tag != NULL) {
		struct sockaddr_in *next_hop;

		/*
		 * Do the hack.
		 */
		next_hop = (struct sockaddr_in *)(fwd_tag + 1);
		ip->ip_dst = next_hop->sin_addr;
		uh->uh_dport = ntohs(next_hop->sin_port);

		/*
		 * Remove the tag from the packet.  We don't need it anymore.
		 */
		m_tag_delete(m, fwd_tag);
	}
#endif

	INP_INFO_RLOCK(&V_udbinfo);
	if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr)) ||
	    in_broadcast(ip->ip_dst, ifp)) {
		struct inpcb *last;
		struct ip_moptions *imo;

		last = NULL;
		LIST_FOREACH(inp, &V_udb, inp_list) {
			if (inp->inp_lport != uh->uh_dport)
				continue;
#ifdef INET6
			if ((inp->inp_vflag & INP_IPV4) == 0)
				continue;
#endif
			if (inp->inp_laddr.s_addr != INADDR_ANY &&
			    inp->inp_laddr.s_addr != ip->ip_dst.s_addr)
				continue;
			if (inp->inp_faddr.s_addr != INADDR_ANY &&
			    inp->inp_faddr.s_addr != ip->ip_src.s_addr)
				continue;
			if (inp->inp_fport != 0 &&
			    inp->inp_fport != uh->uh_sport)
				continue;

			INP_RLOCK(inp);

			/*
			 * Handle socket delivery policy for any-source
			 * and source-specific multicast. [RFC3678]
			 */
			imo = inp->inp_moptions;
			if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr)) &&
			    imo != NULL) {
				struct sockaddr_in	 group;
				int			 blocked;

				bzero(&group, sizeof(struct sockaddr_in));
				group.sin_len = sizeof(struct sockaddr_in);
				group.sin_family = AF_INET;
				group.sin_addr = ip->ip_dst;

				blocked = imo_multi_filter(imo, ifp,
					(struct sockaddr *)&group,
					(struct sockaddr *)&udp_in);
				if (blocked != MCAST_PASS) {
					if (blocked == MCAST_NOTGMEMBER)
						IPSTAT_INC(ips_notmember);
					if (blocked == MCAST_NOTSMEMBER ||
					    blocked == MCAST_MUTED)
						UDPSTAT_INC(udps_filtermcast);
					INP_RUNLOCK(inp);
					continue;
				}
			}
			if (last != NULL) {
				struct mbuf *n;

				n = m_copy(m, 0, M_COPYALL);
				up = intoudpcb(last);
				if (up->u_tun_func == NULL) {
					if (n != NULL)
						udp_append(last, 
						    ip, n, 
						    iphlen +
						    sizeof(struct udphdr),
						    &udp_in);
				} else {
					/*
					 * Engage the tunneling protocol we
					 * will have to leave the info_lock
					 * up, since we are hunting through
					 * multiple UDP's.
					 */

					(*up->u_tun_func)(n, iphlen, last);
				}
				INP_RUNLOCK(last);
			}
			last = inp;
			/*
			 * Don't look for additional matches if this one does
			 * not have either the SO_REUSEPORT or SO_REUSEADDR
			 * socket options set.  This heuristic avoids
			 * searching through all pcbs in the common case of a
			 * non-shared port.  It assumes that an application
			 * will never clear these options after setting them.
			 */
			if ((last->inp_socket->so_options &
			    (SO_REUSEPORT|SO_REUSEADDR)) == 0)
				break;
		}

		if (last == NULL) {
			/*
			 * No matching pcb found; discard datagram.  (No need
			 * to send an ICMP Port Unreachable for a broadcast
			 * or multicast datgram.)
			 */
			UDPSTAT_INC(udps_noportbcast);
			goto badheadlocked;
		}
		up = intoudpcb(last);
		if (up->u_tun_func == NULL) {
			udp_append(last, ip, m, iphlen + sizeof(struct udphdr),
			    &udp_in);
		} else {
			/*
			 * Engage the tunneling protocol.
			 */
			(*up->u_tun_func)(m, iphlen, last);
		}
		INP_RUNLOCK(last);
		INP_INFO_RUNLOCK(&V_udbinfo);
		return;
	}

	/*
	 * Locate pcb for datagram.
	 */
	inp = in_pcblookup_hash(&V_udbinfo, ip->ip_src, uh->uh_sport,
	    ip->ip_dst, uh->uh_dport, 1, ifp);
	if (inp == NULL) {
		if (udp_log_in_vain) {
			char buf[4*sizeof "123"];

			strcpy(buf, inet_ntoa(ip->ip_dst));
			bsd_log(LOG_INFO,
			    "Connection attempt to UDP %s:%d from %s:%d\n",
			    buf, ntohs(uh->uh_dport), inet_ntoa(ip->ip_src),
			    ntohs(uh->uh_sport));
		}
		UDPSTAT_INC(udps_noport);
		if (m->m_flags & (M_BCAST | M_MCAST)) {
			UDPSTAT_INC(udps_noportbcast);
			goto badheadlocked;
		}
		if (V_udp_blackhole)
			goto badheadlocked;
		if (badport_bandlim(BANDLIM_ICMP_UNREACH) < 0)
			goto badheadlocked;
		*ip = save_ip;
		ip->ip_len += iphlen;
		icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_PORT, 0, 0);
		INP_INFO_RUNLOCK(&V_udbinfo);
		return;
	}

	/*
	 * Check the minimum TTL for socket.
	 */
	INP_RLOCK(inp);
	INP_INFO_RUNLOCK(&V_udbinfo);
	if (inp->inp_ip_minttl && inp->inp_ip_minttl > ip->ip_ttl) {
		INP_RUNLOCK(inp);
		goto badunlocked;
	}
	up = intoudpcb(inp);
	if (up->u_tun_func == NULL) {
		udp_append(inp, ip, m, iphlen + sizeof(struct udphdr), &udp_in);
	} else {
		/*
		 * Engage the tunneling protocol.
		 */

		(*up->u_tun_func)(m, iphlen, inp);
	}
	INP_RUNLOCK(inp);
	return;

badheadlocked:
	if (inp)
		INP_RUNLOCK(inp);
	INP_INFO_RUNLOCK(&V_udbinfo);
badunlocked:
	m_freem(m);
}

/*
 * Notify a udp user of an asynchronous error; just wake up so that they can
 * collect error status.
 */
struct inpcb *
udp_notify(struct inpcb *inp, int errorno)
{

	/*
	 * While udp_ctlinput() always calls udp_notify() with a read lock
	 * when invoking it directly, in_pcbnotifyall() currently uses write
	 * locks due to sharing code with TCP.  For now, accept either a read
	 * or a write lock, but a read lock is sufficient.
	 */
	INP_LOCK_ASSERT(inp);

	inp->inp_socket->so_error = errorno;
	sorwakeup(inp->inp_socket);
	sowwakeup(inp->inp_socket);
	return (inp);
}

void
udp_ctlinput(int cmd, struct sockaddr *sa, void *vip)
{
	struct ip *ip = vip;
	struct udphdr *uh;
	struct in_addr faddr;
	struct inpcb *inp;

	faddr = ((struct sockaddr_in *)sa)->sin_addr;
	if (sa->sa_family != AF_INET || faddr.s_addr == INADDR_ANY)
		return;

	/*
	 * Redirects don't need to be handled up here.
	 */
	if (PRC_IS_REDIRECT(cmd))
		return;

	/*
	 * Hostdead is ugly because it goes linearly through all PCBs.
	 *
	 * XXX: We never get this from ICMP, otherwise it makes an excellent
	 * DoS attack on machines with many connections.
	 */
	if (cmd == PRC_HOSTDEAD)
		ip = NULL;
	else if ((unsigned)cmd >= PRC_NCMDS || inetctlerrmap[cmd] == 0)
		return;
	if (ip != NULL) {
		uh = (struct udphdr *)((caddr_t)ip + (ip->ip_hl << 2));
		INP_INFO_RLOCK(&V_udbinfo);
		inp = in_pcblookup_hash(&V_udbinfo, faddr, uh->uh_dport,
		    ip->ip_src, uh->uh_sport, 0, NULL);
		if (inp != NULL) {
			INP_RLOCK(inp);
			if (inp->inp_socket != NULL) {
				udp_notify(inp, inetctlerrmap[cmd]);
			}
			INP_RUNLOCK(inp);
		}
		INP_INFO_RUNLOCK(&V_udbinfo);
	} else
		in_pcbnotifyall(&V_udbinfo, faddr, inetctlerrmap[cmd],
		    udp_notify);
}
/*
static int
udp_pcblist(SYSCTL_HANDLER_ARGS)
{
	int error, i, n;
	struct inpcb *inp, **inp_list;
	inp_gen_t gencnt;
	struct xinpgen xig;

*/	/*
	 * The process of preparing the PCB list is too time-consuming and
	 * resource-intensive to repeat twice on every request.
	 */
/*	if (req->oldptr == 0) {
		n = V_udbinfo.ipi_count;
		req->oldidx = 2 * (sizeof xig)
			+ (n + n/8) * sizeof(struct xinpcb);
		return (0);
	}

	if (req->newptr != 0)
		return (EPERM);

*/	/*
	 * OK, now we're committed to doing something.
	 */
/*	INP_INFO_RLOCK(&V_udbinfo);
	gencnt = V_udbinfo.ipi_gencnt;
	n = V_udbinfo.ipi_count;
	INP_INFO_RUNLOCK(&V_udbinfo);

	error = sysctl_wire_old_buffer(req, 2 * (sizeof xig)
		+ n * sizeof(struct xinpcb));
	if (error != 0)
		return (error);

	xig.xig_len = sizeof xig;
	xig.xig_count = n;
	xig.xig_gen = gencnt;
	xig.xig_sogen = so_gencnt;
	error = SYSCTL_OUT(req, &xig, sizeof xig);
	if (error)
		return (error);

	inp_list = bsd_malloc(n * sizeof *inp_list, M_TEMP, M_WAITOK);
	if (inp_list == 0)
		return (ENOMEM);

	INP_INFO_RLOCK(&V_udbinfo);
	for (inp = LIST_FIRST(V_udbinfo.ipi_listhead), i = 0; inp && i < n;
	     inp = LIST_NEXT(inp, inp_list)) {
		INP_RLOCK(inp);
		if (inp->inp_gencnt <= gencnt &&
		    cr_canseeinpcb(req->td->td_ucred, inp) == 0)
			inp_list[i++] = inp;
		INP_RUNLOCK(inp);
	}
	INP_INFO_RUNLOCK(&V_udbinfo);
	n = i;

	error = 0;
	for (i = 0; i < n; i++) {
		inp = inp_list[i];
		INP_RLOCK(inp);
		if (inp->inp_gencnt <= gencnt) {
			struct xinpcb xi;
			bzero(&xi, sizeof(xi));
			xi.xi_len = sizeof xi;
*/			/* XXX should avoid extra copy */
/*			bcopy(inp, &xi.xi_inp, sizeof *inp);
			if (inp->inp_socket)
				sotoxsocket(inp->inp_socket, &xi.xi_socket);
			xi.xi_inp.inp_gencnt = inp->inp_gencnt;
			INP_RUNLOCK(inp);
			error = SYSCTL_OUT(req, &xi, sizeof xi);
		} else
			INP_RUNLOCK(inp);
	}
	if (!error) {
*/		/*
		 * Give the user an updated idea of our state.  If the
		 * generation differs from what we told her before, she knows
		 * that something happened while we were processing this
		 * request, and it might be necessary to retry.
		 */
/*		INP_INFO_RLOCK(&V_udbinfo);
		xig.xig_gen = V_udbinfo.ipi_gencnt;
		xig.xig_sogen = so_gencnt;
		xig.xig_count = V_udbinfo.ipi_count;
		INP_INFO_RUNLOCK(&V_udbinfo);
		error = SYSCTL_OUT(req, &xig, sizeof xig);
	}
	free(inp_list, M_TEMP);
	return (error);
}

SYSCTL_PROC(_net_inet_udp, UDPCTL_PCBLIST, pcblist, CTLFLAG_RD, 0, 0,
    udp_pcblist, "S,xinpcb", "List of active UDP sockets");

static int
udp_getcred(SYSCTL_HANDLER_ARGS)
{
	struct xucred xuc;
	struct sockaddr_in addrs[2];
	struct inpcb *inp;
	int error;

	//error = priv_check(req->td, PRIV_NETINET_GETCRED);
	if (error)
		return (error);
	error = SYSCTL_IN(req, addrs, sizeof(addrs));
	if (error)
		return (error);
	INP_INFO_RLOCK(&V_udbinfo);
	inp = in_pcblookup_hash(&V_udbinfo, addrs[1].sin_addr, addrs[1].sin_port,
				addrs[0].sin_addr, addrs[0].sin_port, 1, NULL);
	if (inp != NULL) {
		INP_RLOCK(inp);
		INP_INFO_RUNLOCK(&V_udbinfo);
		if (inp->inp_socket == NULL)
			error = ENOENT;
		if (error == 0)
			error = cr_canseeinpcb(req->td->td_ucred, inp);
		if (error == 0)
			cru2x(inp->inp_cred, &xuc);
		INP_RUNLOCK(inp);
	} else {
		INP_INFO_RUNLOCK(&V_udbinfo);
		error = ENOENT;
	}
	if (error == 0)
		error = SYSCTL_OUT(req, &xuc, sizeof(struct xucred));
	return (error);
}

SYSCTL_PROC(_net_inet_udp, OID_AUTO, getcred,
    CTLTYPE_OPAQUE|CTLFLAG_RW|CTLFLAG_PRISON, 0, 0,
    udp_getcred, "S,xucred", "Get the xucred of a UDP connection");
*/
int
udp_ctloutput(struct socket *so, struct sockopt *sopt)
{
	int error = 0, optval;
	struct inpcb *inp;
#ifdef IPSEC_NAT_T
	struct udpcb *up;
#endif

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("%s: inp == NULL", __func__));
	INP_WLOCK(inp);
	if (sopt->sopt_level != IPPROTO_UDP) {
#ifdef INET6
		if (INP_CHECK_SOCKAF(so, AF_INET6)) {
			INP_WUNLOCK(inp);
			error = ip6_ctloutput(so, sopt);
		} else {
#endif
			INP_WUNLOCK(inp);
			error = ip_ctloutput(so, sopt);
#ifdef INET6
		}
#endif
		return (error);
	}

	switch (sopt->sopt_dir) {
	case SOPT_SET:
		switch (sopt->sopt_name) {
		case UDP_ENCAP:
			INP_WUNLOCK(inp);
			error = sooptcopyin(sopt, &optval, sizeof optval,
					    sizeof optval);
			if (error)
				break;
			inp = sotoinpcb(so);
			KASSERT(inp != NULL, ("%s: inp == NULL", __func__));
			INP_WLOCK(inp);
#ifdef IPSEC_NAT_T
			up = intoudpcb(inp);
			KASSERT(up != NULL, ("%s: up == NULL", __func__));
#endif
			switch (optval) {
			case 0:
				/* Clear all UDP encap. */
#ifdef IPSEC_NAT_T
				up->u_flags &= ~UF_ESPINUDP_ALL;
#endif
				break;
#ifdef IPSEC_NAT_T
			case UDP_ENCAP_ESPINUDP:
			case UDP_ENCAP_ESPINUDP_NON_IKE:
				up->u_flags &= ~UF_ESPINUDP_ALL;
				if (optval == UDP_ENCAP_ESPINUDP)
					up->u_flags |= UF_ESPINUDP;
				else if (optval == UDP_ENCAP_ESPINUDP_NON_IKE)
					up->u_flags |= UF_ESPINUDP_NON_IKE;
				break;
#endif
			default:
				error = EINVAL;
				break;
			}
			INP_WUNLOCK(inp);
			break;
		default:
			INP_WUNLOCK(inp);
			error = ENOPROTOOPT;
			break;
		}
		break;
	case SOPT_GET:
		switch (sopt->sopt_name) {
#ifdef IPSEC_NAT_T
		case UDP_ENCAP:
			up = intoudpcb(inp);
			KASSERT(up != NULL, ("%s: up == NULL", __func__));
			optval = up->u_flags & UF_ESPINUDP_ALL;
			INP_WUNLOCK(inp);
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
#endif
		default:
			INP_WUNLOCK(inp);
			error = ENOPROTOOPT;
			break;
		}
		break;
	}	
	return (error);
}

static int
udp_output(struct inpcb *inp, struct mbuf *m, struct sockaddr *addr,
    struct mbuf *control, struct thread *td)
{
	struct udpiphdr *ui;
	int len = m->m_pkthdr.len;
	struct in_addr faddr, laddr;
	struct cmsghdr *cm;
	struct sockaddr_in *sin, src;
	int error = 0;
	int ipflags;
	u_short fport, lport;
	int unlock_udbinfo;

	/*
	 * udp_output() may need to temporarily bind or connect the current
	 * inpcb.  As such, we don't know up front whether we will need the
	 * pcbinfo lock or not.  Do any work to decide what is needed up
	 * front before acquiring any locks.
	 */
	if (len + sizeof(struct udpiphdr) > IP_MAXPACKET) {
		if (control)
			m_freem(control);
		m_freem(m);
		return (EMSGSIZE);
	}

	src.sin_family = 0;
	if (control != NULL) {
		/*
		 * XXX: Currently, we assume all the optional information is
		 * stored in a single mbuf.
		 */
		if (control->m_next) {
			m_freem(control);
			m_freem(m);
			return (EINVAL);
		}
		for (; control->m_len > 0;
		    control->m_data += CMSG_ALIGN(cm->cmsg_len),
		    control->m_len -= CMSG_ALIGN(cm->cmsg_len)) {
			cm = mtod(control, struct cmsghdr *);
			if (control->m_len < sizeof(*cm) || cm->cmsg_len == 0
			    || cm->cmsg_len > control->m_len) {
				error = EINVAL;
				break;
			}
			if (cm->cmsg_level != IPPROTO_IP)
				continue;

			switch (cm->cmsg_type) {
			case IP_SENDSRCADDR:
				if (cm->cmsg_len !=
				    CMSG_LEN(sizeof(struct in_addr))) {
					error = EINVAL;
					break;
				}
				bzero(&src, sizeof(src));
				src.sin_family = AF_INET;
				src.sin_len = sizeof(src);
				src.sin_port = inp->inp_lport;
				src.sin_addr =
				    *(struct in_addr *)CMSG_DATA(cm);
				break;

			default:
				error = ENOPROTOOPT;
				break;
			}
			if (error)
				break;
		}
		m_freem(control);
	}
	if (error) {
		m_freem(m);
		return (error);
	}

	/*
	 * Depending on whether or not the application has bound or connected
	 * the socket, we may have to do varying levels of work.  The optimal
	 * case is for a connected UDP socket, as a global lock isn't
	 * required at all.
	 *
	 * In order to decide which we need, we require stability of the
	 * inpcb binding, which we ensure by acquiring a read lock on the
	 * inpcb.  This doesn't strictly follow the lock order, so we play
	 * the trylock and retry game; note that we may end up with more
	 * conservative locks than required the second time around, so later
	 * assertions have to accept that.  Further analysis of the number of
	 * misses under contention is required.
	 */
	sin = (struct sockaddr_in *)addr;
	INP_RLOCK(inp);
	if (sin != NULL &&
	    (inp->inp_laddr.s_addr == INADDR_ANY && inp->inp_lport == 0)) {
		INP_RUNLOCK(inp);
		INP_INFO_WLOCK(&V_udbinfo);
		INP_WLOCK(inp);
		unlock_udbinfo = 2;
	} else if ((sin != NULL && (
	    (sin->sin_addr.s_addr == INADDR_ANY) ||
	    (sin->sin_addr.s_addr == INADDR_BROADCAST) ||
	    (inp->inp_laddr.s_addr == INADDR_ANY) ||
	    (inp->inp_lport == 0))) ||
	    (src.sin_family == AF_INET)) {
		if (!INP_INFO_TRY_RLOCK(&V_udbinfo)) {
			INP_RUNLOCK(inp);
			INP_INFO_RLOCK(&V_udbinfo);
			INP_RLOCK(inp);
		}
		unlock_udbinfo = 1;
	} else
		unlock_udbinfo = 0;

	/*
	 * If the IP_SENDSRCADDR control message was specified, override the
	 * source address for this datagram.  Its use is invalidated if the
	 * address thus specified is incomplete or clobbers other inpcbs.
	 */
	laddr = inp->inp_laddr;
	lport = inp->inp_lport;
	if (src.sin_family == AF_INET) {
		INP_INFO_LOCK_ASSERT(&V_udbinfo);
		if ((lport == 0) ||
		    (laddr.s_addr == INADDR_ANY &&
		     src.sin_addr.s_addr == INADDR_ANY)) {
			error = EINVAL;
			goto release;
		}
		error = in_pcbbind_setup(inp, (struct sockaddr *)&src,
		    &laddr.s_addr, &lport, NULL/*td->td_ucred*/);
		if (error)
			goto release;
	}

	/*
	 * If a UDP socket has been connected, then a local address/port will
	 * have been selected and bound.
	 *
	 * If a UDP socket has not been connected to, then an explicit
	 * destination address must be used, in which case a local
	 * address/port may not have been selected and bound.
	 */
	if (sin != NULL) {
		INP_LOCK_ASSERT(inp);
		if (inp->inp_faddr.s_addr != INADDR_ANY) {
			error = EISCONN;
			goto release;
		}

		/*
		 * Jail may rewrite the destination address, so let it do
		 * that before we use it.
		 */
//		error = prison_remote_ip4(td->td_ucred, &sin->sin_addr);
//		if (error)
//			goto release;

		/*
		 * If a local address or port hasn't yet been selected, or if
		 * the destination address needs to be rewritten due to using
		 * a special INADDR_ constant, invoke in_pcbconnect_setup()
		 * to do the heavy lifting.  Once a port is selected, we
		 * commit the binding back to the socket; we also commit the
		 * binding of the address if in jail.
		 *
		 * If we already have a valid binding and we're not
		 * requesting a destination address rewrite, use a fast path.
		 */
		if (inp->inp_laddr.s_addr == INADDR_ANY ||
		    inp->inp_lport == 0 ||
		    sin->sin_addr.s_addr == INADDR_ANY ||
		    sin->sin_addr.s_addr == INADDR_BROADCAST) {
			INP_INFO_LOCK_ASSERT(&V_udbinfo);
			error = in_pcbconnect_setup(inp, addr, &laddr.s_addr,
			    &lport, &faddr.s_addr, &fport, NULL,
			    NULL/*td->td_ucred*/);
			if (error)
				goto release;

			/*
			 * XXXRW: Why not commit the port if the address is
			 * !INADDR_ANY?
			 */
			/* Commit the local port if newly assigned. */
			if (inp->inp_laddr.s_addr == INADDR_ANY &&
			    inp->inp_lport == 0) {
				INP_INFO_WLOCK_ASSERT(&V_udbinfo);
				INP_WLOCK_ASSERT(inp);
				/*
				 * Remember addr if jailed, to prevent
				 * rebinding.
				 */
//				if (prison_flag(td->td_ucred, PR_IP4))
//					inp->inp_laddr = laddr;
				inp->inp_lport = lport;
				if (in_pcbinshash(inp) != 0) {
					inp->inp_lport = 0;
					error = EAGAIN;
					goto release;
				}
				inp->inp_flags |= INP_ANONPORT;
			}
		} else {
			faddr = sin->sin_addr;
			fport = sin->sin_port;
		}
	} else {
		INP_LOCK_ASSERT(inp);
		faddr = inp->inp_faddr;
		fport = inp->inp_fport;
		if (faddr.s_addr == INADDR_ANY) {
			error = ENOTCONN;
			goto release;
		}
	}

	/*
	 * Calculate data length and get a mbuf for UDP, IP, and possible
	 * link-layer headers.  Immediate slide the data pointer back forward
	 * since we won't use that space at this layer.
	 */
	M_PREPEND(m, sizeof(struct udpiphdr) + max_linkhdr, M_DONTWAIT);
	if (m == NULL) {
		error = ENOBUFS;
		goto release;
	}
	m->m_data += max_linkhdr;
	m->m_len -= max_linkhdr;
	m->m_pkthdr.len -= max_linkhdr;

	/*
	 * Fill in mbuf with extended UDP header and addresses and length put
	 * into network format.
	 */
	ui = mtod(m, struct udpiphdr *);
	bzero(ui->ui_x1, sizeof(ui->ui_x1));	/* XXX still needed? */
	ui->ui_pr = IPPROTO_UDP;
	ui->ui_src = laddr;
	ui->ui_dst = faddr;
	ui->ui_sport = lport;
	ui->ui_dport = fport;
	ui->ui_ulen = htons((u_short)len + sizeof(struct udphdr));

	/*
	 * Set the Don't Fragment bit in the IP header.
	 */
	if (inp->inp_flags & INP_DONTFRAG) {
		struct ip *ip;

		ip = (struct ip *)&ui->ui_i;
		ip->ip_off |= IP_DF;
	}

	ipflags = 0;
	if (inp->inp_socket->so_options & SO_DONTROUTE)
		ipflags |= IP_ROUTETOIF;
	if (inp->inp_socket->so_options & SO_BROADCAST)
		ipflags |= IP_ALLOWBROADCAST;
	if (inp->inp_flags & INP_ONESBCAST)
		ipflags |= IP_SENDONES;

#ifdef MAC
	mac_inpcb_create_mbuf(inp, m);
#endif

	/*
	 * Set up checksum and output datagram.
	 */
	if (udp_cksum) {
		if (inp->inp_flags & INP_ONESBCAST)
			faddr.s_addr = INADDR_BROADCAST;
		ui->ui_sum = in_pseudo(ui->ui_src.s_addr, faddr.s_addr,
		    htons((u_short)len + sizeof(struct udphdr) + IPPROTO_UDP));
		m->m_pkthdr.csum_flags = CSUM_UDP;
		m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);
	} else
		ui->ui_sum = 0;
	((struct ip *)ui)->ip_len = sizeof (struct udpiphdr) + len;
	((struct ip *)ui)->ip_ttl = inp->inp_ip_ttl;	/* XXX */
	((struct ip *)ui)->ip_tos = inp->inp_ip_tos;	/* XXX */
	UDPSTAT_INC(udps_opackets);

	if (unlock_udbinfo == 2)
		INP_INFO_WUNLOCK(&V_udbinfo);
	else if (unlock_udbinfo == 1)
		INP_INFO_RUNLOCK(&V_udbinfo);
	error = ip_output(m, inp->inp_options, NULL, ipflags,
	    inp->inp_moptions, inp);
	if (unlock_udbinfo == 2)
		INP_WUNLOCK(inp);
	else
		INP_RUNLOCK(inp);
	return (error);

release:
	if (unlock_udbinfo == 2) {
		INP_WUNLOCK(inp);
		INP_INFO_WUNLOCK(&V_udbinfo);
	} else if (unlock_udbinfo == 1) {
		INP_RUNLOCK(inp);
		INP_INFO_RUNLOCK(&V_udbinfo);
	} else
		INP_RUNLOCK(inp);
	m_freem(m);
	return (error);
}


#if defined(IPSEC) && defined(IPSEC_NAT_T)
#ifdef INET
/*
 * Potentially decap ESP in UDP frame.  Check for an ESP header
 * and optional marker; if present, strip the UDP header and
 * push the result through IPSec.
 *
 * Returns mbuf to be processed (potentially re-allocated) or
 * NULL if consumed and/or processed.
 */
static struct mbuf *
udp4_espdecap(struct inpcb *inp, struct mbuf *m, int off)
{
	size_t minlen, payload, skip, iphlen;
	caddr_t data;
	struct udpcb *up;
	struct m_tag *tag;
	struct udphdr *udphdr;
	struct ip *ip;

	INP_RLOCK_ASSERT(inp);

	/* 
	 * Pull up data so the longest case is contiguous:
	 *    IP/UDP hdr + non ESP marker + ESP hdr.
	 */
	minlen = off + sizeof(uint64_t) + sizeof(struct esp);
	if (minlen > m->m_pkthdr.len)
		minlen = m->m_pkthdr.len;
	if ((m = m_pullup(m, minlen)) == NULL) {
		V_ipsec4stat.in_inval++;
		return (NULL);		/* Bypass caller processing. */
	}
	data = mtod(m, caddr_t);	/* Points to ip header. */
	payload = m->m_len - off;	/* Size of payload. */

	if (payload == 1 && data[off] == '\xff')
		return (m);		/* NB: keepalive packet, no decap. */

	up = intoudpcb(inp);
	KASSERT(up != NULL, ("%s: udpcb NULL", __func__));
	KASSERT((up->u_flags & UF_ESPINUDP_ALL) != 0,
	    ("u_flags 0x%x", up->u_flags));

	/* 
	 * Check that the payload is large enough to hold an
	 * ESP header and compute the amount of data to remove.
	 *
	 * NB: the caller has already done a pullup for us.
	 * XXX can we assume alignment and eliminate bcopys?
	 */
	if (up->u_flags & UF_ESPINUDP_NON_IKE) {
		/*
		 * draft-ietf-ipsec-nat-t-ike-0[01].txt and
		 * draft-ietf-ipsec-udp-encaps-(00/)01.txt, ignoring
		 * possible AH mode non-IKE marker+non-ESP marker
		 * from draft-ietf-ipsec-udp-encaps-00.txt.
		 */
		uint64_t marker;

		if (payload <= sizeof(uint64_t) + sizeof(struct esp))
			return (m);	/* NB: no decap. */
		bcopy(data + off, &marker, sizeof(uint64_t));
		if (marker != 0)	/* Non-IKE marker. */
			return (m);	/* NB: no decap. */
		skip = sizeof(uint64_t) + sizeof(struct udphdr);
	} else {
		uint32_t spi;

		if (payload <= sizeof(struct esp)) {
			V_ipsec4stat.in_inval++;
			m_freem(m);
			return (NULL);	/* Discard. */
		}
		bcopy(data + off, &spi, sizeof(uint32_t));
		if (spi == 0)		/* Non-ESP marker. */
			return (m);	/* NB: no decap. */
		skip = sizeof(struct udphdr);
	}

	/*
	 * Setup a PACKET_TAG_IPSEC_NAT_T_PORT tag to remember
	 * the UDP ports. This is required if we want to select
	 * the right SPD for multiple hosts behind same NAT.
	 *
	 * NB: ports are maintained in network byte order everywhere
	 *     in the NAT-T code.
	 */
	tag = m_tag_get(PACKET_TAG_IPSEC_NAT_T_PORTS,
		2 * sizeof(uint16_t), M_NOWAIT);
	if (tag == NULL) {
		V_ipsec4stat.in_nomem++;
		m_freem(m);
		return (NULL);		/* Discard. */
	}
	iphlen = off - sizeof(struct udphdr);
	udphdr = (struct udphdr *)(data + iphlen);
	((uint16_t *)(tag + 1))[0] = udphdr->uh_sport;
	((uint16_t *)(tag + 1))[1] = udphdr->uh_dport;
	m_tag_prepend(m, tag);

	/*
	 * Remove the UDP header (and possibly the non ESP marker)
	 * IP header length is iphlen
	 * Before:
	 *   <--- off --->
	 *   +----+------+-----+
	 *   | IP |  UDP | ESP |
	 *   +----+------+-----+
	 *        <-skip->
	 * After:
	 *          +----+-----+
	 *          | IP | ESP |
	 *          +----+-----+
	 *   <-skip->
	 */
	ovbcopy(data, data + skip, iphlen);
	m_adj(m, skip);

	ip = mtod(m, struct ip *);
	ip->ip_len -= skip;
	ip->ip_p = IPPROTO_ESP;

	/*
	 * We cannot yet update the cksums so clear any
	 * h/w cksum flags as they are no longer valid.
	 */
	if (m->m_pkthdr.csum_flags & CSUM_DATA_VALID)
		m->m_pkthdr.csum_flags &= ~(CSUM_DATA_VALID|CSUM_PSEUDO_HDR);

	(void) ipsec4_common_input(m, iphlen, ip->ip_p);
	return (NULL);			/* NB: consumed, bypass processing. */
}
#endif /* INET */
#endif /* defined(IPSEC) && defined(IPSEC_NAT_T) */

static void
udp_abort(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_abort: inp == NULL"));
	INP_INFO_WLOCK(&V_udbinfo);
	INP_WLOCK(inp);
	if (inp->inp_faddr.s_addr != INADDR_ANY) {
		in_pcbdisconnect(inp);
		inp->inp_laddr.s_addr = INADDR_ANY;
		soisdisconnected(so);
	}
	INP_WUNLOCK(inp);
	INP_INFO_WUNLOCK(&V_udbinfo);
}

static int
udp_attach(struct socket *so, int proto, struct thread *td)
{
	struct inpcb *inp;
	int error;

	inp = sotoinpcb(so);
	KASSERT(inp == NULL, ("udp_attach: inp != NULL"));
	error = soreserve(so, udp_sendspace, udp_recvspace);
	if (error)
		return (error);
	INP_INFO_WLOCK(&V_udbinfo);
	error = in_pcballoc(so, &V_udbinfo);
	if (error) {
		INP_INFO_WUNLOCK(&V_udbinfo);
		return (error);
	}

	inp = (struct inpcb *)so->so_pcb;
	inp->inp_vflag |= INP_IPV4;
	inp->inp_ip_ttl = V_ip_defttl;

	error = udp_newudpcb(inp);
	if (error) {
		in_pcbdetach(inp);
		in_pcbfree(inp);
		INP_INFO_WUNLOCK(&V_udbinfo);
		return (error);
	}

	INP_WUNLOCK(inp);
	INP_INFO_WUNLOCK(&V_udbinfo);
	return (0);
}

int
udp_set_kernel_tunneling(struct socket *so, udp_tun_func_t f)
{
	struct inpcb *inp;
	struct udpcb *up;

	KASSERT(so->so_type == SOCK_DGRAM, ("udp_set_kernel_tunneling: !dgram"));
	KASSERT(so->so_pcb != NULL, ("udp_set_kernel_tunneling: NULL inp"));
	if (so->so_type != SOCK_DGRAM) {
		/* Not UDP socket... sorry! */
		return (ENOTSUP);
	}
	inp = (struct inpcb *)so->so_pcb;
	if (inp == NULL) {
		/* NULL INP? */
		return (EINVAL);
	}
	INP_WLOCK(inp);
	up = intoudpcb(inp);
	if (up->u_tun_func != NULL) {
		INP_WUNLOCK(inp);
		return (EBUSY);
	}
	up->u_tun_func = f;
	INP_WUNLOCK(inp);
	return (0);
}

static int
udp_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct inpcb *inp;
	int error;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_bind: inp == NULL"));
	INP_INFO_WLOCK(&V_udbinfo);
	INP_WLOCK(inp);
	error = in_pcbbind(inp, nam, NULL/*td->td_ucred*/);
	INP_WUNLOCK(inp);
	INP_INFO_WUNLOCK(&V_udbinfo);
	return (error);
}

static void
udp_close(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_close: inp == NULL"));
	INP_INFO_WLOCK(&V_udbinfo);
	INP_WLOCK(inp);
	if (inp->inp_faddr.s_addr != INADDR_ANY) {
		in_pcbdisconnect(inp);
		inp->inp_laddr.s_addr = INADDR_ANY;
		soisdisconnected(so);
	}
	INP_WUNLOCK(inp);
	INP_INFO_WUNLOCK(&V_udbinfo);
}

static int
udp_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct inpcb *inp;
	int error;
	struct sockaddr_in *sin;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_connect: inp == NULL"));
	INP_INFO_WLOCK(&V_udbinfo);
	INP_WLOCK(inp);
	if (inp->inp_faddr.s_addr != INADDR_ANY) {
		INP_WUNLOCK(inp);
		INP_INFO_WUNLOCK(&V_udbinfo);
		return (EISCONN);
	}
	sin = (struct sockaddr_in *)nam;
/*	error = prison_remote_ip4(td->td_ucred, &sin->sin_addr);
	if (error != 0) {
		INP_WUNLOCK(inp);
		INP_INFO_WUNLOCK(&V_udbinfo);
		return (error);
	}*/
	error = in_pcbconnect(inp, nam, NULL/*td->td_ucred*/);
	if (error == 0)
		soisconnected(so);
	INP_WUNLOCK(inp);
	INP_INFO_WUNLOCK(&V_udbinfo);
	return (error);
}

static void
udp_detach(struct socket *so)
{
	struct inpcb *inp;
	struct udpcb *up;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_detach: inp == NULL"));
	KASSERT(inp->inp_faddr.s_addr == INADDR_ANY,
	    ("udp_detach: not disconnected"));
	INP_INFO_WLOCK(&V_udbinfo);
	INP_WLOCK(inp);
	up = intoudpcb(inp);
	KASSERT(up != NULL, ("%s: up == NULL", __func__));
	inp->inp_ppcb = NULL;
	in_pcbdetach(inp);
	in_pcbfree(inp);
	INP_INFO_WUNLOCK(&V_udbinfo);
	udp_discardcb(up);
}

static int
udp_disconnect(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_disconnect: inp == NULL"));
	INP_INFO_WLOCK(&V_udbinfo);
	INP_WLOCK(inp);
	if (inp->inp_faddr.s_addr == INADDR_ANY) {
		INP_WUNLOCK(inp);
		INP_INFO_WUNLOCK(&V_udbinfo);
		return (ENOTCONN);
	}

	in_pcbdisconnect(inp);
	inp->inp_laddr.s_addr = INADDR_ANY;
	SOCK_LOCK(so);
	so->so_state &= ~SS_ISCONNECTED;		/* XXX */
	SOCK_UNLOCK(so);
	INP_WUNLOCK(inp);
	INP_INFO_WUNLOCK(&V_udbinfo);
	return (0);
}

static int
udp_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
    struct mbuf *control, struct thread *td)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_send: inp == NULL"));
	return (udp_output(inp, m, addr, control, td));
}

int
udp_shutdown(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_shutdown: inp == NULL"));
	INP_WLOCK(inp);
	socantsendmore(so);
	INP_WUNLOCK(inp);
	return (0);
}

struct pr_usrreqs udp_usrreqs = {
	.pru_abort =		udp_abort,
	.pru_attach =		udp_attach,
	.pru_bind =		udp_bind,
	.pru_connect =		udp_connect,
	.pru_control =		in_control,
	.pru_detach =		udp_detach,
	.pru_disconnect =	udp_disconnect,
	.pru_peeraddr =		in_getpeeraddr,
	.pru_send =		udp_send,
	.pru_soreceive =	soreceive_dgram,
	.pru_sosend =		sosend_dgram,
	.pru_shutdown =		udp_shutdown,
	.pru_sockaddr =		in_getsockaddr,
	.pru_sosetlabel =	in_pcbsosetlabel,
	.pru_close =		udp_close,
};
