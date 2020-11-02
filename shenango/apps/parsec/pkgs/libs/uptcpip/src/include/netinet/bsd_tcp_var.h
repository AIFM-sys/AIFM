/*-
 * Copyright (c) 1982, 1986, 1993, 1994, 1995
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
 *	@(#)tcp_var.h	8.4 (Berkeley) 5/24/95
 * $FreeBSD$
 */

#ifndef _BSD_NETINET_TCP_VAR_H_
#define _BSD_NETINET_TCP_VAR_H_

#include <netinet/bsd_tcp.h>

#ifdef _FREEBSD_KERNEL
#include <net/bsd_vnet.h>

/*
 * Kernel variables for tcp.
 */
VNET_DECLARE(int, tcp_do_rfc1323);
VNET_DECLARE(int, tcp_reass_qsize);
VNET_DECLARE(struct uma_zone *, tcp_reass_zone);
#define	V_tcp_do_rfc1323	VNET(tcp_do_rfc1323)
#define	V_tcp_reass_qsize	VNET(tcp_reass_qsize)
#define	V_tcp_reass_zone	VNET(tcp_reass_zone)

#endif /* _FREEBSD_KERNEL */

/* TCP segment queue entry */
struct tseg_qent {
	LIST_ENTRY(tseg_qent) tqe_q;
	int	tqe_len;		/* TCP segment data length */
	struct	tcphdr *tqe_th;		/* a pointer to tcp header */
	struct	mbuf	*tqe_m;		/* mbuf contains packet */
};
LIST_HEAD(tsegqe_head, tseg_qent);

struct sackblk {
	tcp_seq start;		/* start seq no. of sack block */
	tcp_seq end;		/* end seq no. */
};

struct sackhole {
	tcp_seq start;		/* start seq no. of hole */
	tcp_seq end;		/* end seq no. */
	tcp_seq rxmit;		/* next seq. no in hole to be retransmitted */
	TAILQ_ENTRY(sackhole) scblink;	/* scoreboard linkage */
};

struct sackhint {
	struct sackhole	*nexthole;
	int		sack_bytes_rexmit;

	int		ispare;		/* explicit pad for 64bit alignment */
	uint64_t	_pad[2];	/* 1 sacked_bytes, 1 TBD */
};

struct tcptemp {
	u_char	tt_ipgen[40]; /* the size must be of max ip header, now IPv6 */
	struct	tcphdr tt_t;
};

#define tcp6cb		tcpcb  /* for KAME src sync over BSD*'s */

/* Neighbor Discovery, Neighbor Unreachability Detection Upper layer hint. */
#ifdef INET6
#define ND6_HINT(tp)						\
do {								\
	if ((tp) && (tp)->t_inpcb &&				\
	    ((tp)->t_inpcb->inp_vflag & INP_IPV6) != 0)		\
		nd6_nud_hint(NULL, NULL, 0);			\
} while (0)
#else
#define ND6_HINT(tp)
#endif

/*
 * Tcp control block, one per tcp; fields:
 * Organized for 16 byte cacheline efficiency.
 */
struct tcpcb {
	struct	tsegqe_head t_segq;	/* segment reassembly queue */
	void	*t_pspare[2];		/* new reassembly queue */
	int	t_segqlen;		/* segment reassembly queue length */
	int	t_dupacks;		/* consecutive dup acks recd */

	struct tcp_timer *t_timers;	/* All the TCP timers in one struct */

	struct	inpcb *t_inpcb;		/* back pointer to internet pcb */
	int	t_state;		/* state of this connection */
	u_int	t_flags;

	struct	vnet *t_vnet;		/* back pointer to parent vnet */

	tcp_seq	snd_una;		/* send unacknowledged */
	tcp_seq	snd_max;		/* highest sequence number sent;
					 * used to recognize retransmits
					 */
	tcp_seq	snd_nxt;		/* send next */
	tcp_seq	snd_up;			/* send urgent pointer */

	tcp_seq	snd_wl1;		/* window update seg seq number */
	tcp_seq	snd_wl2;		/* window update seg ack number */
	tcp_seq	iss;			/* initial send sequence number */
	tcp_seq	irs;			/* initial receive sequence number */

	tcp_seq	rcv_nxt;		/* receive next */
	tcp_seq	rcv_adv;		/* advertised window */
	u_long	rcv_wnd;		/* receive window */
	tcp_seq	rcv_up;			/* receive urgent pointer */

