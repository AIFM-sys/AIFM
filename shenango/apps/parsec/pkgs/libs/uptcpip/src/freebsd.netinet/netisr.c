/*-
 * Copyright (c) 2007-2009 Robert N. M. Watson
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/bsd_cdefs.h>
//__FBSDID("$FreeBSD$");

/*
 * netisr is a packet dispatch service, allowing synchronous (directly
 * dispatched) and asynchronous (deferred dispatch) processing of packets by
 * registered protocol handlers.  Callers pass a protocol identifier and
 * packet to netisr, along with a direct dispatch hint, and work will either
 * be immediately processed with the registered handler, or passed to a
 * kernel software interrupt (SWI) thread for deferred dispatch.  Callers
 * will generally select one or the other based on:
 *
 * - Might directly dispatching a netisr handler lead to code reentrance or
 *   lock recursion, such as entering the socket code from the socket code.
 * - Might directly dispatching a netisr handler lead to recursive
 *   processing, such as when decapsulating several wrapped layers of tunnel
 *   information (IPSEC within IPSEC within ...).
 *
 * Maintaining ordering for protocol streams is a critical design concern.
 * Enforcing ordering limits the opportunity for concurrency, but maintains
 * the strong ordering requirements found in some protocols, such as TCP.  Of
 * related concern is CPU affinity--it is desirable to process all data
 * associated with a particular stream on the same CPU over time in order to
 * avoid acquiring locks associated with the connection on different CPUs,
 * keep connection data in one cache, and to generally encourage associated
 * user threads to live on the same CPU as the stream.  It's also desirable
 * to avoid lock migration and contention where locks are associated with
 * more than one flow.
 *
 * netisr supports several policy variations, represented by the
 * NETISR_POLICY_* constants, allowing protocols to play a varying role in
 * identifying flows, assigning work to CPUs, etc.  These are described in
 * detail in netisr.h.
 */

#include "bsd_opt_ddb.h"
#include "bsd_opt_device_polling.h"

#include <sys/bsd_param.h>
#include <sys/bsd_bus.h>
#include <sys/bsd_kernel.h>
#include <sys/bsd_kthread.h>
#include <sys/bsd_interrupt.h>
#include <sys/bsd_lock.h>
#include <sys/bsd_mbuf.h>
#include <sys/bsd_mutex.h>
//#include <sys/bsd_pcpu.h>
////#include <sys/bsd_proc.h>
#include <sys/bsd_rmlock.h>
//#include <sys/bsd_sched.h>
//#include <sys/bsd_smp.h>
#include <sys/bsd_socket.h>
//#include <sys/bsd_sysctl.h>
#include <sys/bsd_systm.h>

#ifdef DDB
#include <ddb/bsd_ddb.h>
#endif

#include <net/bsd_if.h>
#include <net/bsd_if_var.h>
#include <net/bsd_netisr.h>
#include <net/bsd_vnet.h>

//static int mp_maxid = 1;
//static int mp_ncpus = 1;

/*-
 * Synchronize use and modification of the registered netisr data structures;
 * acquire a read lock while modifying the set of registered protocols to
 * prevent partially registered or unregistered protocols from being run.
 *
 * The following data structures and fields are protected by this lock:
 *
 * - The np array, including all fields of struct netisr_proto.
 * - The nws array, including all fields of struct netisr_worker.
 * - The nws_array array.
 *
 * Note: the NETISR_LOCKING define controls whether read locks are acquired
 * in packet processing paths requiring netisr registration stability.  This
 * is disabled by default as it can lead to a measurable performance
 * degradation even with rmlocks (3%-6% for loopback ping-pong traffic), and
 * because netisr registration and unregistration is extremely rare at
 * runtime.  If it becomes more common, this decision should be revisited.
 *
 * XXXRW: rmlocks don't support assertions.
 */

//static struct rmlock	netisr_rmlock;
#define	NETISR_LOCK_INIT()	//rm_init_flags(&netisr_rmlock, "netisr",  RM_NOWITNESS)
#define	NETISR_LOCK_ASSERT()
#define	NETISR_RLOCK(tracker)	//rm_rlock(&netisr_rmlock, (tracker))
#define	NETISR_RUNLOCK(tracker)	//rm_runlock(&netisr_rmlock, (tracker))
#define	NETISR_WLOCK()		//rm_wlock(&netisr_rmlock)
#define	NETISR_WUNLOCK()	//rm_wunlock(&netisr_rmlock)
/* #define	NETISR_LOCKING */

//SYSCTL_NODE(_net, OID_AUTO, isr, CTLFLAG_RW, 0, "netisr");

/*-
 * Three direct dispatch policies are supported:
 *
 * - Always defer: all work is scheduled for a netisr, regardless of context.
 *   (!direct)
 *
 * - Hybrid: if the executing context allows direct dispatch, and we're
 *   running on the CPU the work would be done on, then direct dispatch if it
 *   wouldn't violate ordering constraints on the workstream.
 *   (direct && !direct_force)
 *
 * - Always direct: if the executing context allows direct dispatch, always
 *   direct dispatch.  (direct && direct_force)
 *
 * Notice that changing the global policy could lead to short periods of
 * misordered processing, but this is considered acceptable as compared to
 * the complexity of enforcing ordering during policy changes.
 */
