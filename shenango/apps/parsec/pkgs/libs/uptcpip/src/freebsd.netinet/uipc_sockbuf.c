/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
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
 *	@(#)uipc_socket2.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/bsd_cdefs.h>
//__FBSDID("$FreeBSD$");

#include "bsd_opt_param.h"

#include <sys/bsd_param.h>
#include <sys/bsd_aio.h> /* for aio_swake proto */
#include <sys/bsd_kernel.h>
#include <sys/bsd_lock.h>
#include <sys/bsd_mbuf.h>
#include <sys/bsd_mutex.h>
////#include <sys/bsd_proc.h>
#include <sys/bsd_protosw.h>
#include <sys/bsd_resourcevar.h>
//#include <sys/bsd_signalvar.h>
#include <sys/bsd_socket.h>
#include <sys/bsd_socketvar.h>
#include <sys/bsd_sx.h>
//#include <sys/bsd_sysctl.h>

#include "host_serv.h"

/*
 * Function pointer set by the AIO routines so that the socket buffer code
 * can call back into the AIO module if it is loaded.
 */
void	(*aio_swake)(struct socket *, struct sockbuf *);

/*
 * Primitive routines for operating on socket buffers
 */

u_long	sb_max = SB_MAX;
u_long sb_max_adj = 1ULL * (SB_MAX) * (MCLBYTES) / (MSIZE + MCLBYTES); /* adjusted sb_max */

static	u_long sb_efficiency = 8;	/* parameter for sbreserve() */

static void	sbdrop_internal(struct sockbuf *sb, int len);
static void	sbflush_internal(struct sockbuf *sb);

/*
 * Socantsendmore indicates that no more data will be sent on the socket; it
 * would normally be applied to a socket when the user informs the system
 * that no more data is to be sent, by the protocol code (in case
 * PRU_SHUTDOWN).  Socantrcvmore indicates that no more data will be
 * received, and will normally be applied to the socket by a protocol when it
 * detects that the peer will send no more data.  Data queued for reading in
 * the socket may yet be read.
 */
void
socantsendmore_locked(struct socket *so)
{

	SOCKBUF_LOCK_ASSERT(&so->so_snd);

	so->so_snd.sb_state |= SBS_CANTSENDMORE;
	sowwakeup_locked(so);
	mtx_assert(SOCKBUF_MTX(&so->so_snd), MA_NOTOWNED);
}

void
socantsendmore(struct socket *so)
{

	SOCKBUF_LOCK(&so->so_snd);
	socantsendmore_locked(so);
	mtx_assert(SOCKBUF_MTX(&so->so_snd), MA_NOTOWNED);
}

void
socantrcvmore_locked(struct socket *so)
{

	SOCKBUF_LOCK_ASSERT(&so->so_rcv);

	so->so_rcv.sb_state |= SBS_CANTRCVMORE;
	sorwakeup_locked(so);
	mtx_assert(SOCKBUF_MTX(&so->so_rcv), MA_NOTOWNED);
}

void
socantrcvmore(struct socket *so)
{

	SOCKBUF_LOCK(&so->so_rcv);
	socantrcvmore_locked(so);
	mtx_assert(SOCKBUF_MTX(&so->so_rcv), MA_NOTOWNED);
}

/*
 * Wait for data to arrive at/drain from a socket buffer.
 */
int
sbwait(struct sockbuf *sb)
{

	SOCKBUF_LOCK_ASSERT(sb);

	sb->sb_flags |= SB_WAIT;
	return (msleep(sb->sb_cc_cond, &sb->sb_mtx,
	    (sb->sb_flags & SB_NOINTR) ? PSOCK : PSOCK | PCATCH, "sbwait",
	    sb->sb_timeo));
}

int
sblock(struct sockbuf *sb, int flags)
{

	KASSERT((flags & SBL_VALID) == flags,
	    ("sblock: flags invalid (0x%x)", flags));

	if (flags & SBL_WAIT) {
		if ((sb->sb_flags & SB_NOINTR) ||
		    (flags & SBL_NOINTR)) {
			sx_xlock(&sb->sb_sx);
			return (0);
		}
		return (sx_xlock_sig(&sb->sb_sx));
	} else {
		if (sx_try_xlock(&sb->sb_sx) == 0)
			return (EWOULDBLOCK);
		return (0);
	}
}

void
sbunlock(struct sockbuf *sb)
{
	sx_xunlock(&sb->sb_sx);
}

/*
 * Wakeup processes waiting on a socket buffer.  Do asynchronous notification
 * via SIGIO if the socket has the SS_ASYNC flag set.
 *
 * Called with the socket buffer lock held; will release the lock by the end
 * of the function.  This allows the caller to acquire the socket buffer lock
 * while testing for the need for various sorts of wakeup and hold it through
 * to the point where it's no longer required.  We currently hold the lock
 * through calls out to other subsystems (with the exception of kqueue), and
 * then release it to avoid lock order issues.  It's not clear that's
 * correct.
 */
void
sowakeup(struct socket *so, struct sockbuf *sb)
{
	int ret;

	SOCKBUF_LOCK_ASSERT(sb);

//	selwakeuppri(&sb->sb_sel, PSOCK);
//	if (!SEL_WAITING(&sb->sb_sel))
//		sb->sb_flags &= ~SB_SEL;
	if (sb->sb_flags & SB_WAIT) {
		sb->sb_flags &= ~SB_WAIT;
		wakeup(sb->sb_cc_cond);
	}
//	KNOTE_LOCKED(&sb->sb_sel.si_note, 0);
	if (sb->sb_upcall != NULL) {
		ret = sb->sb_upcall(so, sb->sb_upcallarg, M_DONTWAIT);
		if (ret == SU_ISCONNECTED) {
			KASSERT(sb == &so->so_rcv,
			    ("SO_SND upcall returned SU_ISCONNECTED"));
			soupcall_clear(so, SO_RCV);
		}
	} else
		ret = SU_OK;
//	if (sb->sb_flags & SB_AIO)
//		aio_swake(so, sb);
	SOCKBUF_UNLOCK(sb);
	if (ret == SU_ISCONNECTED)
		soisconnected(so);
//	if ((so->so_state & SS_ASYNC) && so->so_sigio != NULL)
//		pgsigio(&so->so_sigio, SIGIO, 0);

	mtx_assert(SOCKBUF_MTX(sb), MA_NOTOWNED);
}

/*
 * Socket buffer (struct sockbuf) utility routines.
 *
 * Each socket contains two socket buffers: one for sending data and one for
 * receiving data.  Each buffer contains a queue of mbufs, information about
 * the number of mbufs and amount of data in the queue, and other fields
 * allowing select() statements and notification on data availability to be
 * implemented.
 *
 * Data stored in a socket buffer is maintained as a list of records.  Each
 * record is a list of mbufs chained together with the m_next field.  Records
 * are chained together with the m_nextpkt field. The upper level routine
 * soreceive() expects the following conventions to be observed when placing
 * information in the receive buffer:
 *
 * 1. If the protocol requires each message be preceded by the sender's name,
 *    then a record containing that name must be present before any
 *    associated data (mbuf's must be of type MT_SONAME).
 * 2. If the protocol supports the exchange of ``access rights'' (really just
 *    additional data associated with the message), and there are ``rights''
 *    to be received, then a record containing this data should be present
 *    (mbuf's must be of type MT_RIGHTS).
 * 3. If a name or rights record exists, then it must be followed by a data
 *    record, perhaps of zero length.
 *
 * Before using a new socket structure it is first necessary to reserve
 * buffer space to the socket, by calling sbreserve().  This should commit
 * some of the available buffer space in the system buffer pool for the
 * socket (currently, it does nothing but enforce limits).  The space should
 * be released by calling sbrelease() when the socket is destroyed.
 */
int
soreserve(struct socket *so, u_long sndcc, u_long rcvcc)
{
	struct thread *td = NULL;//curthread;

	SOCKBUF_LOCK(&so->so_snd);
	SOCKBUF_LOCK(&so->so_rcv);
	if (sbreserve_locked(&so->so_snd, sndcc, so, td) == 0)
		goto bad;
	if (sbreserve_locked(&so->so_rcv, rcvcc, so, td) == 0)
		goto bad2;
	if (so->so_rcv.sb_lowat == 0)
		so->so_rcv.sb_lowat = 1;// 0x8000;
	if (so->so_snd.sb_lowat == 0)
		so->so_snd.sb_lowat = MCLBYTES;
	if (so->so_snd.sb_lowat > so->so_snd.sb_hiwat)
		so->so_snd.sb_lowat = so->so_snd.sb_hiwat;

	so->so_snd.sb_cc_cond = host_pthread_cond_init();
	so->so_snd.sb_timeo_cond= host_pthread_cond_init();
	so->so_rcv.sb_cc_cond = host_pthread_cond_init();
	so->so_rcv.sb_timeo_cond= host_pthread_cond_init();
	
	SOCKBUF_UNLOCK(&so->so_rcv);
	SOCKBUF_UNLOCK(&so->so_snd);
	return (0);
bad2:
	sbrelease_locked(&so->so_snd, so);
bad:
	SOCKBUF_UNLOCK(&so->so_rcv);
	SOCKBUF_UNLOCK(&so->so_snd);
	return (ENOBUFS);
}

#if 0
static int
sysctl_handle_sb_max(SYSCTL_HANDLER_ARGS)
{
	int error = 0;
	u_long tmp_sb_max = sb_max;

	error = sysctl_handle_long(oidp, &tmp_sb_max, arg2, req);
	if (error || !req->newptr)
		return (error);
	if (tmp_sb_max < MSIZE + MCLBYTES)
		return (EINVAL);
	sb_max = tmp_sb_max;
	sb_max_adj = (u_quad_t)sb_max * MCLBYTES / (MSIZE + MCLBYTES);
	return (0);
}
#endif	
/*
 * Allot mbufs to a sockbuf.  Attempt to scale mbmax so that mbcnt doesn't
 * become limiting if buffering efficiency is near the normal case.
 */
int
sbreserve_locked(struct sockbuf *sb, u_long cc, struct socket *so,
    struct thread *td)
{
	rlim_t sbsize_limit;

	SOCKBUF_LOCK_ASSERT(sb);

	/*
	 * When a thread is passed, we take into account the thread's socket
	 * buffer size limit.  The caller will generally pass curthread, but
	 * in the TCP input path, NULL will be passed to indicate that no
	 * appropriate thread resource limits are available.  In that case,
	 * we don't apply a process limit.
	 */
	if (cc > sb_max_adj)
		return (0);
	if (td != NULL) {
		//PROC_LOCK(td->td_proc);
		//sbsize_limit = lim_cur(td->td_proc, RLIMIT_SBSIZE);
		//PROC_UNLOCK(td->td_proc);
	} else 
		sbsize_limit = RLIM_INFINITY;
//	if (!chgsbsize(so->so_cred->cr_uidinfo, &sb->sb_hiwat, cc,
//	    sbsize_limit))
//		return (0);
	sb->sb_hiwat = cc; //baoyg
	
	sb->sb_mbmax = min(cc * sb_efficiency, sb_max);
	if (sb->sb_lowat > sb->sb_hiwat)
		sb->sb_lowat = sb->sb_hiwat;

	return (1);
}

int
sbreserve(struct sockbuf *sb, u_long cc, struct socket *so, 
    struct thread *td)
{
	int error;

	SOCKBUF_LOCK(sb);
	error = sbreserve_locked(sb, cc, so, td);
	SOCKBUF_UNLOCK(sb);
	return (error);
}

/*
 * Free mbufs held by a socket, and reserved mbuf space.
 */
void
sbrelease_internal(struct sockbuf *sb, struct socket *so)
{

	sbflush_internal(sb);
//	(void)chgsbsize(so->so_cred->cr_uidinfo, &sb->sb_hiwat, 0,
//	    RLIM_INFINITY);
	sb->sb_hiwat = 0; //baoyg
	sb->sb_mbmax = 0;
}

void
sbrelease_locked(struct sockbuf *sb, struct socket *so)
{

	SOCKBUF_LOCK_ASSERT(sb);

	sbrelease_internal(sb, so);
}

void
sbrelease(struct sockbuf *sb, struct socket *so)
{

	SOCKBUF_LOCK(sb);
	sbrelease_locked(sb, so);
	SOCKBUF_UNLOCK(sb);
}

void
sbdestroy(struct sockbuf *sb, struct socket *so)
{

	host_pthread_cond_destroy(sb->sb_cc_cond);
	host_pthread_cond_destroy(sb->sb_timeo_cond);	
	sbrelease_internal(sb, so);
}

/*
 * Routines to add and remove data from an mbuf queue.
 *
 * The routines sbappend() or sbappendrecord() are normally called to append
 * new mbufs to a socket buffer, after checking that adequate space is
 * available, comparing the function sbspace() with the amount of data to be
 * added.  sbappendrecord() differs from sbappend() in that data supplied is
 * treated as the beginning of a new record.  To place a sender's address,
 * optional access rights, and data in a socket receive buffer,
 * sbappendaddr() should be used.  To place access rights and data in a
 * socket receive buffer, sbappendrights() should be used.  In either case,
 * the new data begins a new record.  Note that unlike sbappend() and
 * sbappendrecord(), these routines check for the caller that there will be
 * enough space to store the data.  Each fails if there is not enough space,
 * or if it cannot find mbufs to store additional information in.
 *
 * Reliable protocols may use the socket send buffer to hold data awaiting
 * acknowledgement.  Data is normally copied from a socket send buffer in a
 * protocol with m_copy for output to a peer, and then removing the data from
 * the socket buffer with sbdrop() or sbdroprecord() when the data is
 * acknowledged by the peer.
 */
#ifdef SOCKBUF_DEBUG
void
sblastrecordchk(struct sockbuf *sb, const char *file, int line)
{
	struct mbuf *m = sb->sb_mb;

	SOCKBUF_LOCK_ASSERT(sb);

	while (m && m->m_nextpkt)
		m = m->m_nextpkt;

	if (m != sb->sb_lastrecord) {
		printf("%s: sb_mb %p sb_lastrecord %p last %p\n",
			__func__, sb->sb_mb, sb->sb_lastrecord, m);
		printf("packet chain:\n");
		for (m = sb->sb_mb; m != NULL; m = m->m_nextpkt)
			printf("\t%p\n", m);
		panic("%s from %s:%u", __func__, file, line);
	}
}

void
sblastmbufchk(struct sockbuf *sb, const char *file, int line)
{
	struct mbuf *m = sb->sb_mb;
	struct mbuf *n;

	SOCKBUF_LOCK_ASSERT(sb);

	while (m && m->m_nextpkt)
		m = m->m_nextpkt;

	while (m && m->m_next)
		m = m->m_next;

	if (m != sb->sb_mbtail) {
		printf("%s: sb_mb %p sb_mbtail %p last %p\n",
			__func__, sb->sb_mb, sb->sb_mbtail, m);
		printf("packet tree:\n");
		for (m = sb->sb_mb; m != NULL; m = m->m_nextpkt) {
			printf("\t");
			for (n = m; n != NULL; n = n->m_next)
				printf("%p ", n);
			printf("\n");
		}
		panic("%s from %s:%u", __func__, file, line);
	}
}
#endif /* SOCKBUF_DEBUG */

#define SBLINKRECORD(sb, m0) do {					\
	SOCKBUF_LOCK_ASSERT(sb);					\
	if ((sb)->sb_lastrecord != NULL)				\
		(sb)->sb_lastrecord->m_nextpkt = (m0);			\
	else								\
		(sb)->sb_mb = (m0);					\
	(sb)->sb_lastrecord = (m0);					\
} while (/*CONSTCOND*/0)