	u_long	snd_wnd;		/* send window */
	u_long	snd_cwnd;		/* congestion-controlled window */
	u_long	snd_bwnd;		/* bandwidth-controlled window */
	u_long	snd_ssthresh;		/* snd_cwnd size threshold for
					 * for slow start exponential to
					 * linear switch
					 */
	u_long	snd_bandwidth;		/* calculated bandwidth or 0 */
	tcp_seq	snd_recover;		/* for use in NewReno Fast Recovery */

	u_int	t_maxopd;		/* mss plus options */

	u_int	t_rcvtime;		/* inactivity time */
	u_int	t_starttime;		/* time connection was established */
	u_int	t_rtttime;		/* RTT measurement start time */
	tcp_seq	t_rtseq;		/* sequence number being timed */

	u_int	t_bw_rtttime;		/* used for bandwidth calculation */
	tcp_seq	t_bw_rtseq;		/* used for bandwidth calculation */

	int	t_rxtcur;		/* current retransmit value (ticks) */
	u_int	t_maxseg;		/* maximum segment size */
	int	t_srtt;			/* smoothed round-trip time */
	int	t_rttvar;		/* variance in round-trip time */

	int	t_rxtshift;		/* log(2) of rexmt exp. backoff */
	u_int	t_rttmin;		/* minimum rtt allowed */
	u_int	t_rttbest;		/* best rtt we've seen */
	u_long	t_rttupdated;		/* number of times rtt sampled */
	u_long	max_sndwnd;		/* largest window peer has offered */

	int	t_softerror;		/* possible error not yet reported */
/* out-of-band data */
	char	t_oobflags;		/* have some */
	char	t_iobc;			/* input character */
/* RFC 1323 variables */
	u_char	snd_scale;		/* window scaling for send window */
	u_char	rcv_scale;		/* window scaling for recv window */
	u_char	request_r_scale;	/* pending window scaling */
	u_int32_t  ts_recent;		/* timestamp echo data */
	u_int	ts_recent_age;		/* when last updated */
	u_int32_t  ts_offset;		/* our timestamp offset */

	tcp_seq	last_ack_sent;
/* experimental */
	u_long	snd_cwnd_prev;		/* cwnd prior to retransmit */
	u_long	snd_ssthresh_prev;	/* ssthresh prior to retransmit */
	tcp_seq	snd_recover_prev;	/* snd_recover prior to retransmit */
	u_int	t_badrxtwin;		/* window for retransmit recovery */
	u_char	snd_limited;		/* segments limited transmitted */
/* SACK related state */
	int	snd_numholes;		/* number of holes seen by sender */
	TAILQ_HEAD(sackhole_head, sackhole) snd_holes;
					/* SACK scoreboard (sorted) */
	tcp_seq	snd_fack;		/* last seq number(+1) sack'd by rcv'r*/
	int	rcv_numsacks;		/* # distinct sack blks present */
	struct sackblk sackblks[MAX_SACK_BLKS]; /* seq nos. of sack blocks */
	tcp_seq sack_newdata;		/* New data xmitted in this recovery
					   episode starts at this seq number */
	struct sackhint	sackhint;	/* SACK scoreboard hint */
	int	t_rttlow;		/* smallest observerved RTT */
	u_int32_t	rfbuf_ts;	/* recv buffer autoscaling timestamp */
	int	rfbuf_cnt;		/* recv buffer autoscaling byte count */
	struct toe_usrreqs *t_tu;	/* offload operations vector */
	void	*t_toe;			/* TOE pcb pointer */
	int	t_bytes_acked;		/* # bytes acked during current RTT */

	int	t_ispare;		/* explicit pad for 64bit alignment */
	void	*t_pspare2[6];		/* 2 CC / 4 TBD */
	uint64_t _pad[12];		/* 7 UTO, 5 TBD (1-2 CC/RTT?) */
};

/*
 * Flags and utility macros for the t_flags field.
 */