static int	netisr_direct_force = 1;	/* Always direct dispatch. */
//TUNABLE_INT("net.isr.direct_force", &netisr_direct_force);
//SYSCTL_INT(_net_isr, OID_AUTO, direct_force, CTLFLAG_RW,
//    &netisr_direct_force, 0, "Force direct dispatch");

static int	netisr_direct = 1;	/* Enable direct dispatch. */
//TUNABLE_INT("net.isr.direct", &netisr_direct);
//SYSCTL_INT(_net_isr, OID_AUTO, direct, CTLFLAG_RW,
//    &netisr_direct, 0, "Enable direct dispatch");

/*
 * Allow the administrator to limit the number of threads (CPUs) to use for
 * netisr.  We don't check netisr_maxthreads before creating the thread for
 * CPU 0, so in practice we ignore values <= 1.  This must be set at boot.
 * We will create at most one thread per CPU.
 */
//static int	netisr_maxthreads = -1;		/* Max number of threads. */
//TUNABLE_INT("net.isr.maxthreads", &netisr_maxthreads);
//SYSCTL_INT(_net_isr, OID_AUTO, maxthreads, CTLFLAG_RD,
//    &netisr_maxthreads, 0,
//    "Use at most this many CPUs for netisr processing");

//static int	netisr_bindthreads = 0;		/* Bind threads to CPUs. */
//TUNABLE_INT("net.isr.bindthreads", &netisr_bindthreads);
//SYSCTL_INT(_net_isr, OID_AUTO, bindthreads, CTLFLAG_RD,
//    &netisr_bindthreads, 0, "Bind netisr threads to CPUs.");

/*
 * Limit per-workstream queues to at most net.isr.maxqlimit, both for initial
 * configuration and later modification using netisr_setqlimit().
 */
#define	NETISR_DEFAULT_MAXQLIMIT	10240
static u_int	netisr_maxqlimit = NETISR_DEFAULT_MAXQLIMIT;
//TUNABLE_INT("net.isr.maxqlimit", &netisr_maxqlimit);
//SYSCTL_INT(_net_isr, OID_AUTO, maxqlimit, CTLFLAG_RD,
//    &netisr_maxqlimit, 0,
//    "Maximum netisr per-protocol, per-CPU queue depth.");

/*
 * The default per-workstream queue limit for protocols that don't initialize
 * the nh_qlimit field of their struct netisr_handler.  If this is set above
 * netisr_maxqlimit, we truncate it to the maximum during boot.
 */
#define	NETISR_DEFAULT_DEFAULTQLIMIT	256
static u_int	netisr_defaultqlimit = NETISR_DEFAULT_DEFAULTQLIMIT;
//TUNABLE_INT("net.isr.defaultqlimit", &netisr_defaultqlimit);
//SYSCTL_INT(_net_isr, OID_AUTO, defaultqlimit, CTLFLAG_RD,
//    &netisr_defaultqlimit, 0,
//    "Default netisr per-protocol, per-CPU queue limit if not set by protocol");

/*
 * Each protocol is described by a struct netisr_proto, which holds all
 * global per-protocol information.  This data structure is set up by
 * netisr_register(), and derived from the public struct netisr_handler.
 */
struct netisr_proto {
	const char	*np_name;	/* Character string protocol name. */
	netisr_handler_t *np_handler;	/* Protocol handler. */
	netisr_m2flow_t	*np_m2flow;	/* Query flow for untagged packet. */
	netisr_m2cpuid_t *np_m2cpuid;	/* Query CPU to process packet on. */
	netisr_drainedcpu_t *np_drainedcpu; /* Callback when drained a queue. */
	u_int		 np_qlimit;	/* Maximum per-CPU queue depth. */
	u_int		 np_policy;	/* Work placement policy. */
};

#define	NETISR_MAXPROT		16		/* Compile-time limit. */

/*
 * The np array describes all registered protocols, indexed by protocol
 * number.
 */
static struct netisr_proto	np[NETISR_MAXPROT];

/*
 * Protocol-specific work for each workstream is described by struct
 * netisr_work.  Each work descriptor consists of an mbuf queue and
 * statistics.
 */
struct netisr_work {
	/*
	 * Packet queue, linked by m_nextpkt.
	 */
	struct mbuf	*nw_head;
	struct mbuf	*nw_tail;
	u_int		 nw_len;
	u_int		 nw_qlimit;
	u_int		 nw_watermark;

	/*
	 * Statistics -- written unlocked, but mostly from curcpu.
	 */
	u_int64_t	 nw_dispatched; /* Number of direct dispatches. */
	u_int64_t	 nw_hybrid_dispatched; /* "" hybrid dispatches. */
	u_int64_t	 nw_qdrops;	/* "" drops. */
	u_int64_t	 nw_queued;	/* "" enqueues. */
	u_int64_t	 nw_handled;	/* "" handled in worker. */
};

/*
 * Workstreams hold a set of ordered work across each protocol, and are
 * described by netisr_workstream.  Each workstream is associated with a
 * worker thread, which in turn is pinned to a CPU.  Work associated with a
 * workstream can be processd in other threads during direct dispatch;
 * concurrent processing is prevented by the NWS_RUNNING flag, which
 * indicates that a thread is already processing the work queue.
 */