/*
 * Append mbuf chain m to the last record in the socket buffer sb.  The
 * additional space associated the mbuf chain is recorded in sb.  Empty mbufs
 * are discarded and mbufs are compacted where possible.
 */
void
sbappend_locked(struct sockbuf *sb, struct mbuf *m)
{
	struct mbuf *n;

	SOCKBUF_LOCK_ASSERT(sb);

	if (m == 0)
		return;

	SBLASTRECORDCHK(sb);
	n = sb->sb_mb;
	if (n) {
		while (n->m_nextpkt)
			n = n->m_nextpkt;
		do {
			if (n->m_flags & M_EOR) {
				sbappendrecord_locked(sb, m); /* XXXXXX!!!! */
				return;
			}
		} while (n->m_next && (n = n->m_next));
	} else {
		/*
		 * XXX Would like to simply use sb_mbtail here, but
		 * XXX I need to verify that I won't miss an EOR that
		 * XXX way.
		 */
		if ((n = sb->sb_lastrecord) != NULL) {
			do {
				if (n->m_flags & M_EOR) {
					sbappendrecord_locked(sb, m); /* XXXXXX!!!! */
					return;
				}
			} while (n->m_next && (n = n->m_next));
		} else {
			/*
			 * If this is the first record in the socket buffer,
			 * it's also the last record.
			 */
			sb->sb_lastrecord = m;
		}
	}
	sbcompress(sb, m, n);
	SBLASTRECORDCHK(sb);
}