#define	TF_ACKNOW	0x000001	/* ack peer immediately */
#define	TF_DELACK	0x000002	/* ack, but try to delay it */
#define	TF_NODELAY	0x000004	/* don't delay packets to coalesce */
#define	TF_NOOPT	0x000008	/* don't use tcp options */
#define	TF_SENTFIN	0x000010	/* have sent FIN */
#define	TF_REQ_SCALE	0x000020	/* have/will request window scaling */
#define	TF_RCVD_SCALE	0x000040	/* other side has requested scaling */
#define	TF_REQ_TSTMP	0x000080	/* have/will request timestamps */
#define	TF_RCVD_TSTMP	0x000100	/* a timestamp was received in SYN */
#define	TF_SACK_PERMIT	0x000200	/* other side said I could SACK */
#define	TF_NEEDSYN	0x000400	/* send SYN (implicit state) */
#define	TF_NEEDFIN	0x000800	/* send FIN (implicit state) */
#define	TF_NOPUSH	0x001000	/* don't push */
#define	TF_MORETOCOME	0x010000	/* More data to be appended to sock */
#define	TF_LQ_OVERFLOW	0x020000	/* listen queue overflow */
#define	TF_LASTIDLE	0x040000	/* connection was previously idle */
#define	TF_RXWIN0SENT	0x080000	/* sent a receiver win 0 in response */
#define	TF_FASTRECOVERY	0x100000	/* in NewReno Fast Recovery */
#define	TF_WASFRECOVERY	0x200000	/* was in NewReno Fast Recovery */
#define	TF_SIGNATURE	0x400000	/* require MD5 digests (RFC2385) */
#define	TF_FORCEDATA	0x800000	/* force out a byte */
#define	TF_TSO		0x1000000	/* TSO enabled on this connection */
#define	TF_TOE		0x2000000	/* this connection is offloaded */
#define	TF_ECN_PERMIT	0x4000000	/* connection ECN-ready */
#define	TF_ECN_SND_CWR	0x8000000	/* ECN CWR in queue */
#define	TF_ECN_SND_ECE	0x10000000	/* ECN ECE in queue */

#define IN_FASTRECOVERY(tp)	(tp->t_flags & TF_FASTRECOVERY)
#define ENTER_FASTRECOVERY(tp)	tp->t_flags |= TF_FASTRECOVERY
#define EXIT_FASTRECOVERY(tp)	tp->t_flags &= ~TF_FASTRECOVERY

/*
 * Flags for the t_oobflags field.
 */
#define	TCPOOB_HAVEDATA	0x01
#define	TCPOOB_HADDATA	0x02

#ifdef TCP_SIGNATURE
/*
 * Defines which are needed by the xform_tcp module and tcp_[in|out]put
 * for SADB verification and lookup.
 */
#define	TCP_SIGLEN	16	/* length of computed digest in bytes */
#define	TCP_KEYLEN_MIN	1	/* minimum length of TCP-MD5 key */
#define	TCP_KEYLEN_MAX	80	/* maximum length of TCP-MD5 key */
/*
 * Only a single SA per host may be specified at this time. An SPI is
 * needed in order for the KEY_ALLOCSA() lookup to work.
 */
#define	TCP_SIG_SPI	0x1000
#endif /* TCP_SIGNATURE */

/*
 * Structure to hold TCP options that are only used during segment
 * processing (in tcp_input), but not held in the tcpcb.
 * It's basically used to reduce the number of parameters
 * to tcp_dooptions and tcp_addoptions.
 * The binary order of the to_flags is relevant for packing of the
 * options in tcp_addoptions.
 */
struct tcpopt {
	u_int64_t	to_flags;	/* which options are present */
#define	TOF_MSS		0x0001		/* maximum segment size */
#define	TOF_SCALE	0x0002		/* window scaling */
#define	TOF_SACKPERM	0x0004		/* SACK permitted */
#define	TOF_TS		0x0010		/* timestamp */
#define	TOF_SIGNATURE	0x0040		/* TCP-MD5 signature option (RFC2385) */
#define	TOF_SACK	0x0080		/* Peer sent SACK option */
#define	TOF_MAXOPT	0x0100
	u_int32_t	to_tsval;	/* new timestamp */
	u_int32_t	to_tsecr;	/* reflected timestamp */
	u_char		*to_sacks;	/* pointer to the first SACK blocks */
	u_char		*to_signature;	/* pointer to the TCP-MD5 signature */
	u_int16_t	to_mss;		/* maximum segment size */
	u_int8_t	to_wscale;	/* window scaling */
	u_int8_t	to_nsacks;	/* number of SACK blocks */
};

/*
 * Flags for tcp_dooptions.
 */
#define	TO_SYN		0x01		/* parse SYN-only options */

struct hc_metrics_lite {	/* must stay in sync with hc_metrics */
	u_long	rmx_mtu;	/* MTU for this path */
	u_long	rmx_ssthresh;	/* outbound gateway buffer limit */
	u_long	rmx_rtt;	/* estimated round trip time */
	u_long	rmx_rttvar;	/* estimated rtt variance */
	u_long	rmx_bandwidth;	/* estimated bandwidth */
	u_long	rmx_cwnd;	/* congestion window */
	u_long	rmx_sendpipe;   /* outbound delay-bandwidth product */
	u_long	rmx_recvpipe;   /* inbound delay-bandwidth product */
};

#ifndef _BSD_NETINET_IN_PCB_H_
struct in_conninfo;
#endif /* _BSD_NETINET_IN_PCB_H_ */