struct netisr_workstream {
	struct intr_event *nws_intr_event;	/* Handler for stream. */
	void		*nws_swi_cookie;	/* swi(9) cookie for stream. */
	struct mtx	 nws_mtx;		/* Synchronize work. */
	u_int		 nws_cpu;		/* CPU pinning. */
	u_int		 nws_flags;		/* Wakeup flags. */
	u_int		 nws_pendingbits;	/* Scheduled protocols. */

	/*
	 * Each protocol has per-workstream data.
	 */
	struct netisr_work	nws_work[NETISR_MAXPROT];
} __aligned(CACHE_LINE_SIZE);

/*
 * Per-CPU workstream data.
 */
struct netisr_workstream     nws[1];
//DPCPU_DEFINE(struct netisr_workstream, nws);

/*
 * Map contiguous values between 0 and nws_count into CPU IDs appropriate for
 * accessing workstreams.  This allows constructions of the form
 * DPCPU_ID_GET(nws_array[arbitraryvalue % nws_count], nws).
 */
static u_int				 nws_array[MAXCPU];

/*
 * Number of registered workstreams.  Will be at most the number of running
 * CPUs once fully started.
 */
static u_int				 nws_count = 0;
//SYSCTL_INT(_net_isr, OID_AUTO, numthreads, CTLFLAG_RD,
//    &nws_count, 0, "Number of extant netisr threads.");

/*
 * Per-workstream flags.
 */
#define	NWS_RUNNING	0x00000001	/* Currently running in a thread. */
#define	NWS_DISPATCHING	0x00000002	/* Currently being direct-dispatched. */
#define	NWS_SCHEDULED	0x00000004	/* Signal issued. */

/*
 * Synchronization for each workstream: a mutex protects all mutable fields
 * in each stream, including per-protocol state (mbuf queues).  The SWI is
 * woken up if asynchronous dispatch is required.
 */
#define	NWS_LOCK(s)		mtx_lock(&(s)->nws_mtx)
#define	NWS_LOCK_ASSERT(s)	mtx_assert(&(s)->nws_mtx, MA_OWNED)
#define	NWS_UNLOCK(s)		mtx_unlock(&(s)->nws_mtx)
#define	NWS_SIGNAL(s)		swi_sched((s)->nws_swi_cookie, 0)

/*
 * Utility routines for protocols that implement their own mapping of flows
 * to CPUs.
 */
u_int
netisr_get_cpucount(void)
{

	return (nws_count);
}

u_int
netisr_get_cpuid(u_int cpunumber)
{

	KASSERT(cpunumber < nws_count, ("%s: %u > %u", __func__, cpunumber,
	    nws_count));

	return (nws_array[cpunumber]);
}

/*
 * The default implementation of -> CPU ID mapping.
 *
 * Non-static so that protocols can use it to map their own work to specific
 * CPUs in a manner consistent to netisr for affinity purposes.
 */
u_int
netisr_default_flow2cpu(u_int flowid)
{

	return (nws_array[flowid % nws_count]);
}

/*
 * Register a new netisr handler, which requires initializing per-protocol
 * fields for each workstream.  All netisr work is briefly suspended while
 * the protocol is installed.
 */
void
netisr_register(const struct netisr_handler *nhp)
{
	struct netisr_work *npwp;
	const char *name;
	u_int proto;

	proto = nhp->nh_proto;
	name = nhp->nh_name;

	/*
	 * Test that the requested registration is valid.
	 */
	KASSERT(nhp->nh_name != NULL,
	    ("%s: nh_name NULL for %u", __func__, proto));
	KASSERT(nhp->nh_handler != NULL,
	    ("%s: nh_handler NULL for %s", __func__, name));
	KASSERT(nhp->nh_policy == NETISR_POLICY_SOURCE ||
	    nhp->nh_policy == NETISR_POLICY_FLOW ||
	    nhp->nh_policy == NETISR_POLICY_CPU,
	    ("%s: unsupported nh_policy %u for %s", __func__,
	    nhp->nh_policy, name));
	KASSERT(nhp->nh_policy == NETISR_POLICY_FLOW ||
	    nhp->nh_m2flow == NULL,
	    ("%s: nh_policy != FLOW but m2flow defined for %s", __func__,
	    name));
	KASSERT(nhp->nh_policy == NETISR_POLICY_CPU || nhp->nh_m2cpuid == NULL,
	    ("%s: nh_policy != CPU but m2cpuid defined for %s", __func__,
	    name));
	KASSERT(nhp->nh_policy != NETISR_POLICY_CPU || nhp->nh_m2cpuid != NULL,
	    ("%s: nh_policy == CPU but m2cpuid not defined for %s", __func__,
	    name));
	KASSERT(proto < NETISR_MAXPROT,
	    ("%s(%u, %s): protocol too big", __func__, proto, name));

	/*
	 * Test that no existing registration exists for this protocol.
	 */
	NETISR_WLOCK();
	KASSERT(np[proto].np_name == NULL,
	    ("%s(%u, %s): name present", __func__, proto, name));
	KASSERT(np[proto].np_handler == NULL,
	    ("%s(%u, %s): handler present", __func__, proto, name));

	np[proto].np_name = name;
	np[proto].np_handler = nhp->nh_handler;
	np[proto].np_m2flow = nhp->nh_m2flow;
	np[proto].np_m2cpuid = nhp->nh_m2cpuid;
	np[proto].np_drainedcpu = nhp->nh_drainedcpu;
	if (nhp->nh_qlimit == 0)
		np[proto].np_qlimit = netisr_defaultqlimit;
	else if (nhp->nh_qlimit > netisr_maxqlimit) {
		printf("%s: %s requested queue limit %u capped to "
		    "net.isr.maxqlimit %u\n", __func__, name, nhp->nh_qlimit,
		    netisr_maxqlimit);
		np[proto].np_qlimit = netisr_maxqlimit;
	} else
		np[proto].np_qlimit = nhp->nh_qlimit;
	np[proto].np_policy = nhp->nh_policy;

    //baoyg: only one cpu here
	npwp = &nws->nws_work[proto];
	bzero(npwp, sizeof(*npwp));
	npwp->nw_qlimit = np[proto].np_qlimit;

/*	for (i = 0; i <= mp_maxid; i++) {
		if (CPU_ABSENT(i))
			continue;
		npwp = &(DPCPU_ID_PTR(i, nws))->nws_work[proto];
		bzero(npwp, sizeof(*npwp));
		npwp->nw_qlimit = np[proto].np_qlimit;
	}*/
	NETISR_WUNLOCK();
}