/*
 * Append mbuf chain m to the last record in the socket buffer sb.  The
 * additional space associated the mbuf chain is recorded in sb.  Empty mbufs
 * are discarded and mbufs are compacted where possible.
 */
void
sbappend(struct sockbuf *sb, struct mbuf *m)
{

	SOCKBUF_LOCK(sb);
	sbappend_locked(sb, m);
	SOCKBUF_UNLOCK(sb);
}

/*
 * This version of sbappend() should only be used when the caller absolutely
 * knows that there will never be more than one record in the socket buffer,
 * that is, a stream protocol (such as TCP).
 */
void
sbappendstream_locked(struct sockbuf *sb, struct mbuf *m)
{
	SOCKBUF_LOCK_ASSERT(sb);

	KASSERT(m->m_nextpkt == NULL,("sbappendstream 0"));
	KASSERT(sb->sb_mb == sb->sb_lastrecord,("sbappendstream 1"));

	SBLASTMBUFCHK(sb);

	/* Remove all packet headers and mbuf tags to get a pure data chain. */
	m_demote(m, 1);
	
	sbcompress(sb, m, sb->sb_mbtail);

	sb->sb_lastrecord = sb->sb_mb;
	SBLASTRECORDCHK(sb);
}

/*
 * This version of sbappend() should only be used when the caller absolutely
 * knows that there will never be more than one record in the socket buffer,
 * that is, a stream protocol (such as TCP).
 */