struct tcptw {
	struct inpcb	*tw_inpcb;	/* XXX back pointer to internet pcb */
	tcp_seq		snd_nxt;
	tcp_seq		rcv_nxt;
	tcp_seq		iss;
	tcp_seq		irs;
	u_short		last_win;	/* cached window value */
	u_short		tw_so_options;	/* copy of so_options */
	struct ucred	*tw_cred;	/* user credentials */
	u_int32_t	t_recent;
	u_int32_t	ts_offset;	/* our timestamp offset */
	u_int		t_starttime;
	int		tw_time;
	TAILQ_ENTRY(tcptw) tw_2msl;
};

#define	intotcpcb(ip)	((struct tcpcb *)(ip)->inp_ppcb)
#define	intotw(ip)	((struct tcptw *)(ip)->inp_ppcb)
#define	sototcpcb(so)	(intotcpcb(sotoinpcb(so)))

/*
 * The smoothed round-trip time and estimated variance
 * are stored as fixed point numbers scaled by the values below.
 * For convenience, these scales are also used in smoothing the average
 * (smoothed = (1/scale)sample + ((scale-1)/scale)smoothed).
 * With these scales, srtt has 3 bits to the right of the binary point,
 * and thus an "ALPHA" of 0.875.  rttvar has 2 bits to the right of the
 * binary point, and is smoothed with an ALPHA of 0.75.
 */
#define	TCP_RTT_SCALE		32	/* multiplier for srtt; 3 bits frac. */
#define	TCP_RTT_SHIFT		5	/* shift for srtt; 3 bits frac. */
#define	TCP_RTTVAR_SCALE	16	/* multiplier for rttvar; 2 bits */
#define	TCP_RTTVAR_SHIFT	4	/* shift for rttvar; 2 bits */
#define	TCP_DELTA_SHIFT		2	/* see tcp_input.c */

/*
 * The initial retransmission should happen at rtt + 4 * rttvar.
 * Because of the way we do the smoothing, srtt and rttvar
 * will each average +1/2 tick of bias.  When we compute
 * the retransmit timer, we want 1/2 tick of rounding and
 * 1 extra tick because of +-1/2 tick uncertainty in the
 * firing of the timer.  The bias will give us exactly the
 * 1.5 tick we need.  But, because the bias is
 * statistical, we have to test that we don't drop below
 * the minimum feasible timer (which is 2 ticks).
 * This version of the macro adapted from a paper by Lawrence
 * Brakmo and Larry Peterson which outlines a problem caused
 * by insufficient precision in the original implementation,
 * which results in inappropriately large RTO values for very
 * fast networks.
 */
#define	TCP_REXMTVAL(tp) \
	max((tp)->t_rttmin, (((tp)->t_srtt >> (TCP_RTT_SHIFT - TCP_DELTA_SHIFT))  \
	  + (tp)->t_rttvar) >> TCP_DELTA_SHIFT)

/*
 * TCP statistics.
 * Many of these should be kept per connection,
 * but that's inconvenient at the moment.
 */
struct	tcpstat {
	u_long	tcps_connattempt;	/* connections initiated */
	u_long	tcps_accepts;		/* connections accepted */
	u_long	tcps_connects;		/* connections established */
	u_long	tcps_drops;		/* connections dropped */
	u_long	tcps_conndrops;		/* embryonic connections dropped */
	u_long	tcps_minmssdrops;	/* average minmss too low drops */
	u_long	tcps_closed;		/* conn. closed (includes drops) */
	u_long	tcps_segstimed;		/* segs where we tried to get rtt */
	u_long	tcps_rttupdated;	/* times we succeeded */
	u_long	tcps_delack;		/* delayed acks sent */
	u_long	tcps_timeoutdrop;	/* conn. dropped in rxmt timeout */
	u_long	tcps_rexmttimeo;	/* retransmit timeouts */
	u_long	tcps_persisttimeo;	/* persist timeouts */
	u_long	tcps_keeptimeo;		/* keepalive timeouts */
	u_long	tcps_keepprobe;		/* keepalive probes sent */
	u_long	tcps_keepdrops;		/* connections dropped in keepalive */

	u_long	tcps_sndtotal;		/* total packets sent */
	u_long	tcps_sndpack;		/* data packets sent */
	u_long	tcps_sndbyte;		/* data bytes sent */
	u_long	tcps_sndrexmitpack;	/* data packets retransmitted */
	u_long	tcps_sndrexmitbyte;	/* data bytes retransmitted */
	u_long	tcps_sndrexmitbad;	/* unnecessary packet retransmissions */
	u_long	tcps_sndacks;		/* ack-only packets sent */
	u_long	tcps_sndprobe;		/* window probes sent */
	u_long	tcps_sndurg;		/* packets sent with URG only */
	u_long	tcps_sndwinup;		/* window update-only packets sent */
	u_long	tcps_sndctrl;		/* control (SYN|FIN|RST) packets sent */