/*
 * Clear drop counters across all workstreams for a protocol.
 */
void
netisr_clearqdrops(const struct netisr_handler *nhp)
{
	struct netisr_work *npwp;
#ifdef INVARIANTS
	const char *name;
#endif
	u_int proto;

	proto = nhp->nh_proto;
#ifdef INVARIANTS
	name = nhp->nh_name;
#endif
	KASSERT(proto < NETISR_MAXPROT,
	    ("%s(%u): protocol too big for %s", __func__, proto, name));

	NETISR_WLOCK();
	KASSERT(np[proto].np_handler != NULL,
	    ("%s(%u): protocol not registered for %s", __func__, proto,
	    name));

//	for (i = 0; i <= mp_maxid; i++) {
//		if (CPU_ABSENT(i))
//			continue;
		npwp = &nws->nws_work[proto];
		npwp->nw_qdrops = 0;
//	}
	NETISR_WUNLOCK();
}

/*
 * Query the current drop counters across all workstreams for a protocol.
 */
void
netisr_getqdrops(const struct netisr_handler *nhp, u_int64_t *qdropp)
{
/*	struct netisr_work *npwp;
	struct rm_priotracker tracker;
#ifdef INVARIANTS
	const char *name;
#endif
	u_int i, proto;

	*qdropp = 0;
	proto = nhp->nh_proto;
#ifdef INVARIANTS
	name = nhp->nh_name;
#endif
	KASSERT(proto < NETISR_MAXPROT,
	    ("%s(%u): protocol too big for %s", __func__, proto, name));

	NETISR_RLOCK(&tracker);
	KASSERT(np[proto].np_handler != NULL,
	    ("%s(%u): protocol not registered for %s", __func__, proto,
	    name));

	for (i = 0; i <= mp_maxid; i++) {
		if (CPU_ABSENT(i))
			continue;
		npwp = &(DPCPU_ID_PTR(i, nws))->nws_work[proto];
		*qdropp += npwp->nw_qdrops;
	}
	NETISR_RUNLOCK(&tracker);*/
}

/*
 * Query the current queue limit for per-workstream queues for a protocol.
 */
void
netisr_getqlimit(const struct netisr_handler *nhp, u_int *qlimitp)
{
/*	struct rm_priotracker tracker;
#ifdef INVARIANTS
	const char *name;
#endif
	u_int proto;

	proto = nhp->nh_proto;
#ifdef INVARIANTS
	name = nhp->nh_name;
#endif
	KASSERT(proto < NETISR_MAXPROT,
	    ("%s(%u): protocol too big for %s", __func__, proto, name));

	NETISR_RLOCK(&tracker);
	KASSERT(np[proto].np_handler != NULL,
	    ("%s(%u): protocol not registered for %s", __func__, proto,
	    name));
	*qlimitp = np[proto].np_qlimit;
	NETISR_RUNLOCK(&tracker);*/
}

/*
 * Update the queue limit across per-workstream queues for a protocol.  We
 * simply change the limits, and don't drain overflowed packets as they will
 * (hopefully) take care of themselves shortly.
 */
int
netisr_setqlimit(const struct netisr_handler *nhp, u_int qlimit)
{
/*	struct netisr_work *npwp;
#ifdef INVARIANTS
	const char *name;
#endif
	u_int i, proto;

	if (qlimit > netisr_maxqlimit)
		return (EINVAL);

	proto = nhp->nh_proto;
#ifdef INVARIANTS
	name = nhp->nh_name;
#endif
	KASSERT(proto < NETISR_MAXPROT,
	    ("%s(%u): protocol too big for %s", __func__, proto, name));

	NETISR_WLOCK();
	KASSERT(np[proto].np_handler != NULL,
	    ("%s(%u): protocol not registered for %s", __func__, proto,
	    name));

	np[proto].np_qlimit = qlimit;
	for (i = 0; i <= mp_maxid; i++) {
		if (CPU_ABSENT(i))
			continue;
		npwp = &(DPCPU_ID_PTR(i, nws))->nws_work[proto];
		npwp->nw_qlimit = qlimit;
	}
	NETISR_WUNLOCK();*/
	return (0);
}

/*
 * Drain all packets currently held in a particular protocol work queue.
 */
