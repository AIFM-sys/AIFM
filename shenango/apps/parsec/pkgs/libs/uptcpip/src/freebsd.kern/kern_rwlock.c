/*-
 * Copyright (c) 2006 John Baldwin <jhb@FreeBSD.org>
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

/*
 * Machine independent bits of reader/writer lock implementation.
 */

#include <sys/bsd_cdefs.h>
//__FBSDID("$FreeBSD$");

//#include "opt_ddb.h"
//#include "opt_kdtrace.h"
//#include "opt_no_adaptive_rwlocks.h"

#include <sys/bsd_param.h>
//#include <sys/ktr.h>
#include <sys/bsd_kernel.h>
#include <sys/bsd_lock.h>
#include <sys/bsd_mutex.h>
//#include <sys/proc.h>
#include <sys/bsd_rwlock.h>
//#include <sys/sysctl.h>
#include <sys/bsd_systm.h>
//#include <sys/turnstile.h>

//#include <machine/cpu.h>

#include "host_serv.h"

//#define UPTCP_RWLOCK_DEBUG
#ifndef UPTCP_RWLOCK_DEBUG
#define UPTCP_RWLOCK_DEBUG_PRINT(fmt, args...)		
#else
#define UPTCP_RWLOCK_DEBUG_PRINT(fmt, args...)		\
	do{									\
		char	str[256];						\
		sprintf(str, "[DAMON.RWLOCK]: %s", (fmt));	\
		printf(str, ## args);			\
	}while(0);							
#endif


#if defined(SMP) && !defined(NO_ADAPTIVE_RWLOCKS)
#define	ADAPTIVE_RWLOCKS
#endif

#ifdef ADAPTIVE_RWLOCKS
#define	ROWNER_RETRIES	10
#define	ROWNER_LOOPS	10000
#endif

#ifdef DDB
#include <ddb/ddb.h>

static void	db_show_rwlock(struct lock_object *lock);
#endif
static void	assert_rw(struct lock_object *lock, int what);
static void	lock_rw(struct lock_object *lock, int how);
#ifdef KDTRACE_HOOKS
static int	owner_rw(struct lock_object *lock, struct thread **owner);
#endif
static int	unlock_rw(struct lock_object *lock);

struct lock_class lock_class_rw = {
	.lc_name = "rw",
	.lc_flags = LC_SLEEPLOCK | LC_RECURSABLE | LC_UPGRADABLE,
	.lc_assert = assert_rw,
#ifdef DDB
	.lc_ddb_show = db_show_rwlock,
#endif
	.lc_lock = lock_rw,
	.lc_unlock = unlock_rw,
#ifdef KDTRACE_HOOKS
	.lc_owner = owner_rw,
#endif
};

/*
 * Return a pointer to the owning thread if the lock is write-locked or
 * NULL if the lock is unlocked or read-locked.
 */
#define	rw_wowner(rw)							\
	((rw)->rw_lock & RW_LOCK_READ ? NULL :				\
	    (struct thread *)RW_OWNER((rw)->rw_lock))

/*
 * Returns if a write owner is recursed.  Write ownership is not assured
 * here and should be previously checked.
 */
#define	rw_recursed(rw)		((rw)->rw_recurse != 0)

/*
 * Return true if curthread helds the lock.
 */
#define	rw_wlocked(rw)		(rw_wowner((rw)) == curthread)

/*
 * Return a pointer to the owning thread for this lock who should receive
 * any priority lent by threads that block on this lock.  Currently this
 * is identical to rw_wowner().
 */
#define	rw_owner(rw)		rw_wowner(rw)

#ifndef INVARIANTS
#define	_rw_assert(rw, what, file, line)
#endif

void
assert_rw(struct lock_object *lock, int what)
{

	rw_assert((struct rwlock *)lock, what);
}

void
lock_rw(struct lock_object *lock, int how)
{
	struct rwlock *rw;

	rw = (struct rwlock *)lock;
	if (how)
		rw_wlock(rw);
	else
		rw_rlock(rw);
}

int
unlock_rw(struct lock_object *lock)
{
	struct rwlock *rw;

	rw = (struct rwlock *)lock;
	rw_assert(rw, RA_LOCKED | LA_NOTRECURSED);
	if (rw->rw_lock & RW_LOCK_READ) {
		rw_runlock(rw);
		return (0);
	} else {
		rw_wunlock(rw);
		return (1);
	}
}

#ifdef KDTRACE_HOOKS
int
owner_rw(struct lock_object *lock, struct thread **owner)
{
	struct rwlock *rw = (struct rwlock *)lock;
	uintptr_t x = rw->rw_lock;

	*owner = rw_wowner(rw);
	return ((x & RW_LOCK_READ) != 0 ?  (RW_READERS(x) != 0) :
	    (*owner != NULL));
}
#endif

void
rw_init_flags(struct rwlock *rw, const char *name, int opts)
{
	int flags;

	MPASS((opts & ~(RW_DUPOK | RW_NOPROFILE | RW_NOWITNESS | RW_QUIET |
	    RW_RECURSE)) == 0);
	ASSERT_ATOMIC_LOAD_PTR(rw->rw_lock,
	    ("%s: rw_lock not aligned for %s: %p", __func__, name,
	    &rw->rw_lock));

	flags = LO_UPGRADABLE;
	if (opts & RW_DUPOK)
		flags |= LO_DUPOK;
	if (opts & RW_NOPROFILE)
		flags |= LO_NOPROFILE;
	if (!(opts & RW_NOWITNESS))
		flags |= LO_WITNESS;
	if (opts & RW_RECURSE)
		flags |= LO_RECURSABLE;
	if (opts & RW_QUIET)
		flags |= LO_QUIET;

	rw->rw_lock = RW_UNLOCKED;
	rw->rw_recurse = 0;
	//lock_init(&rw->lock_object, &lock_class_rw, name, NULL, flags);
	
	rw->lock_object.lo_name = name;
	rw->lock_object.lo_flags |= flags | LO_INITIALIZED;
	rw->lock_object.lo_data = 0;
	rw->lock_object.lo_owner = 0;	
	rw->lock_object.ext_lock = host_pthread_rwlock_init();
	
}

void
rw_destroy(struct rwlock *rw)
{

	KASSERT(rw->rw_lock == RW_UNLOCKED, ("rw lock not unlocked"));
	KASSERT(rw->rw_recurse == 0, ("rw lock still recursed"));
	rw->rw_lock = RW_DESTROYED;
	//lock_destroy(&rw->lock_object);

	rw->lock_object.lo_flags &= ~LO_INITIALIZED;
	host_pthread_rwlock_destory((void*)&rw->lock_object);		
}

void
rw_sysinit(void *arg)
{
	struct rw_args *args = arg;

	rw_init(args->ra_rw, args->ra_desc);
}

void
rw_sysinit_flags(void *arg)
{
	struct rw_args_flags *args = arg;

	rw_init_flags(args->ra_rw, args->ra_desc, args->ra_flags);
}

#if 0
int
rw_wowned(struct rwlock *rw)
{

	return (rw_wowner(rw) == curthread);
}

void
_rw_wlock(struct rwlock *rw, const char *file, int line)
{

	MPASS(curthread != NULL);
	KASSERT(rw->rw_lock != RW_DESTROYED,
	    ("rw_wlock() of destroyed rwlock @ %s:%d", file, line));
	WITNESS_CHECKORDER(&rw->lock_object, LOP_NEWORDER | LOP_EXCLUSIVE, file,
	    line, NULL);
	__rw_wlock(rw, curthread, file, line);
	LOCK_LOG_LOCK("WLOCK", &rw->lock_object, 0, rw->rw_recurse, file, line);
	WITNESS_LOCK(&rw->lock_object, LOP_EXCLUSIVE, file, line);
	curthread->td_locks++;
}

int
_rw_try_wlock(struct rwlock *rw, const char *file, int line)
{
	int rval;

	KASSERT(rw->rw_lock != RW_DESTROYED,
	    ("rw_try_wlock() of destroyed rwlock @ %s:%d", file, line));

	if (rw_wlocked(rw) &&
	    (rw->lock_object.lo_flags & LO_RECURSABLE) != 0) {
		rw->rw_recurse++;
		rval = 1;
	} else
		rval = atomic_cmpset_acq_ptr(&rw->rw_lock, RW_UNLOCKED,
		    (uintptr_t)curthread);

	LOCK_LOG_TRY("WLOCK", &rw->lock_object, 0, rval, file, line);
	if (rval) {
		WITNESS_LOCK(&rw->lock_object, LOP_EXCLUSIVE | LOP_TRYLOCK,
		    file, line);
		curthread->td_locks++;
	}
	return (rval);
}

void
_rw_wunlock(struct rwlock *rw, const char *file, int line)
{

	MPASS(curthread != NULL);
	KASSERT(rw->rw_lock != RW_DESTROYED,
	    ("rw_wunlock() of destroyed rwlock @ %s:%d", file, line));
	_rw_assert(rw, RA_WLOCKED, file, line);
	curthread->td_locks--;
	WITNESS_UNLOCK(&rw->lock_object, LOP_EXCLUSIVE, file, line);
	LOCK_LOG_LOCK("WUNLOCK", &rw->lock_object, 0, rw->rw_recurse, file,
	    line);
	if (!rw_recursed(rw))
		LOCKSTAT_PROFILE_RELEASE_LOCK(LS_RW_WUNLOCK_RELEASE, rw);
	__rw_wunlock(rw, curthread, file, line);
}
/*
 * Determines whether a new reader can acquire a lock.  Succeeds if the
 * reader already owns a read lock and the lock is locked for read to
 * prevent deadlock from reader recursion.  Also succeeds if the lock
 * is unlocked and has no writer waiters or spinners.  Failing otherwise
 * prioritizes writers before readers.
 */
#define	RW_CAN_READ(_rw)						\
    ((curthread->td_rw_rlocks && (_rw) & RW_LOCK_READ) || ((_rw) &	\
    (RW_LOCK_READ | RW_LOCK_WRITE_WAITERS | RW_LOCK_WRITE_SPINNER)) ==	\
    RW_LOCK_READ)

void
_rw_rlock(struct rwlock *rw, const char *file, int line)
{
	struct turnstile *ts;
#ifdef ADAPTIVE_RWLOCKS
	volatile struct thread *owner;
	int spintries = 0;
	int i;
#endif
#ifdef LOCK_PROFILING
	uint64_t waittime = 0;
	int contested = 0;
#endif
	uintptr_t v;
#ifdef KDTRACE_HOOKS
	uint64_t spin_cnt = 0;
	uint64_t sleep_cnt = 0;
	int64_t sleep_time = 0;
#endif

	KASSERT(rw->rw_lock != RW_DESTROYED,
	    ("rw_rlock() of destroyed rwlock @ %s:%d", file, line));
	KASSERT(rw_wowner(rw) != curthread,
	    ("%s (%s): wlock already held @ %s:%d", __func__,
	    rw->lock_object.lo_name, file, line));
	WITNESS_CHECKORDER(&rw->lock_object, LOP_NEWORDER, file, line, NULL);

	for (;;) {
#ifdef KDTRACE_HOOKS
		spin_cnt++;
#endif
		/*
		 * Handle the easy case.  If no other thread has a write
		 * lock, then try to bump up the count of read locks.  Note
		 * that we have to preserve the current state of the
		 * RW_LOCK_WRITE_WAITERS flag.  If we fail to acquire a
		 * read lock, then rw_lock must have changed, so restart
		 * the loop.  Note that this handles the case of a
		 * completely unlocked rwlock since such a lock is encoded
		 * as a read lock with no waiters.
		 */
		v = rw->rw_lock;
		if (RW_CAN_READ(v)) {
			/*
			 * The RW_LOCK_READ_WAITERS flag should only be set
			 * if the lock has been unlocked and write waiters
			 * were present.
			 */
			if (atomic_cmpset_acq_ptr(&rw->rw_lock, v,
			    v + RW_ONE_READER)) {
				if (LOCK_LOG_TEST(&rw->lock_object, 0))
					CTR4(KTR_LOCK,
					    "%s: %p succeed %p -> %p", __func__,
					    rw, (void *)v,
					    (void *)(v + RW_ONE_READER));
				break;
			}
			continue;
		}
		lock_profile_obtain_lock_failed(&rw->lock_object,
		    &contested, &waittime);

#ifdef ADAPTIVE_RWLOCKS
		/*
		 * If the owner is running on another CPU, spin until
		 * the owner stops running or the state of the lock
		 * changes.
		 */
		if ((v & RW_LOCK_READ) == 0) {
			owner = (struct thread *)RW_OWNER(v);
			if (TD_IS_RUNNING(owner)) {
				if (LOCK_LOG_TEST(&rw->lock_object, 0))
					CTR3(KTR_LOCK,
					    "%s: spinning on %p held by %p",
					    __func__, rw, owner);
				while ((struct thread*)RW_OWNER(rw->rw_lock) ==
				    owner && TD_IS_RUNNING(owner)) {
					cpu_spinwait();
#ifdef KDTRACE_HOOKS
					spin_cnt++;
#endif
				}
				continue;
			}
		} else if (spintries < ROWNER_RETRIES) {
			spintries++;
			for (i = 0; i < ROWNER_LOOPS; i++) {
				v = rw->rw_lock;
				if ((v & RW_LOCK_READ) == 0 || RW_CAN_READ(v))
					break;
				cpu_spinwait();
			}
			if (i != ROWNER_LOOPS)
				continue;
		}
#endif

		/*
		 * Okay, now it's the hard case.  Some other thread already
		 * has a write lock or there are write waiters present,
		 * acquire the turnstile lock so we can begin the process
		 * of blocking.
		 */
		ts = turnstile_trywait(&rw->lock_object);

		/*
		 * The lock might have been released while we spun, so
		 * recheck its state and restart the loop if needed.
		 */
		v = rw->rw_lock;
		if (RW_CAN_READ(v)) {
			turnstile_cancel(ts);
			continue;
		}

#ifdef ADAPTIVE_RWLOCKS
		/*
		 * The current lock owner might have started executing
		 * on another CPU (or the lock could have changed
		 * owners) while we were waiting on the turnstile
		 * chain lock.  If so, drop the turnstile lock and try
		 * again.
		 */
		if ((v & RW_LOCK_READ) == 0) {
			owner = (struct thread *)RW_OWNER(v);
			if (TD_IS_RUNNING(owner)) {
				turnstile_cancel(ts);
				continue;
			}
		}
#endif

		/*
		 * The lock is held in write mode or it already has waiters.
		 */
		MPASS(!RW_CAN_READ(v));

		/*
		 * If the RW_LOCK_READ_WAITERS flag is already set, then
		 * we can go ahead and block.  If it is not set then try
		 * to set it.  If we fail to set it drop the turnstile
		 * lock and restart the loop.
		 */
		if (!(v & RW_LOCK_READ_WAITERS)) {
			if (!atomic_cmpset_ptr(&rw->rw_lock, v,
			    v | RW_LOCK_READ_WAITERS)) {
				turnstile_cancel(ts);
				continue;
			}
			if (LOCK_LOG_TEST(&rw->lock_object, 0))
				CTR2(KTR_LOCK, "%s: %p set read waiters flag",
				    __func__, rw);
		}

		/*
		 * We were unable to acquire the lock and the read waiters
		 * flag is set, so we must block on the turnstile.
		 */
		if (LOCK_LOG_TEST(&rw->lock_object, 0))
			CTR2(KTR_LOCK, "%s: %p blocking on turnstile", __func__,
			    rw);
#ifdef KDTRACE_HOOKS
		sleep_time -= lockstat_nsecs();
#endif
		turnstile_wait(ts, rw_owner(rw), TS_SHARED_QUEUE);
#ifdef KDTRACE_HOOKS
		sleep_time += lockstat_nsecs();
		sleep_cnt++;
#endif
		if (LOCK_LOG_TEST(&rw->lock_object, 0))
			CTR2(KTR_LOCK, "%s: %p resuming from turnstile",
			    __func__, rw);
	}

	/*
	 * TODO: acquire "owner of record" here.  Here be turnstile dragons
	 * however.  turnstiles don't like owners changing between calls to
	 * turnstile_wait() currently.
	 */
	LOCKSTAT_PROFILE_OBTAIN_LOCK_SUCCESS(LS_RW_RLOCK_ACQUIRE, rw, contested,
	    waittime, file, line);
	LOCK_LOG_LOCK("RLOCK", &rw->lock_object, 0, 0, file, line);
	WITNESS_LOCK(&rw->lock_object, 0, file, line);
	curthread->td_locks++;
	curthread->td_rw_rlocks++;
#ifdef KDTRACE_HOOKS
	if (sleep_time)
		LOCKSTAT_RECORD1(LS_RW_RLOCK_BLOCK, rw, sleep_time);

	/*
	 * Record only the loops spinning and not sleeping. 
	 */
	if (spin_cnt > sleep_cnt)
		LOCKSTAT_RECORD1(LS_RW_RLOCK_SPIN, rw, (spin_cnt - sleep_cnt));
#endif
}

int
_rw_try_rlock(struct rwlock *rw, const char *file, int line)
{
	uintptr_t x;

	for (;;) {
		x = rw->rw_lock;
		KASSERT(rw->rw_lock != RW_DESTROYED,
		    ("rw_try_rlock() of destroyed rwlock @ %s:%d", file, line));
		if (!(x & RW_LOCK_READ))
			break;
		if (atomic_cmpset_acq_ptr(&rw->rw_lock, x, x + RW_ONE_READER)) {
			LOCK_LOG_TRY("RLOCK", &rw->lock_object, 0, 1, file,
			    line);
			WITNESS_LOCK(&rw->lock_object, LOP_TRYLOCK, file, line);
			curthread->td_locks++;
			curthread->td_rw_rlocks++;
			return (1);
		}
	}

	LOCK_LOG_TRY("RLOCK", &rw->lock_object, 0, 0, file, line);
	return (0);
}

void
_rw_runlock(struct rwlock *rw, const char *file, int line)
{
	struct turnstile *ts;
	uintptr_t x, v, queue;

	KASSERT(rw->rw_lock != RW_DESTROYED,
	    ("rw_runlock() of destroyed rwlock @ %s:%d", file, line));
	_rw_assert(rw, RA_RLOCKED, file, line);
	curthread->td_locks--;
	curthread->td_rw_rlocks--;
	WITNESS_UNLOCK(&rw->lock_object, 0, file, line);
	LOCK_LOG_LOCK("RUNLOCK", &rw->lock_object, 0, 0, file, line);

	/* TODO: drop "owner of record" here. */

	for (;;) {
		/*
		 * See if there is more than one read lock held.  If so,
		 * just drop one and return.
		 */
		x = rw->rw_lock;
		if (RW_READERS(x) > 1) {
			if (atomic_cmpset_rel_ptr(&rw->rw_lock, x,
			    x - RW_ONE_READER)) {
				if (LOCK_LOG_TEST(&rw->lock_object, 0))
					CTR4(KTR_LOCK,
					    "%s: %p succeeded %p -> %p",
					    __func__, rw, (void *)x,
					    (void *)(x - RW_ONE_READER));
				break;
			}
			continue;
		}
		/*
		 * If there aren't any waiters for a write lock, then try
		 * to drop it quickly.
		 */
		if (!(x & RW_LOCK_WAITERS)) {
			MPASS((x & ~RW_LOCK_WRITE_SPINNER) ==
			    RW_READERS_LOCK(1));
			if (atomic_cmpset_rel_ptr(&rw->rw_lock, x,
			    RW_UNLOCKED)) {
				if (LOCK_LOG_TEST(&rw->lock_object, 0))
					CTR2(KTR_LOCK, "%s: %p last succeeded",
					    __func__, rw);
				break;
			}
			continue;
		}
		/*
		 * Ok, we know we have waiters and we think we are the
		 * last reader, so grab the turnstile lock.
		 */
		turnstile_chain_lock(&rw->lock_object);
		v = rw->rw_lock & (RW_LOCK_WAITERS | RW_LOCK_WRITE_SPINNER);
		MPASS(v & RW_LOCK_WAITERS);

		/*
		 * Try to drop our lock leaving the lock in a unlocked
		 * state.
		 *
		 * If you wanted to do explicit lock handoff you'd have to
		 * do it here.  You'd also want to use turnstile_signal()
		 * and you'd have to handle the race where a higher
		 * priority thread blocks on the write lock before the
		 * thread you wakeup actually runs and have the new thread
		 * "steal" the lock.  For now it's a lot simpler to just
		 * wakeup all of the waiters.
		 *
		 * As above, if we fail, then another thread might have
		 * acquired a read lock, so drop the turnstile lock and
		 * restart.
		 */
		x = RW_UNLOCKED;
		if (v & RW_LOCK_WRITE_WAITERS) {
			queue = TS_EXCLUSIVE_QUEUE;
			x |= (v & RW_LOCK_READ_WAITERS);
		} else
			queue = TS_SHARED_QUEUE;
		if (!atomic_cmpset_rel_ptr(&rw->rw_lock, RW_READERS_LOCK(1) | v,
		    x)) {
			turnstile_chain_unlock(&rw->lock_object);
			continue;
		}
		if (LOCK_LOG_TEST(&rw->lock_object, 0))
			CTR2(KTR_LOCK, "%s: %p last succeeded with waiters",
			    __func__, rw);

		/*
		 * Ok.  The lock is released and all that's left is to
		 * wake up the waiters.  Note that the lock might not be
		 * free anymore, but in that case the writers will just
		 * block again if they run before the new lock holder(s)
		 * release the lock.
		 */
		ts = turnstile_lookup(&rw->lock_object);
		MPASS(ts != NULL);
		turnstile_broadcast(ts, queue);
		turnstile_unpend(ts, TS_SHARED_LOCK);
		turnstile_chain_unlock(&rw->lock_object);
		break;
	}
	LOCKSTAT_PROFILE_RELEASE_LOCK(LS_RW_RUNLOCK_RELEASE, rw);
}

/*
 * This function is called when we are unable to obtain a write lock on the
 * first try.  This means that at least one other thread holds either a
 * read or write lock.
 */
void
_rw_wlock_hard(struct rwlock *rw, uintptr_t tid, const char *file, int line)
{
	struct turnstile *ts;
#ifdef ADAPTIVE_RWLOCKS
	volatile struct thread *owner;
	int spintries = 0;
	int i;
#endif
	uintptr_t v, x;
#ifdef LOCK_PROFILING
	uint64_t waittime = 0;
	int contested = 0;
#endif
#ifdef KDTRACE_HOOKS
	uint64_t spin_cnt = 0;
	uint64_t sleep_cnt = 0;
	int64_t sleep_time = 0;
#endif

	if (rw_wlocked(rw)) {
		KASSERT(rw->lock_object.lo_flags & LO_RECURSABLE,
		    ("%s: recursing but non-recursive rw %s @ %s:%d\n",
		    __func__, rw->lock_object.lo_name, file, line));
		rw->rw_recurse++;
		if (LOCK_LOG_TEST(&rw->lock_object, 0))
			CTR2(KTR_LOCK, "%s: %p recursing", __func__, rw);
		return;
	}

	if (LOCK_LOG_TEST(&rw->lock_object, 0))
		CTR5(KTR_LOCK, "%s: %s contested (lock=%p) at %s:%d", __func__,
		    rw->lock_object.lo_name, (void *)rw->rw_lock, file, line);

	while (!_rw_write_lock(rw, tid)) {
#ifdef KDTRACE_HOOKS
		spin_cnt++;
#endif
		lock_profile_obtain_lock_failed(&rw->lock_object,
		    &contested, &waittime);
#ifdef ADAPTIVE_RWLOCKS
		/*
		 * If the lock is write locked and the owner is
		 * running on another CPU, spin until the owner stops
		 * running or the state of the lock changes.
		 */
		v = rw->rw_lock;
		owner = (struct thread *)RW_OWNER(v);
		if (!(v & RW_LOCK_READ) && TD_IS_RUNNING(owner)) {
			if (LOCK_LOG_TEST(&rw->lock_object, 0))
				CTR3(KTR_LOCK, "%s: spinning on %p held by %p",
				    __func__, rw, owner);
			while ((struct thread*)RW_OWNER(rw->rw_lock) == owner &&
			    TD_IS_RUNNING(owner)) {
				cpu_spinwait();
#ifdef KDTRACE_HOOKS
				spin_cnt++;
#endif
			}
			continue;
		}
		if ((v & RW_LOCK_READ) && RW_READERS(v) &&
		    spintries < ROWNER_RETRIES) {
			if (!(v & RW_LOCK_WRITE_SPINNER)) {
				if (!atomic_cmpset_ptr(&rw->rw_lock, v,
				    v | RW_LOCK_WRITE_SPINNER)) {
					continue;
				}
			}
			spintries++;
			for (i = 0; i < ROWNER_LOOPS; i++) {
				if ((rw->rw_lock & RW_LOCK_WRITE_SPINNER) == 0)
					break;
				cpu_spinwait();
			}
#ifdef KDTRACE_HOOKS
			spin_cnt += ROWNER_LOOPS - i;
#endif
			if (i != ROWNER_LOOPS)
				continue;
		}
#endif
		ts = turnstile_trywait(&rw->lock_object);
		v = rw->rw_lock;

#ifdef ADAPTIVE_RWLOCKS
		/*
		 * The current lock owner might have started executing
		 * on another CPU (or the lock could have changed
		 * owners) while we were waiting on the turnstile
		 * chain lock.  If so, drop the turnstile lock and try
		 * again.
		 */
		if (!(v & RW_LOCK_READ)) {
			owner = (struct thread *)RW_OWNER(v);
			if (TD_IS_RUNNING(owner)) {
				turnstile_cancel(ts);
				continue;
			}
		}
#endif
		/*
		 * Check for the waiters flags about this rwlock.
		 * If the lock was released, without maintain any pending
		 * waiters queue, simply try to acquire it.
		 * If a pending waiters queue is present, claim the lock
		 * ownership and maintain the pending queue.
		 */
		x = v & (RW_LOCK_WAITERS | RW_LOCK_WRITE_SPINNER);
		if ((v & ~x) == RW_UNLOCKED) {
			x &= ~RW_LOCK_WRITE_SPINNER;
			if (atomic_cmpset_acq_ptr(&rw->rw_lock, v, tid | x)) {
				if (x)
					turnstile_claim(ts);
				else
					turnstile_cancel(ts);
				break;
			}
			turnstile_cancel(ts);
			continue;
		}
		/*
		 * If the RW_LOCK_WRITE_WAITERS flag isn't set, then try to
		 * set it.  If we fail to set it, then loop back and try
		 * again.
		 */
		if (!(v & RW_LOCK_WRITE_WAITERS)) {
			if (!atomic_cmpset_ptr(&rw->rw_lock, v,
			    v | RW_LOCK_WRITE_WAITERS)) {
				turnstile_cancel(ts);
				continue;
			}
			if (LOCK_LOG_TEST(&rw->lock_object, 0))
				CTR2(KTR_LOCK, "%s: %p set write waiters flag",
				    __func__, rw);
		}
		/*
		 * We were unable to acquire the lock and the write waiters
		 * flag is set, so we must block on the turnstile.
		 */
		if (LOCK_LOG_TEST(&rw->lock_object, 0))
			CTR2(KTR_LOCK, "%s: %p blocking on turnstile", __func__,
			    rw);
#ifdef KDTRACE_HOOKS
		sleep_time -= lockstat_nsecs();
#endif
		turnstile_wait(ts, rw_owner(rw), TS_EXCLUSIVE_QUEUE);
#ifdef KDTRACE_HOOKS
		sleep_time += lockstat_nsecs();
		sleep_cnt++;
#endif
		if (LOCK_LOG_TEST(&rw->lock_object, 0))
			CTR2(KTR_LOCK, "%s: %p resuming from turnstile",
			    __func__, rw);
#ifdef ADAPTIVE_RWLOCKS
		spintries = 0;
#endif
	}
	LOCKSTAT_PROFILE_OBTAIN_LOCK_SUCCESS(LS_RW_WLOCK_ACQUIRE, rw, contested,
	    waittime, file, line);
#ifdef KDTRACE_HOOKS
	if (sleep_time)
		LOCKSTAT_RECORD1(LS_RW_WLOCK_BLOCK, rw, sleep_time);

	/*
	 * Record only the loops spinning and not sleeping.
	 */ 
	if (spin_cnt > sleep_cnt)
		LOCKSTAT_RECORD1(LS_RW_WLOCK_SPIN, rw, (spin_cnt - sleep_cnt));
#endif
}

/*
 * This function is called if the first try at releasing a write lock failed.
 * This means that one of the 2 waiter bits must be set indicating that at
 * least one thread is waiting on this lock.
 */
void
_rw_wunlock_hard(struct rwlock *rw, uintptr_t tid, const char *file, int line)
{
	struct turnstile *ts;
	uintptr_t v;
	int queue;

	if (rw_wlocked(rw) && rw_recursed(rw)) {
		rw->rw_recurse--;
		if (LOCK_LOG_TEST(&rw->lock_object, 0))
			CTR2(KTR_LOCK, "%s: %p unrecursing", __func__, rw);
		return;
	}

	KASSERT(rw->rw_lock & (RW_LOCK_READ_WAITERS | RW_LOCK_WRITE_WAITERS),
	    ("%s: neither of the waiter flags are set", __func__));

	if (LOCK_LOG_TEST(&rw->lock_object, 0))
		CTR2(KTR_LOCK, "%s: %p contested", __func__, rw);

	turnstile_chain_lock(&rw->lock_object);
	ts = turnstile_lookup(&rw->lock_object);
	MPASS(ts != NULL);

	/*
	 * Use the same algo as sx locks for now.  Prefer waking up shared
	 * waiters if we have any over writers.  This is probably not ideal.
	 *
	 * 'v' is the value we are going to write back to rw_lock.  If we
	 * have waiters on both queues, we need to preserve the state of
	 * the waiter flag for the queue we don't wake up.  For now this is
	 * hardcoded for the algorithm mentioned above.
	 *
	 * In the case of both readers and writers waiting we wakeup the
	 * readers but leave the RW_LOCK_WRITE_WAITERS flag set.  If a
	 * new writer comes in before a reader it will claim the lock up
	 * above.  There is probably a potential priority inversion in
	 * there that could be worked around either by waking both queues
	 * of waiters or doing some complicated lock handoff gymnastics.
	 */
	v = RW_UNLOCKED;
	if (rw->rw_lock & RW_LOCK_WRITE_WAITERS) {
		queue = TS_EXCLUSIVE_QUEUE;
		v |= (rw->rw_lock & RW_LOCK_READ_WAITERS);
	} else
		queue = TS_SHARED_QUEUE;

	/* Wake up all waiters for the specific queue. */
	if (LOCK_LOG_TEST(&rw->lock_object, 0))
		CTR3(KTR_LOCK, "%s: %p waking up %s waiters", __func__, rw,
		    queue == TS_SHARED_QUEUE ? "read" : "write");
	turnstile_broadcast(ts, queue);
	atomic_store_rel_ptr(&rw->rw_lock, v);
	turnstile_unpend(ts, TS_EXCLUSIVE_LOCK);
	turnstile_chain_unlock(&rw->lock_object);
}

/*
 * Attempt to do a non-blocking upgrade from a read lock to a write
 * lock.  This will only succeed if this thread holds a single read
 * lock.  Returns true if the upgrade succeeded and false otherwise.
 */
int
_rw_try_upgrade(struct rwlock *rw, const char *file, int line)
{
	uintptr_t v, x, tid;
	struct turnstile *ts;
	int success;

	KASSERT(rw->rw_lock != RW_DESTROYED,
	    ("rw_try_upgrade() of destroyed rwlock @ %s:%d", file, line));
	_rw_assert(rw, RA_RLOCKED, file, line);

	/*
	 * Attempt to switch from one reader to a writer.  If there
	 * are any write waiters, then we will have to lock the
	 * turnstile first to prevent races with another writer
	 * calling turnstile_wait() before we have claimed this
	 * turnstile.  So, do the simple case of no waiters first.
	 */
	tid = (uintptr_t)curthread;
	success = 0;
	for (;;) {
		v = rw->rw_lock;
		if (RW_READERS(v) > 1)
			break;
		if (!(v & RW_LOCK_WAITERS)) {
			success = atomic_cmpset_ptr(&rw->rw_lock, v, tid);
			if (!success)
				continue;
			break;
		}

		/*
		 * Ok, we think we have waiters, so lock the turnstile.
		 */
		ts = turnstile_trywait(&rw->lock_object);
		v = rw->rw_lock;
		if (RW_READERS(v) > 1) {
			turnstile_cancel(ts);
			break;
		}
		/*
		 * Try to switch from one reader to a writer again.  This time
		 * we honor the current state of the waiters flags.
		 * If we obtain the lock with the flags set, then claim
		 * ownership of the turnstile.
		 */
		x = rw->rw_lock & RW_LOCK_WAITERS;
		success = atomic_cmpset_ptr(&rw->rw_lock, v, tid | x);
		if (success) {
			if (x)
				turnstile_claim(ts);
			else
				turnstile_cancel(ts);
			break;
		}
		turnstile_cancel(ts);
	}
	LOCK_LOG_TRY("WUPGRADE", &rw->lock_object, 0, success, file, line);
	if (success) {
		curthread->td_rw_rlocks--;
		WITNESS_UPGRADE(&rw->lock_object, LOP_EXCLUSIVE | LOP_TRYLOCK,
		    file, line);
		LOCKSTAT_RECORD0(LS_RW_TRYUPGRADE_UPGRADE, rw);
	}
	return (success);
}

/*
 * Downgrade a write lock into a single read lock.
 */
void
_rw_downgrade(struct rwlock *rw, const char *file, int line)
{
	struct turnstile *ts;
	uintptr_t tid, v;
	int rwait, wwait;

	KASSERT(rw->rw_lock != RW_DESTROYED,
	    ("rw_downgrade() of destroyed rwlock @ %s:%d", file, line));
	_rw_assert(rw, RA_WLOCKED | RA_NOTRECURSED, file, line);
#ifndef INVARIANTS
	if (rw_recursed(rw))
		panic("downgrade of a recursed lock");
#endif

	WITNESS_DOWNGRADE(&rw->lock_object, 0, file, line);

	/*
	 * Convert from a writer to a single reader.  First we handle
	 * the easy case with no waiters.  If there are any waiters, we
	 * lock the turnstile and "disown" the lock.
	 */
	tid = (uintptr_t)curthread;
	if (atomic_cmpset_rel_ptr(&rw->rw_lock, tid, RW_READERS_LOCK(1)))
		goto out;

	/*
	 * Ok, we think we have waiters, so lock the turnstile so we can
	 * read the waiter flags without any races.
	 */
	turnstile_chain_lock(&rw->lock_object);
	v = rw->rw_lock & RW_LOCK_WAITERS;
	rwait = v & RW_LOCK_READ_WAITERS;
	wwait = v & RW_LOCK_WRITE_WAITERS;
	MPASS(rwait | wwait);

	/*
	 * Downgrade from a write lock while preserving waiters flag
	 * and give up ownership of the turnstile.
	 */
	ts = turnstile_lookup(&rw->lock_object);
	MPASS(ts != NULL);
	if (!wwait)
		v &= ~RW_LOCK_READ_WAITERS;
	atomic_store_rel_ptr(&rw->rw_lock, RW_READERS_LOCK(1) | v);
	/*
	 * Wake other readers if there are no writers pending.  Otherwise they
	 * won't be able to acquire the lock anyway.
	 */
	if (rwait && !wwait) {
		turnstile_broadcast(ts, TS_SHARED_QUEUE);
		turnstile_unpend(ts, TS_EXCLUSIVE_LOCK);
	} else
		turnstile_disown(ts);
	turnstile_chain_unlock(&rw->lock_object);
out:
	curthread->td_rw_rlocks++;
	LOCK_LOG_LOCK("WDOWNGRADE", &rw->lock_object, 0, 0, file, line);
	LOCKSTAT_RECORD0(LS_RW_DOWNGRADE_DOWNGRADE, rw);
}

#ifdef INVARIANT_SUPPORT
#ifndef INVARIANTS
#undef _rw_assert
#endif

/*
 * In the non-WITNESS case, rw_assert() can only detect that at least
 * *some* thread owns an rlock, but it cannot guarantee that *this*
 * thread owns an rlock.
 */
void
_rw_assert(struct rwlock *rw, int what, const char *file, int line)
{

	if (panicstr != NULL)
		return;
	switch (what) {
	case RA_LOCKED:
	case RA_LOCKED | RA_RECURSED:
	case RA_LOCKED | RA_NOTRECURSED:
	case RA_RLOCKED:
#ifdef WITNESS
		witness_assert(&rw->lock_object, what, file, line);
#else
		/*
		 * If some other thread has a write lock or we have one
		 * and are asserting a read lock, fail.  Also, if no one
		 * has a lock at all, fail.
		 */
		if (rw->rw_lock == RW_UNLOCKED ||
		    (!(rw->rw_lock & RW_LOCK_READ) && (what == RA_RLOCKED ||
		    rw_wowner(rw) != curthread)))
			panic("Lock %s not %slocked @ %s:%d\n",
			    rw->lock_object.lo_name, (what == RA_RLOCKED) ?
			    "read " : "", file, line);

		if (!(rw->rw_lock & RW_LOCK_READ)) {
			if (rw_recursed(rw)) {
				if (what & RA_NOTRECURSED)
					panic("Lock %s recursed @ %s:%d\n",
					    rw->lock_object.lo_name, file,
					    line);
			} else if (what & RA_RECURSED)
				panic("Lock %s not recursed @ %s:%d\n",
				    rw->lock_object.lo_name, file, line);
		}
#endif
		break;
	case RA_WLOCKED:
	case RA_WLOCKED | RA_RECURSED:
	case RA_WLOCKED | RA_NOTRECURSED:
		if (rw_wowner(rw) != curthread)
			panic("Lock %s not exclusively locked @ %s:%d\n",
			    rw->lock_object.lo_name, file, line);
		if (rw_recursed(rw)) {
			if (what & RA_NOTRECURSED)
				panic("Lock %s recursed @ %s:%d\n",
				    rw->lock_object.lo_name, file, line);
		} else if (what & RA_RECURSED)
			panic("Lock %s not recursed @ %s:%d\n",
			    rw->lock_object.lo_name, file, line);
		break;
	case RA_UNLOCKED:
#ifdef WITNESS
		witness_assert(&rw->lock_object, what, file, line);
#else
		/*
		 * If we hold a write lock fail.  We can't reliably check
		 * to see if we hold a read lock or not.
		 */
		if (rw_wowner(rw) == curthread)
			panic("Lock %s exclusively locked @ %s:%d\n",
			    rw->lock_object.lo_name, file, line);
#endif
		break;
	default:
		panic("Unknown rw lock assertion: %d @ %s:%d", what, file,
		    line);
	}
}
#endif /* INVARIANT_SUPPORT */
#endif //0

void rw_wlock(struct rwlock* rw)
{
	UPTCP_RWLOCK_DEBUG_PRINT("rw_wlock %s, recurse = %d\n", 
			rw->lock_object.lo_name, rw->lock_object.lo_data);

	host_pthread_rwlock_wlock((void*)&rw->lock_object);

}

void rw_wunlock(struct rwlock *rw)
{
	UPTCP_RWLOCK_DEBUG_PRINT("rw_wunlock %s, recurse = %d\n", 
			rw->lock_object.lo_name, rw->lock_object.lo_data);
	
	host_pthread_rwlock_wunlock((void*)&rw->lock_object);
}

void rw_rlock(struct rwlock* rw)
{
	UPTCP_RWLOCK_DEBUG_PRINT("rw_rlock %s, recurse = %d\n", 
			rw->lock_object.lo_name, rw->lock_object.lo_data);
	
	host_pthread_rwlock_rlock((void*)&rw->lock_object);
}

void rw_runlock(struct rwlock* rw)
{
	UPTCP_RWLOCK_DEBUG_PRINT("rw_runlock %s, recurse = %d\n", 
			rw->lock_object.lo_name, rw->lock_object.lo_data);
	
	host_pthread_rwlock_runlock((void*)&rw->lock_object);
}

int  rw_try_rlock(struct rwlock* rw)
{
	UPTCP_RWLOCK_DEBUG_PRINT("rw_try_rlock %s, recurse = %d\n", 
			rw->lock_object.lo_name, rw->lock_object.lo_data);
	
	return host_pthread_rwlock_tryrlock((void*)&rw->lock_object);
}


int rw_try_upgrade(struct rwlock* rw)
{
	UPTCP_RWLOCK_DEBUG_PRINT("rw_try_upgrade %s, recurse = %d\n", 
			rw->lock_object.lo_name, rw->lock_object.lo_data);
	
	//host_pthread_rwlock_wlock(rw->lock_object->ext_lock);
	return 0;
}
int rw_try_wlock(struct rwlock* rw)
{
	UPTCP_RWLOCK_DEBUG_PRINT("rw_try_wlock %s, recurse = %d\n", 
			rw->lock_object.lo_name, rw->lock_object.lo_data);
	
	return host_pthread_rwlock_trywlock((void*)&rw->lock_object);
}

void rw_downgrade(struct rwlock *rw)
{
	UPTCP_RWLOCK_DEBUG_PRINT("rw_downgrade %s, recurse = %d\n", 
			rw->lock_object.lo_name, rw->lock_object.lo_data);
	
	//host_pthread_rwlock_wlock(rw->lock_object->ext_lock);
}

void rw_unlock(struct rwlock* rw)
{
	UPTCP_RWLOCK_DEBUG_PRINT("rw_unlock %s, recurse = %d\n", 
			rw->lock_object.lo_name, rw->lock_object.lo_data);
	
	host_pthread_rwlock_unlock((void*)&rw->lock_object);
}


#ifdef DDB
void
db_show_rwlock(struct lock_object *lock)
{
	struct rwlock *rw;
	struct thread *td;

	rw = (struct rwlock *)lock;

	db_printf(" state: ");
	if (rw->rw_lock == RW_UNLOCKED)
		db_printf("UNLOCKED\n");
	else if (rw->rw_lock == RW_DESTROYED) {
		db_printf("DESTROYED\n");
		return;
	} else if (rw->rw_lock & RW_LOCK_READ)
		db_printf("RLOCK: %ju locks\n",
		    (uintmax_t)(RW_READERS(rw->rw_lock)));
	else {
		td = rw_wowner(rw);
		db_printf("WLOCK: %p (tid %d, pid %d, \"%s\")\n", td,
		    td->td_tid, td->td_proc->p_pid, td->td_name);
		if (rw_recursed(rw))
			db_printf(" recursed: %u\n", rw->rw_recurse);
	}
	db_printf(" waiters: ");
	switch (rw->rw_lock & (RW_LOCK_READ_WAITERS | RW_LOCK_WRITE_WAITERS)) {
	case RW_LOCK_READ_WAITERS:
		db_printf("readers\n");
		break;
	case RW_LOCK_WRITE_WAITERS:
		db_printf("writers\n");
		break;
	case RW_LOCK_READ_WAITERS | RW_LOCK_WRITE_WAITERS:
		db_printf("readers and writers\n");
		break;
	default:
		db_printf("none\n");
		break;
	}
}

#endif