void
sbappendstream(struct sockbuf *sb, struct mbuf *m)
{

	SOCKBUF_LOCK(sb);
	sbappendstream_locked(sb, m);
	SOCKBUF_UNLOCK(sb);
}

#ifdef SOCKBUF_DEBUG
void
sbcheck(struct sockbuf *sb)
{
	struct mbuf *m;
	struct mbuf *n = 0;
	u_long len = 0, mbcnt = 0;

	SOCKBUF_LOCK_ASSERT(sb);

	for (m = sb->sb_mb; m; m = n) {
	    n = m->m_nextpkt;
	    for (; m; m = m->m_next) {
		len += m->m_len;
		mbcnt += MSIZE;
		if (m->m_flags & M_EXT) /*XXX*/ /* pretty sure this is bogus */
			mbcnt += m->m_ext.ext_size;
	    }
	}
	if (len != sb->sb_cc || mbcnt != sb->sb_mbcnt) {
		printf("cc %ld != %u || mbcnt %ld != %u\n", len, sb->sb_cc,
		    mbcnt, sb->sb_mbcnt);
		panic("sbcheck");
	}
}
#endif

/*
 * As above, except the mbuf chain begins a new record.
 */
void
sbappendrecord_locked(struct sockbuf *sb, struct mbuf *m0)
{
	struct mbuf *m;

	SOCKBUF_LOCK_ASSERT(sb);

	if (m0 == 0)
		return;
	/*
	 * Put the first mbuf on the queue.  Note this permits zero length
	 * records.
	 */
	sballoc(sb, m0);
	SBLASTRECORDCHK(sb);
	SBLINKRECORD(sb, m0);
	sb->sb_mbtail = m0;
	m = m0->m_next;
	
	CHECK_SPLIT_LINKCNT(m0, m0->m_next, "sbappendrecord_locked");
	m0->m_next = 0;
	if (m && (m0->m_flags & M_EOR)) {
		m0->m_flags &= ~M_EOR;
		m->m_flags |= M_EOR;
	}
	/* always call sbcompress() so it can do SBLASTMBUFCHK() */
	sbcompress(sb, m, m0);
}