static void
netisr_drain_proto(struct netisr_work *npwp)
{
	struct mbuf *m;

	/*
	 * We would assert the lock on the workstream but it's not passed in.
	 */
	while ((m = npwp->nw_head) != NULL) {
		npwp->nw_head = m->m_nextpkt;
		m->m_nextpkt = NULL;
		if (npwp->nw_head == NULL)
			npwp->nw_tail = NULL;
		npwp->nw_len--;
		m_freem(m);
	}
	KASSERT(npwp->nw_tail == NULL, ("%s: tail", __func__));
	KASSERT(npwp->nw_len == 0, ("%s: len", __func__));
}

/*
 * Remove the registration of a network protocol, which requires clearing
 * per-protocol fields across all workstreams, including freeing all mbufs in
 * the queues at time of unregister.  All work in netisr is briefly suspended
 * while this takes place.
 */
void
netisr_unregister(const struct netisr_handler *nhp)
{
	struct netisr_work *npwp;
#ifdef INVARIANTS
	const char *name;
#endif
	u_int proto;

	proto = nhp->nh_proto;
#ifdef INVARIANTS
	name = nhp->nh_name;
#endif
	KASSERT(proto < NETISR_MAXPROT,
	    ("%s(%u): protocol too big for %s", __func__, proto, name));

	NETISR_WLOCK();
	KASSERT(np[proto].np_handler != NULL,
	    ("%s(%u): protocol not registered for %s", __func__, proto,
	    name));

	np[proto].np_name = NULL;
	np[proto].np_handler = NULL;
	np[proto].np_m2flow = NULL;
	np[proto].np_m2cpuid = NULL;
	np[proto].np_qlimit = 0;
	np[proto].np_policy = 0;
//	for (i = 0; i <= mp_maxid; i++) {
//		if (CPU_ABSENT(i))
//			continue;
		npwp = &nws->nws_work[proto];
		netisr_drain_proto(npwp);
		bzero(npwp, sizeof(*npwp));
//	}
	NETISR_WUNLOCK();
}

/*
 * Look up the workstream given a packet and source identifier.  Do this by
 * checking the protocol's policy, and optionally call out to the protocol
 * for assistance if required.
 */
static struct mbuf *
netisr_select_cpuid(struct netisr_proto *npp, uintptr_t source,
    struct mbuf *m, u_int *cpuidp)
{
	struct ifnet *ifp;

	NETISR_LOCK_ASSERT();

	/*
	 * In the event we have only one worker, shortcut and deliver to it
	 * without further ado.
	 */
	if (nws_count == 1) {
		*cpuidp = nws_array[0];
		return (m);
	}

	/*
	 * What happens next depends on the policy selected by the protocol.
	 * If we want to support per-interface policies, we should do that
	 * here first.
	 */
	switch (npp->np_policy) {
	case NETISR_POLICY_CPU:
		return (npp->np_m2cpuid(m, source, cpuidp));

	case NETISR_POLICY_FLOW:
		if (!(m->m_flags & M_FLOWID) && npp->np_m2flow != NULL) {
			m = npp->np_m2flow(m, source);
			if (m == NULL)
				return (NULL);
		}
		if (m->m_flags & M_FLOWID) {
			*cpuidp =
			    netisr_default_flow2cpu(m->m_pkthdr.flowid);
			return (m);
		}
		/* FALLTHROUGH */

	case NETISR_POLICY_SOURCE:
		ifp = m->m_pkthdr.rcvif;
		if (ifp != NULL)
			*cpuidp = nws_array[(ifp->if_index + source) %
			    nws_count];
		else
			*cpuidp = nws_array[source % nws_count];
		return (m);

	default:
		panic("%s: invalid policy %u for %s", __func__,
		    npp->np_policy, npp->np_name);
	}
}

/*
 * Process packets associated with a workstream and protocol.  For reasons of
 * fairness, we process up to one complete netisr queue at a time, moving the
 * queue to a stack-local queue for processing, but do not loop refreshing
 * from the global queue.  The caller is responsible for deciding whether to
 * loop, and for setting the NWS_RUNNING flag.  The passed workstream will be
 * locked on entry and relocked before return, but will be released while
 * processing.  The number of packets processed is returned.
 */
static u_int
netisr_process_workstream_proto(struct netisr_workstream *nwsp, u_int proto)
{
	struct netisr_work local_npw, *npwp;
	u_int handled;
	struct mbuf *m;

	NETISR_LOCK_ASSERT();
	NWS_LOCK_ASSERT(nwsp);

	KASSERT(nwsp->nws_flags & NWS_RUNNING,
	    ("%s(%u): not running", __func__, proto));
	KASSERT(proto >= 0 && proto < NETISR_MAXPROT,
	    ("%s(%u): invalid proto\n", __func__, proto));

	npwp = &nwsp->nws_work[proto];
	if (npwp->nw_len == 0)
		return (0);

	/*
	 * Move the global work queue to a thread-local work queue.
	 *
	 * Notice that this means the effective maximum length of the queue
	 * is actually twice that of the maximum queue length specified in
	 * the protocol registration call.
	 */
	handled = npwp->nw_len;
	local_npw = *npwp;
	npwp->nw_head = NULL;
	npwp->nw_tail = NULL;
	npwp->nw_len = 0;
	nwsp->nws_pendingbits &= ~(1 << proto);
	NWS_UNLOCK(nwsp);
	while ((m = local_npw.nw_head) != NULL) {
		local_npw.nw_head = m->m_nextpkt;
		m->m_nextpkt = NULL;
		if (local_npw.nw_head == NULL)
			local_npw.nw_tail = NULL;
		local_npw.nw_len--;
		VNET_ASSERT(m->m_pkthdr.rcvif != NULL);
		CURVNET_SET(m->m_pkthdr.rcvif->if_vnet);
		np[proto].np_handler(m);
		CURVNET_RESTORE();
	}
	KASSERT(local_npw.nw_len == 0,
	    ("%s(%u): len %u", __func__, proto, local_npw.nw_len));
	if (np[proto].np_drainedcpu)
		np[proto].np_drainedcpu(nwsp->nws_cpu);
	NWS_LOCK(nwsp);
	npwp->nw_handled += handled;
	return (handled);
}