	u_long	tcps_rcvtotal;		/* total packets received */
	u_long	tcps_rcvpack;		/* packets received in sequence */
	u_long	tcps_rcvbyte;		/* bytes received in sequence */
	u_long	tcps_rcvbadsum;		/* packets received with ccksum errs */
	u_long	tcps_rcvbadoff;		/* packets received with bad offset */
	u_long	tcps_rcvmemdrop;	/* packets dropped for lack of memory */
	u_long	tcps_rcvshort;		/* packets received too short */
	u_long	tcps_rcvduppack;	/* duplicate-only packets received */
	u_long	tcps_rcvdupbyte;	/* duplicate-only bytes received */
	u_long	tcps_rcvpartduppack;	/* packets with some duplicate data */
	u_long	tcps_rcvpartdupbyte;	/* dup. bytes in part-dup. packets */
	u_long	tcps_rcvoopack;		/* out-of-order packets received */
	u_long	tcps_rcvoobyte;		/* out-of-order bytes received */
	u_long	tcps_rcvpackafterwin;	/* packets with data after window */
	u_long	tcps_rcvbyteafterwin;	/* bytes rcvd after window */
	u_long	tcps_rcvafterclose;	/* packets rcvd after "close" */
	u_long	tcps_rcvwinprobe;	/* rcvd window probe packets */
	u_long	tcps_rcvdupack;		/* rcvd duplicate acks */
	u_long	tcps_rcvacktoomuch;	/* rcvd acks for unsent data */
	u_long	tcps_rcvackpack;	/* rcvd ack packets */
	u_long	tcps_rcvackbyte;	/* bytes acked by rcvd acks */
	u_long	tcps_rcvwinupd;		/* rcvd window update packets */
	u_long	tcps_pawsdrop;		/* segments dropped due to PAWS */
	u_long	tcps_predack;		/* times hdr predict ok for acks */
	u_long	tcps_preddat;		/* times hdr predict ok for data pkts */
	u_long	tcps_pcbcachemiss;
	u_long	tcps_cachedrtt;		/* times cached RTT in route updated */
	u_long	tcps_cachedrttvar;	/* times cached rttvar updated */
	u_long	tcps_cachedssthresh;	/* times cached ssthresh updated */
	u_long	tcps_usedrtt;		/* times RTT initialized from route */
	u_long	tcps_usedrttvar;	/* times RTTVAR initialized from rt */
	u_long	tcps_usedssthresh;	/* times ssthresh initialized from rt*/
	u_long	tcps_persistdrop;	/* timeout in persist state */
	u_long	tcps_badsyn;		/* bogus SYN, e.g. premature ACK */
	u_long	tcps_mturesent;		/* resends due to MTU discovery */
	u_long	tcps_listendrop;	/* listen queue overflows */
	u_long	tcps_badrst;		/* ignored RSTs in the window */

	u_long	tcps_sc_added;		/* entry added to syncache */
	u_long	tcps_sc_retransmitted;	/* syncache entry was retransmitted */
	u_long	tcps_sc_dupsyn;		/* duplicate SYN packet */
	u_long	tcps_sc_dropped;	/* could not reply to packet */
	u_long	tcps_sc_completed;	/* successful extraction of entry */
	u_long	tcps_sc_bucketoverflow;	/* syncache per-bucket limit hit */
	u_long	tcps_sc_cacheoverflow;	/* syncache cache limit hit */
	u_long	tcps_sc_reset;		/* RST removed entry from syncache */
	u_long	tcps_sc_stale;		/* timed out or listen socket gone */
	u_long	tcps_sc_aborted;	/* syncache entry aborted */
	u_long	tcps_sc_badack;		/* removed due to bad ACK */
	u_long	tcps_sc_unreach;	/* ICMP unreachable received */
	u_long	tcps_sc_zonefail;	/* zalloc() failed */
	u_long	tcps_sc_sendcookie;	/* SYN cookie sent */
	u_long	tcps_sc_recvcookie;	/* SYN cookie received */

	u_long	tcps_hc_added;		/* entry added to hostcache */
	u_long	tcps_hc_bucketoverflow;	/* hostcache per bucket limit hit */

	u_long  tcps_finwait2_drops;    /* Drop FIN_WAIT_2 connection after time limit */