/*
 * As above, except the mbuf chain begins a new record.
 */
void
sbappendrecord(struct sockbuf *sb, struct mbuf *m0)
{

	SOCKBUF_LOCK(sb);
	sbappendrecord_locked(sb, m0);
	SOCKBUF_UNLOCK(sb);
}

/*
 * Append address and data, and optionally, control (ancillary) data to the
 * receive queue of a socket.  If present, m0 must include a packet header
 * with total length.  Returns 0 if no space in sockbuf or insufficient
 * mbufs.
 */
int
sbappendaddr_locked(struct sockbuf *sb, const struct sockaddr *asa,
    struct mbuf *m0, struct mbuf *control)
{
	struct mbuf *m, *n, *nlast;
	int space = asa->sa_len;

	SOCKBUF_LOCK_ASSERT(sb);

	if (m0 && (m0->m_flags & M_PKTHDR) == 0)
		panic("sbappendaddr_locked");
	if (m0)
		space += m0->m_pkthdr.len;
	space += m_length(control, &n);

	if (space > sbspace(sb))
		return (0);
#if MSIZE <= 256
	if (asa->sa_len > MLEN)
		return (0);
#endif
	MGET(m, M_DONTWAIT, MT_SONAME);
	if (m == 0)
		return (0);
	m->m_len = asa->sa_len;
	bcopy(asa, mtod(m, caddr_t), asa->sa_len);
	if (n){
		CHECK_ADD_LINKCNT(n, m0, NULL, "sbappendaddr_locked");
		n->m_next = m0;		/* concatenate data to control */
	}else
		control = m0;
	CHECK_ADD_LINKCNT(m, control, NULL, "sbappendaddr_locked");	
	m->m_next = control;
	for (n = m; n->m_next != NULL; n = n->m_next)
		sballoc(sb, n);
	sballoc(sb, n);
	nlast = n;
	SBLINKRECORD(sb, m);