#if 0
/*
 * SWI handler for netisr -- processes prackets in a set of workstreams that
 * it owns, woken up by calls to NWS_SIGNAL().  If this workstream is already
 * being direct dispatched, go back to sleep and wait for the dispatching
 * thread to wake us up again.
 */
static void
swi_net(void *arg)
{
#ifdef NETISR_LOCKING
	struct rm_priotracker tracker;
#endif
	struct netisr_workstream *nwsp;
	u_int bits, prot;

	nwsp = arg;

#ifdef DEVICE_POLLING
	KASSERT(nws_count == 1,
	    ("%s: device_polling but nws_count != 1", __func__));
	netisr_poll();
#endif
#ifdef NETISR_LOCKING
	NETISR_RLOCK(&tracker);
#endif
	NWS_LOCK(nwsp);
	KASSERT(!(nwsp->nws_flags & NWS_RUNNING), ("swi_net: running"));
	if (nwsp->nws_flags & NWS_DISPATCHING)
		goto out;
	nwsp->nws_flags |= NWS_RUNNING;
	nwsp->nws_flags &= ~NWS_SCHEDULED;
	while ((bits = nwsp->nws_pendingbits) != 0) {
		while ((prot = ffs(bits)) != 0) {
			prot--;
			bits &= ~(1 << prot);
			(void)netisr_process_workstream_proto(nwsp, prot);
		}
	}
	nwsp->nws_flags &= ~NWS_RUNNING;
out:
	NWS_UNLOCK(nwsp);
#ifdef NETISR_LOCKING
	NETISR_RUNLOCK(&tracker);
#endif
#ifdef DEVICE_POLLING
	netisr_pollmore();
#endif
}
#endif //0

static int
netisr_queue_workstream(struct netisr_workstream *nwsp, u_int proto,
    struct netisr_work *npwp, struct mbuf *m, int *dosignalp)
{

	NWS_LOCK_ASSERT(nwsp);

	*dosignalp = 0;
	if (npwp->nw_len < npwp->nw_qlimit) {
		m->m_nextpkt = NULL;
		if (npwp->nw_head == NULL) {
			npwp->nw_head = m;
			npwp->nw_tail = m;
		} else {
			npwp->nw_tail->m_nextpkt = m;
			npwp->nw_tail = m;
		}
		npwp->nw_len++;
		if (npwp->nw_len > npwp->nw_watermark)
			npwp->nw_watermark = npwp->nw_len;
		nwsp->nws_pendingbits |= (1 << proto);
		if (!(nwsp->nws_flags & 
		    (NWS_RUNNING | NWS_DISPATCHING | NWS_SCHEDULED))) {
			nwsp->nws_flags |= NWS_SCHEDULED;
			*dosignalp = 1;	/* Defer until unlocked. */
		}
		npwp->nw_queued++;
		return (0);
	} else {
		m_freem(m);
		npwp->nw_qdrops++;
		return (ENOBUFS);
	}
}

static int
netisr_queue_internal(u_int proto, struct mbuf *m, u_int cpuid)
{
	struct netisr_workstream *nwsp;
	struct netisr_work *npwp;
	int dosignal, error;

#ifdef NETISR_LOCKING
	NETISR_LOCK_ASSERT();
#endif
	KASSERT(cpuid <= mp_maxid, ("%s: cpuid too big (%u, %u)", __func__,
	    cpuid, mp_maxid));
	KASSERT(!CPU_ABSENT(cpuid), ("%s: CPU %u absent", __func__, cpuid));

	dosignal = 0;
	error = 0;
	nwsp = nws;
	npwp = &nwsp->nws_work[proto];
	NWS_LOCK(nwsp);
	error = netisr_queue_workstream(nwsp, proto, npwp, m, &dosignal);
	NWS_UNLOCK(nwsp);
	if (dosignal)
		;//FIXME NWS_SIGNAL(nwsp);
	return (error);
}

int
netisr_queue_src(u_int proto, uintptr_t source, struct mbuf *m)
{
#ifdef NETISR_LOCKING
	struct rm_priotracker tracker;
#endif
	u_int cpuid;
	int error;

	KASSERT(proto < NETISR_MAXPROT,
	    ("%s: invalid proto %u", __func__, proto));

#ifdef NETISR_LOCKING
	NETISR_RLOCK(&tracker);
#endif
	KASSERT(np[proto].np_handler != NULL,
	    ("%s: invalid proto %u", __func__, proto));

	m = netisr_select_cpuid(&np[proto], source, m, &cpuid);
	if (m != NULL) {
		KASSERT(!CPU_ABSENT(cpuid), ("%s: CPU %u absent", __func__,
		    cpuid));
		error = netisr_queue_internal(proto, m, cpuid);
	} else
		error = ENOBUFS;
#ifdef NETISR_LOCKING
	NETISR_RUNLOCK(&tracker);
#endif
	return (error);
}