	/* SACK related stats */
	u_long	tcps_sack_recovery_episode; /* SACK recovery episodes */
	u_long  tcps_sack_rexmits;	    /* SACK rexmit segments   */
	u_long  tcps_sack_rexmit_bytes;	    /* SACK rexmit bytes      */
	u_long  tcps_sack_rcv_blocks;	    /* SACK blocks (options) received */
	u_long  tcps_sack_send_blocks;	    /* SACK blocks (options) sent     */
	u_long  tcps_sack_sboverflow; 	    /* times scoreboard overflowed */
	
	/* ECN related stats */
	u_long	tcps_ecn_ce;		/* ECN Congestion Experienced */
	u_long	tcps_ecn_ect0;		/* ECN Capable Transport */
	u_long	tcps_ecn_ect1;		/* ECN Capable Transport */
	u_long	tcps_ecn_shs;		/* ECN successful handshakes */
	u_long	tcps_ecn_rcwnd;		/* # times ECN reduced the cwnd */

	u_long	_pad[12];		/* 6 UTO, 6 TBD */
};

#ifdef _FREEBSD_KERNEL
/*
 * In-kernel consumers can use these accessor macros directly to update
 * stats.
 */
#define	TCPSTAT_ADD(name, val)	V_tcpstat.name += (val)
#define	TCPSTAT_INC(name)	TCPSTAT_ADD(name, 1)

/*
 * Kernel module consumers must use this accessor macro.
 */
void	kmod_tcpstat_inc(int statnum);
#define	KMOD_TCPSTAT_INC(name)						\
	kmod_tcpstat_inc(offsetof(struct tcpstat, name) / sizeof(u_long))
#endif

/*
 * TCB structure exported to user-land via sysctl(3).
 * Evil hack: declare only if in_pcb.h and sys/socketvar.h have been
 * included.  Not all of our clients do.
 */
#if defined(_BSD_NETINET_IN_PCB_H_) && defined(_BSD_SYS_SOCKETVAR_H_)
struct	xtcpcb {
	size_t	xt_len;
	struct	inpcb	xt_inp;
	struct	tcpcb	xt_tp;
	struct	xsocket	xt_socket;
	u_quad_t	xt_alignment_hack;
};
#endif

/*
 * Names for TCP sysctl objects
 */
#define	TCPCTL_DO_RFC1323	1	/* use RFC-1323 extensions */
#define	TCPCTL_MSSDFLT		3	/* MSS default */
#define TCPCTL_STATS		4	/* statistics (read-only) */
#define	TCPCTL_RTTDFLT		5	/* default RTT estimate */
#define	TCPCTL_KEEPIDLE		6	/* keepalive idle timer */
#define	TCPCTL_KEEPINTVL	7	/* interval to send keepalives */
#define	TCPCTL_SENDSPACE	8	/* send buffer space */
#define	TCPCTL_RECVSPACE	9	/* receive buffer space */
#define	TCPCTL_KEEPINIT		10	/* timeout for establishing syn */
#define	TCPCTL_PCBLIST		11	/* list of all outstanding PCBs */
#define	TCPCTL_DELACKTIME	12	/* time before sending delayed ACK */
#define	TCPCTL_V6MSSDFLT	13	/* MSS default for IPv6 */
#define	TCPCTL_SACK		14	/* Selective Acknowledgement,rfc 2018 */
#define	TCPCTL_DROP		15	/* drop tcp connection */
#define	TCPCTL_MAXID		16
#define TCPCTL_FINWAIT2_TIMEOUT        17

#define TCPCTL_NAMES { \
	{ 0, 0 }, \
	{ "rfc1323", CTLTYPE_INT }, \
	{ "mssdflt", CTLTYPE_INT }, \
	{ "stats", CTLTYPE_STRUCT }, \
	{ "rttdflt", CTLTYPE_INT }, \
	{ "keepidle", CTLTYPE_INT }, \
	{ "keepintvl", CTLTYPE_INT }, \
	{ "sendspace", CTLTYPE_INT }, \
	{ "recvspace", CTLTYPE_INT }, \
	{ "keepinit", CTLTYPE_INT }, \
	{ "pcblist", CTLTYPE_STRUCT }, \
	{ "delacktime", CTLTYPE_INT }, \
	{ "v6mssdflt", CTLTYPE_INT }, \
	{ "maxid", CTLTYPE_INT }, \
}


#ifdef _FREEBSD_KERNEL
#ifdef SYSCTL_DECL
SYSCTL_DECL(_net_inet_tcp);
SYSCTL_DECL(_net_inet_tcp_sack);
#endif
MALLOC_DECLARE(M_TCPLOG);

extern	int tcp_log_in_vain;