	sb->sb_mbtail = nlast;
	SBLASTMBUFCHK(sb);

	SBLASTRECORDCHK(sb);
	return (1);
}

/*
 * Append address and data, and optionally, control (ancillary) data to the
 * receive queue of a socket.  If present, m0 must include a packet header
 * with total length.  Returns 0 if no space in sockbuf or insufficient
 * mbufs.
 */
int
sbappendaddr(struct sockbuf *sb, const struct sockaddr *asa,
    struct mbuf *m0, struct mbuf *control)
{
	int retval;

	SOCKBUF_LOCK(sb);
	retval = sbappendaddr_locked(sb, asa, m0, control);
	SOCKBUF_UNLOCK(sb);
	return (retval);
}

int
sbappendcontrol_locked(struct sockbuf *sb, struct mbuf *m0,
    struct mbuf *control)
{
	struct mbuf *m, *n, *mlast;
	int space;

	SOCKBUF_LOCK_ASSERT(sb);

	if (control == 0)
		panic("sbappendcontrol_locked");
	space = m_length(control, &n) + m_length(m0, NULL);

	if (space > sbspace(sb))
		return (0);
	
	CHECK_ADD_LINKCNT(n, m0, NULL, "sbappendcontrol_locked");	
	n->m_next = m0;			/* concatenate data to control */

	SBLASTRECORDCHK(sb);

	for (m = control; m->m_next; m = m->m_next)
		sballoc(sb, m);
	sballoc(sb, m);
	mlast = m;
	SBLINKRECORD(sb, control);

	sb->sb_mbtail = mlast;
	SBLASTMBUFCHK(sb);

	SBLASTRECORDCHK(sb);
	return (1);
}

int
sbappendcontrol(struct sockbuf *sb, struct mbuf *m0, struct mbuf *control)
{
	int retval;

	SOCKBUF_LOCK(sb);
	retval = sbappendcontrol_locked(sb, m0, control);
	SOCKBUF_UNLOCK(sb);
	return (retval);
}

/*
 * Append the data in mbuf chain (m) into the socket buffer sb following mbuf
 * (n).  If (n) is NULL, the buffer is presumed empty.
 *
 * When the data is compressed, mbufs in the chain may be handled in one of
 * three ways:
 *
 * (1) The mbuf may simply be dropped, if it contributes nothing (no data, no
 *     record boundary, and no change in data type).
 *
 * (2) The mbuf may be coalesced -- i.e., data in the mbuf may be copied into
 *     an mbuf already in the socket buffer.  This can occur if an
 *     appropriate mbuf exists, there is room, and no merging of data types
 *     will occur.
 *
 * (3) The mbuf may be appended to the end of the existing mbuf chain.
 *
 * If any of the new mbufs is marked as M_EOR, mark the last mbuf appended as
 * end-of-record.
 */
void
sbcompress(struct sockbuf *sb, struct mbuf *m, struct mbuf *n)
{
	int eor = 0;
	struct mbuf *o;

	SOCKBUF_LOCK_ASSERT(sb);

	while (m) {
		eor |= m->m_flags & M_EOR;
		if (m->m_len == 0 &&
		    (eor == 0 ||
		     (((o = m->m_next) || (o = n)) &&
		      o->m_type == m->m_type))) {
			if (sb->sb_lastrecord == m)
				sb->sb_lastrecord = m->m_next;
			m = m_free(m);
			continue;
		}
		if (n && (n->m_flags & M_EOR) == 0 &&
		    M_WRITABLE(n) &&
		    ((sb->sb_flags & SB_NOCOALESCE) == 0) &&
		    m->m_len <= MCLBYTES / 4 && /* XXX: Don't copy too much */
		    m->m_len <= M_TRAILINGSPACE(n) &&
		    n->m_type == m->m_type) {
			bcopy(mtod(m, caddr_t), mtod(n, caddr_t) + n->m_len,
			    (unsigned)m->m_len);
			n->m_len += m->m_len;
			sb->sb_cc += m->m_len;
			if (m->m_type != MT_DATA && m->m_type != MT_OOBDATA)
				/* XXX: Probably don't need.*/
				sb->sb_ctl += m->m_len;
			m = m_free(m);
			continue;
		}
		if (n){
			CHECK_ADD_LINKCNT(n, m, NULL, "sbcompress");			
			n->m_next = m;
		}else
			sb->sb_mb = m;
		sb->sb_mbtail = m;
		sballoc(sb, m);
		n = m;
		m->m_flags &= ~M_EOR;
		m = m->m_next;
		CHECK_SPLIT_LINKCNT(n, n->m_next, "sbcompress");
		n->m_next = 0;
	}
	if (eor) {
		KASSERT(n != NULL, ("sbcompress: eor && n == NULL"));
		n->m_flags |= eor;
	}
	SBLASTMBUFCHK(sb);
}