int
netisr_queue(u_int proto, struct mbuf *m)
{

	return (netisr_queue_src(proto, 0, m));
}

/*
 * Dispatch a packet for netisr processing, direct dispatch permitted by
 * calling context.
 */
int
netisr_dispatch_src(u_int proto, uintptr_t source, struct mbuf *m)
{
#ifdef NETISR_LOCKING
	struct rm_priotracker tracker;
#endif
	struct netisr_workstream *nwsp;
	struct netisr_work *npwp;
	int dosignal, error;
	u_int cpuid;

	/*
	 * If direct dispatch is entirely disabled, fall back on queueing.
	 */
	if (!netisr_direct)
		return (netisr_queue_src(proto, source, m));

	KASSERT(proto < NETISR_MAXPROT,
	    ("%s: invalid proto %u", __func__, proto));
#ifdef NETISR_LOCKING
	NETISR_RLOCK(&tracker);
#endif
	KASSERT(np[proto].np_handler != NULL,
	    ("%s: invalid proto %u", __func__, proto));

	/*
	 * If direct dispatch is forced, then unconditionally dispatch
	 * without a formal CPU selection.  Borrow the current CPU's stats,
	 * even if there's no worker on it.  In this case we don't update
	 * nws_flags because all netisr processing will be source ordered due
	 * to always being forced to directly dispatch.
	 */
	if (netisr_direct_force) {
		nwsp = nws;
		npwp = &nwsp->nws_work[proto];
		npwp->nw_dispatched++;
		npwp->nw_handled++;
		np[proto].np_handler(m);
		error = 0;
		goto out_unlock;
	}

	/*
	 * Otherwise, we execute in a hybrid mode where we will try to direct
	 * dispatch if we're on the right CPU and the netisr worker isn't
	 * already running.
	 */
	m = netisr_select_cpuid(&np[proto], source, m, &cpuid);
	if (m == NULL) {
		error = ENOBUFS;
		goto out_unlock;
	}
	//KASSERT(!CPU_ABSENT(cpuid), ("%s: CPU %u absent", __func__, cpuid));
	//sched_pin();
	//if (cpuid != curcpu)
	//	goto queue_fallback;
	nwsp = nws;
	npwp = &nwsp->nws_work[proto];

	/*-
	 * We are willing to direct dispatch only if three conditions hold:
	 *
	 * (1) The netisr worker isn't already running,
	 * (2) Another thread isn't already directly dispatching, and
	 * (3) The netisr hasn't already been woken up.
	 */
	NWS_LOCK(nwsp);
	if (nwsp->nws_flags & (NWS_RUNNING | NWS_DISPATCHING | NWS_SCHEDULED)) {
		error = netisr_queue_workstream(nwsp, proto, npwp, m,
		    &dosignal);
		NWS_UNLOCK(nwsp);
		if (dosignal)
			;// FIXME NWS_SIGNAL(nwsp);
		goto out_unpin;
	}

	/*
	 * The current thread is now effectively the netisr worker, so set
	 * the dispatching flag to prevent concurrent processing of the
	 * stream from another thread (even the netisr worker), which could
	 * otherwise lead to effective misordering of the stream.
	 */
	nwsp->nws_flags |= NWS_DISPATCHING;
	NWS_UNLOCK(nwsp);
	np[proto].np_handler(m);
	NWS_LOCK(nwsp);
	nwsp->nws_flags &= ~NWS_DISPATCHING;
	npwp->nw_handled++;
	npwp->nw_hybrid_dispatched++;

	/*
	 * If other work was enqueued by another thread while we were direct
	 * dispatching, we need to signal the netisr worker to do that work.
	 * In the future, we might want to do some of that work in the
	 * current thread, rather than trigger further context switches.  If
	 * so, we'll want to establish a reasonable bound on the work done in
	 * the "borrowed" context.
	 */
	if (nwsp->nws_pendingbits != 0) {
		nwsp->nws_flags |= NWS_SCHEDULED;
		dosignal = 1;
	} else
		dosignal = 0;
	NWS_UNLOCK(nwsp);
	if (dosignal)
		;// FIXME NWS_SIGNAL(nwsp);
	error = 0;
	goto out_unpin;

//queue_fallback:
	error = netisr_queue_internal(proto, m, cpuid);
out_unpin:
	;
	//sched_unpin();
out_unlock:
#ifdef NETISR_LOCKING
	NETISR_RUNLOCK(&tracker);
#endif
	return (error);
}

int
netisr_dispatch(u_int proto, struct mbuf *m)
{

	return (netisr_dispatch_src(proto, 0, m));
}

#ifdef DEVICE_POLLING
/*
 * Kernel polling borrows a netisr thread to run interface polling in; this
 * function allows kernel polling to request that the netisr thread be
 * scheduled even if no packets are pending for protocols.
 */
void
netisr_sched_poll(void)
{
	struct netisr_workstream *nwsp;

	nwsp = DPCPU_ID_PTR(nws_array[0], nws);
	NWS_SIGNAL(nwsp);
}
#endif