VNET_DECLARE(struct inpcbhead, tcb);		/* queue of active tcpcb's */
VNET_DECLARE(struct inpcbinfo, tcbinfo);
VNET_DECLARE(struct tcpstat, tcpstat);		/* tcp statistics */
VNET_DECLARE(int, tcp_mssdflt);	/* XXX */
VNET_DECLARE(int, tcp_minmss);
VNET_DECLARE(int, tcp_delack_enabled);
VNET_DECLARE(int, tcp_do_newreno);
VNET_DECLARE(int, path_mtu_discovery);
VNET_DECLARE(int, ss_fltsz);
VNET_DECLARE(int, ss_fltsz_local);

#define	V_tcb			VNET(tcb)
#define	V_tcbinfo		VNET(tcbinfo)
#define	V_tcpstat		VNET(tcpstat)
#define	V_tcp_mssdflt		VNET(tcp_mssdflt)
#define	V_tcp_minmss		VNET(tcp_minmss)
#define	V_tcp_delack_enabled	VNET(tcp_delack_enabled)
#define	V_tcp_do_newreno	VNET(tcp_do_newreno)
#define	V_path_mtu_discovery	VNET(path_mtu_discovery)
#define	V_ss_fltsz		VNET(ss_fltsz)
#define	V_ss_fltsz_local	VNET(ss_fltsz_local)

VNET_DECLARE(int, blackhole);
VNET_DECLARE(int, drop_synfin);
VNET_DECLARE(int, tcp_do_rfc3042);
VNET_DECLARE(int, tcp_do_rfc3390);
VNET_DECLARE(int, tcp_insecure_rst);
VNET_DECLARE(int, tcp_do_autorcvbuf);
VNET_DECLARE(int, tcp_autorcvbuf_inc);
VNET_DECLARE(int, tcp_autorcvbuf_max);
VNET_DECLARE(int, tcp_do_rfc3465);
VNET_DECLARE(int, tcp_abc_l_var);

#define	V_blackhole		VNET(blackhole)
#define	V_drop_synfin		VNET(drop_synfin)
#define	V_tcp_do_rfc3042	VNET(tcp_do_rfc3042)
#define	V_tcp_do_rfc3390	VNET(tcp_do_rfc3390)
#define	V_tcp_insecure_rst	VNET(tcp_insecure_rst)
#define	V_tcp_do_autorcvbuf	VNET(tcp_do_autorcvbuf)
#define	V_tcp_autorcvbuf_inc	VNET(tcp_autorcvbuf_inc)
#define	V_tcp_autorcvbuf_max	VNET(tcp_autorcvbuf_max)
#define	V_tcp_do_rfc3465	VNET(tcp_do_rfc3465)
#define	V_tcp_abc_l_var		VNET(tcp_abc_l_var)

VNET_DECLARE(int, tcp_do_tso);
VNET_DECLARE(int, tcp_do_autosndbuf);
VNET_DECLARE(int, tcp_autosndbuf_inc);
VNET_DECLARE(int, tcp_autosndbuf_max);

#define	V_tcp_do_tso		VNET(tcp_do_tso)
#define	V_tcp_do_autosndbuf	VNET(tcp_do_autosndbuf)
#define	V_tcp_autosndbuf_inc	VNET(tcp_autosndbuf_inc)
#define	V_tcp_autosndbuf_max	VNET(tcp_autosndbuf_max)

VNET_DECLARE(int, nolocaltimewait);

#define	V_nolocaltimewait	VNET(nolocaltimewait)

VNET_DECLARE(int, tcp_do_sack);			/* SACK enabled/disabled */
VNET_DECLARE(int, tcp_sack_maxholes);
VNET_DECLARE(int, tcp_sack_globalmaxholes);
VNET_DECLARE(int, tcp_sack_globalholes);
VNET_DECLARE(int, tcp_sc_rst_sock_fail);	/* RST on sock alloc failure */
VNET_DECLARE(int, tcp_do_ecn);			/* TCP ECN enabled/disabled */
VNET_DECLARE(int, tcp_ecn_maxretries);

#define	V_tcp_do_sack		VNET(tcp_do_sack)
#define	V_tcp_sack_maxholes	VNET(tcp_sack_maxholes)
#define	V_tcp_sack_globalmaxholes	VNET(tcp_sack_globalmaxholes)
#define	V_tcp_sack_globalholes	VNET(tcp_sack_globalholes)
#define	V_tcp_sc_rst_sock_fail	VNET(tcp_sc_rst_sock_fail)
#define	V_tcp_do_ecn		VNET(tcp_do_ecn)
#define	V_tcp_ecn_maxretries	VNET(tcp_ecn_maxretries)

int	 tcp_addoptions(struct tcpopt *, u_char *);
struct tcpcb *
	 tcp_close(struct tcpcb *);