/*
 * Free all mbufs in a sockbuf.  Check that all resources are reclaimed.
 */
static void
sbflush_internal(struct sockbuf *sb)
{

	while (sb->sb_mbcnt) {
		/*
		 * Don't call sbdrop(sb, 0) if the leading mbuf is non-empty:
		 * we would loop forever. Panic instead.
		 */
		if (!sb->sb_cc && (sb->sb_mb == NULL || sb->sb_mb->m_len))
			break;
		sbdrop_internal(sb, (int)sb->sb_cc);
	}
	if (sb->sb_cc || sb->sb_mb || sb->sb_mbcnt)
		panic("sbflush_internal: cc %u || mb %p || mbcnt %u",
		    sb->sb_cc, (void *)sb->sb_mb, sb->sb_mbcnt);
}

void
sbflush_locked(struct sockbuf *sb)
{

	SOCKBUF_LOCK_ASSERT(sb);
	sbflush_internal(sb);
}

void
sbflush(struct sockbuf *sb)
{

	SOCKBUF_LOCK(sb);
	sbflush_locked(sb);
	SOCKBUF_UNLOCK(sb);
}

/*
 * Drop data from (the front of) a sockbuf.
 */
static void
sbdrop_internal(struct sockbuf *sb, int len)
{
	struct mbuf *m;
	struct mbuf *next;

	next = (m = sb->sb_mb) ? m->m_nextpkt : 0;
	while (len > 0) {
		if (m == 0) {
			if (next == 0)
				panic("sbdrop");
			m = next;
			next = m->m_nextpkt;
			continue;
		}
		if (m->m_len > len) {
			m->m_len -= len;
			m->m_data += len;
			sb->sb_cc -= len;
			if (sb->sb_sndptroff != 0)
				sb->sb_sndptroff -= len;
			if (m->m_type != MT_DATA && m->m_type != MT_OOBDATA)
				sb->sb_ctl -= len;
			break;
		}
		len -= m->m_len;
		sbfree(sb, m);
		m = m_free(m);
	}
	while (m && m->m_len == 0) {
		sbfree(sb, m);
		m = m_free(m);
	}
	if (m) {
		sb->sb_mb = m;
		m->m_nextpkt = next;
	} else
		sb->sb_mb = next;
	/*
	 * First part is an inline SB_EMPTY_FIXUP().  Second part makes sure
	 * sb_lastrecord is up-to-date if we dropped part of the last record.
	 */
	m = sb->sb_mb;
	if (m == NULL) {
		sb->sb_mbtail = NULL;
		sb->sb_lastrecord = NULL;
	} else if (m->m_nextpkt == NULL) {
		sb->sb_lastrecord = m;
	}
}

/*
 * Drop data from (the front of) a sockbuf.
 */
void
sbdrop_locked(struct sockbuf *sb, int len)
{

	SOCKBUF_LOCK_ASSERT(sb);

	sbdrop_internal(sb, len);
}

void
sbdrop(struct sockbuf *sb, int len)
{

	SOCKBUF_LOCK(sb);
	sbdrop_locked(sb, len);
	SOCKBUF_UNLOCK(sb);
}

/*
 * Maintain a pointer and offset pair into the socket buffer mbuf chain to
 * avoid traversal of the entire socket buffer for larger offsets.
 */