#if 0
static void
netisr_start_swi(u_int cpuid, struct pcpu *pc)
{
	char swiname[12];
	struct netisr_workstream *nwsp;
	int error;

	KASSERT(!CPU_ABSENT(cpuid), ("%s: CPU %u absent", __func__, cpuid));

	nwsp = nws;
	mtx_init(&nwsp->nws_mtx, "netisr_mtx", NULL, MTX_DEF);
	nwsp->nws_cpu = cpuid;
	snprintf(swiname, sizeof(swiname), "netisr %u", cpuid);
	error = swi_add(&nwsp->nws_intr_event, swiname, swi_net, nwsp,
	    SWI_NET, INTR_MPSAFE, &nwsp->nws_swi_cookie);
	if (error)
		panic("%s: swi_add %d", __func__, error);
//	pc->pc_netisr = nwsp->nws_intr_event;
//	if (netisr_bindthreads) {
//		error = intr_event_bind(nwsp->nws_intr_event, cpuid);
//		if (error != 0)
//			printf("%s: cpu %u: intr_event_bind: %d", __func__,
//			    cpuid, error);
//	}
	NETISR_WLOCK();
	nws_array[nws_count] = nwsp->nws_cpu;
	nws_count++;
	NETISR_WUNLOCK();
}

/*
 * Initialize the netisr subsystem.  We rely on BSS and static initialization
 * of most fields in global data structures.
 *
 * Start a worker thread for the boot CPU so that we can support network
 * traffic immediately in case the network stack is used before additional
 * CPUs are started (for example, diskless boot).
 */
static void
netisr_init(void *arg)
{

	KASSERT(curcpu == 0, ("%s: not on CPU 0", __func__));

	NETISR_LOCK_INIT();
	if (netisr_maxthreads < 1)
		netisr_maxthreads = 1;
	if (netisr_maxthreads > mp_ncpus) {
		printf("netisr2: forcing maxthreads from %d to %d\n",
		    netisr_maxthreads, mp_ncpus);
		netisr_maxthreads = mp_ncpus;
	}
	if (netisr_defaultqlimit > netisr_maxqlimit) {
		printf("netisr2: forcing defaultqlimit from %d to %d\n",
		    netisr_defaultqlimit, netisr_maxqlimit);
		netisr_defaultqlimit = netisr_maxqlimit;
	}
#ifdef DEVICE_POLLING
	/*
	 * The device polling code is not yet aware of how to deal with
	 * multiple netisr threads, so for the time being compiling in device
	 * polling disables parallel netisr workers.
	 */
	if (netisr_maxthreads != 1 || netisr_bindthreads != 0) {
		printf("netisr2: forcing maxthreads to 1 and bindthreads to "
		    "0 for device polling\n");
		netisr_maxthreads = 1;
		netisr_bindthreads = 0;
	}
#endif

	//netisr_start_swi(curcpu, NULL/*pcpu_find(curcpu)*/);
}
//SYSINIT(netisr_init, SI_SUB_SOFTINTR, SI_ORDER_FIRST, netisr_init, NULL);

/*
 * Start worker threads for additional CPUs.  No attempt to gracefully handle
 * work reassignment, we don't yet support dynamic reconfiguration.
 */
static void
netisr_start(void *arg)
{
//	struct pcpu *pc;

//	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
//		if (nws_count >= netisr_maxthreads)
//			break;
//		/* XXXRW: Is skipping absent CPUs still required here? */
//		if (CPU_ABSENT(pc->pc_cpuid))
//			continue;
//		/* Worker will already be present for boot CPU. */
//		if (pc->pc_netisr != NULL)
//			continue;
//		netisr_start_swi(pc->pc_cpuid, pc);
//	}
}
//SYSINIT(netisr_start, SI_SUB_SMP, SI_ORDER_MIDDLE, netisr_start, NULL);
#endif //0

#ifdef DDB
DB_SHOW_COMMAND(netisr, db_show_netisr)
{
	struct netisr_workstream *nwsp;
	struct netisr_work *nwp;
	int first, proto;
	u_int cpuid;

	db_printf("%3s %6s %5s %5s %5s %8s %8s %8s %8s\n", "CPU", "Proto",
	    "Len", "WMark", "Max", "Disp", "HDisp", "Drop", "Queue");
	for (cpuid = 0; cpuid <= mp_maxid; cpuid++) {
		if (CPU_ABSENT(cpuid))
			continue;
		nwsp = DPCPU_ID_PTR(cpuid, nws);
		if (nwsp->nws_intr_event == NULL)
			continue;
		first = 1;
		for (proto = 0; proto < NETISR_MAXPROT; proto++) {
			if (np[proto].np_handler == NULL)
				continue;
			nwp = &nwsp->nws_work[proto];
			if (first) {
				db_printf("%3d ", cpuid);
				first = 0;
			} else
				db_printf("%3s ", "");
			db_printf(
			    "%6s %5d %5d %5d %8ju %8ju %8ju %8ju\n",
			    np[proto].np_name, nwp->nw_len,
			    nwp->nw_watermark, nwp->nw_qlimit,
			    nwp->nw_dispatched, nwp->nw_hybrid_dispatched,
			    nwp->nw_qdrops, nwp->nw_queued);
		}
	}
}
#endif