void	 tcp_discardcb(struct tcpcb *);
void	 tcp_twstart(struct tcpcb *);
#if 0
int	 tcp_twrecycleable(struct tcptw *tw);
#endif
void	 tcp_twclose(struct tcptw *_tw, int _reuse);
void	 tcp_ctlinput(int, struct sockaddr *, void *);
int	 tcp_ctloutput(struct socket *, struct sockopt *);
//struct tcpcb * 	 tcp_drop(struct tcpcb *, int);
struct tcpcb * tcp_drop(struct tcpcb *tp, int);
void	 tcp_drain(void);
void	 tcp_fasttimo(void);
void	 tcp_init(void);
#ifdef VIMAGE
void	 tcp_destroy(void);
#endif
void	 tcp_fini(void *);
char 	*tcp_log_addrs(struct in_conninfo *, struct tcphdr *, void *,
	    const void *);
int	 tcp_reass(struct tcpcb *, struct tcphdr *, int *, struct mbuf *);
void	 tcp_reass_init(void);
void	 tcp_input(struct mbuf *, int);
u_long	 tcp_maxmtu(struct in_conninfo *, int *);
u_long	 tcp_maxmtu6(struct in_conninfo *, int *);
void	 tcp_mss_update(struct tcpcb *, int, struct hc_metrics_lite *, int *);
void	 tcp_mss(struct tcpcb *, int);
int	 tcp_mssopt(struct in_conninfo *);
struct inpcb *  tcp_drop_syn_sent(struct inpcb *, int);
struct inpcb *
	 tcp_mtudisc(struct inpcb *, int);
struct tcpcb *
	 tcp_newtcpcb(struct inpcb *);
int	 tcp_output(struct tcpcb *);
void	 tcp_respond(struct tcpcb *, void *,
	    struct tcphdr *, struct mbuf *, tcp_seq, tcp_seq, int);
void	 tcp_tw_init(void);
#ifdef VIMAGE
void	 tcp_tw_destroy(void);
#endif
void	 tcp_tw_zone_change(void);
int	 tcp_twcheck(struct inpcb *, struct tcpopt *, struct tcphdr *,
	    struct mbuf *, int);
int	 tcp_twrespond(struct tcptw *, int);
void	 tcp_setpersist(struct tcpcb *);
#ifdef TCP_SIGNATURE
int	 tcp_signature_compute(struct mbuf *, int, int, int, u_char *, u_int);
#endif
void	 tcp_slowtimo(void);
struct tcptemp *
	 tcpip_maketemplate(struct inpcb *);
void	 tcpip_fillheaders(struct inpcb *, void *, void *);
void	 tcp_timer_activate(struct tcpcb *, int, u_int);
int	 tcp_timer_active(struct tcpcb *, int);
void	 tcp_trace(short, short, struct tcpcb *, void *, struct tcphdr *, int);
void	 tcp_xmit_bandwidth_limit(struct tcpcb *tp, tcp_seq ack_seq);
/*
 * All tcp_hc_* functions are IPv4 and IPv6 (via in_conninfo)
 */
void	 tcp_hc_init(void);
#ifdef VIMAGE
void	 tcp_hc_destroy(void);
#endif
void	 tcp_hc_get(struct in_conninfo *, struct hc_metrics_lite *);
u_long	 tcp_hc_getmtu(struct in_conninfo *);
void	 tcp_hc_updatemtu(struct in_conninfo *, u_long);
void	 tcp_hc_update(struct in_conninfo *, struct hc_metrics_lite *);

extern	struct pr_usrreqs tcp_usrreqs;
extern	u_long tcp_sendspace;
extern	u_long tcp_recvspace;
tcp_seq tcp_new_isn(struct tcpcb *);

void	 tcp_sack_doack(struct tcpcb *, struct tcpopt *, tcp_seq);
void	 tcp_update_sack_list(struct tcpcb *tp, tcp_seq rcv_laststart, tcp_seq rcv_lastend);
void	 tcp_clean_sackreport(struct tcpcb *tp);
void	 tcp_sack_adjust(struct tcpcb *tp);
struct sackhole *tcp_sack_output(struct tcpcb *tp, int *sack_bytes_rexmt);
void	 tcp_sack_partialack(struct tcpcb *, struct tcphdr *);
void	 tcp_free_sackholes(struct tcpcb *tp);
int	 tcp_newreno(struct tcpcb *, struct tcphdr *);
u_long	 tcp_seq_subtract(u_long, u_long );

#endif /* _FREEBSD_KERNEL */

#endif /* _BSD_NETINET_TCP_VAR_H_ */