struct mbuf *
sbsndptr(struct sockbuf *sb, u_int off, u_int len, u_int *moff)
{
	struct mbuf *m, *ret;

	KASSERT(sb->sb_mb != NULL, ("%s: sb_mb is NULL", __func__));
	KASSERT(off + len <= sb->sb_cc, ("%s: beyond sb", __func__));
	KASSERT(sb->sb_sndptroff <= sb->sb_cc, ("%s: sndptroff broken", __func__));

	/*
	 * Is off below stored offset? Happens on retransmits.
	 * Just return, we can't help here.
	 */
	if (sb->sb_sndptroff > off) {
		*moff = off;
		return (sb->sb_mb);
	}

	/* Return closest mbuf in chain for current offset. */
	*moff = off - sb->sb_sndptroff;
	m = ret = sb->sb_sndptr ? sb->sb_sndptr : sb->sb_mb;

	/* Advance by len to be as close as possible for the next transmit. */
	for (off = off - sb->sb_sndptroff + len - 1;
	     off > 0 && m != NULL && off >= m->m_len;
	     m = m->m_next) {
		sb->sb_sndptroff += m->m_len;
		off -= m->m_len;
	}
	if (off > 0 && m == NULL)
		panic("%s: sockbuf %p and mbuf %p clashing", __func__, sb, ret);
	sb->sb_sndptr = m;

	return (ret);
}

/*
 * Drop a record off the front of a sockbuf and move the next record to the
 * front.
 */
void
sbdroprecord_locked(struct sockbuf *sb)
{
	struct mbuf *m;

	SOCKBUF_LOCK_ASSERT(sb);

	m = sb->sb_mb;
	if (m) {
		sb->sb_mb = m->m_nextpkt;
		do {
			sbfree(sb, m);
			m = m_free(m);
		} while (m);
	}
	SB_EMPTY_FIXUP(sb);
}

/*
 * Drop a record off the front of a sockbuf and move the next record to the
 * front.
 */
void
sbdroprecord(struct sockbuf *sb)
{

	SOCKBUF_LOCK(sb);
	sbdroprecord_locked(sb);
	SOCKBUF_UNLOCK(sb);
}

/*
 * Create a "control" mbuf containing the specified data with the specified
 * type for presentation on a socket buffer.
 */
struct mbuf *
sbcreatecontrol(caddr_t p, int size, int type, int level)
{
	struct cmsghdr *cp;
	struct mbuf *m;

	if (CMSG_SPACE((u_int)size) > MCLBYTES)
		return ((struct mbuf *) NULL);
	if (CMSG_SPACE((u_int)size) > MLEN)
		m = m_getcl(M_DONTWAIT, MT_CONTROL, 0);
	else
		m = m_get(M_DONTWAIT, MT_CONTROL);
	if (m == NULL)
		return ((struct mbuf *) NULL);
	cp = mtod(m, struct cmsghdr *);
	m->m_len = 0;
	KASSERT(CMSG_SPACE((u_int)size) <= M_TRAILINGSPACE(m),
	    ("sbcreatecontrol: short mbuf"));
	if (p != NULL)
		(void)memcpy(CMSG_DATA(cp), p, size);
	m->m_len = CMSG_SPACE(size);
	cp->cmsg_len = CMSG_LEN(size);
	cp->cmsg_level = level;
	cp->cmsg_type = type;
	return (m);
}

/*
 * This does the same for socket buffers that sotoxsocket does for sockets:
 * generate an user-format data structure describing the socket buffer.  Note
 * that the xsockbuf structure, since it is always embedded in a socket, does
 * not include a self pointer nor a length.  We make this entry point public
 * in case some other mechanism needs it.
 */
void
sbtoxsockbuf(struct sockbuf *sb, struct xsockbuf *xsb)
{

	xsb->sb_cc = sb->sb_cc;
	xsb->sb_hiwat = sb->sb_hiwat;
	xsb->sb_mbcnt = sb->sb_mbcnt;
	xsb->sb_mcnt = sb->sb_mcnt;	
	xsb->sb_ccnt = sb->sb_ccnt;
	xsb->sb_mbmax = sb->sb_mbmax;
	xsb->sb_lowat = sb->sb_lowat;
	xsb->sb_flags = sb->sb_flags;
	xsb->sb_timeo = sb->sb_timeo;
}

/* This takes the place of kern.maxsockbuf, which moved to kern.ipc. */
//static int dummy;
//SYSCTL_INT(_kern, KERN_DUMMY, dummy, CTLFLAG_RW, &dummy, 0, "");
//SYSCTL_OID(_kern_ipc, KIPC_MAXSOCKBUF, maxsockbuf, CTLTYPE_ULONG|CTLFLAG_RW,
//    &sb_max, 0, sysctl_handle_sb_max, "LU", "Maximum socket buffer size");
//SYSCTL_ULONG(_kern_ipc, KIPC_SOCKBUF_WASTE, sockbuf_waste_factor, CTLFLAG_RW,
//    &sb_efficiency, 0, "");
