/*-
 * Copyright (c) 1998 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from BSDI $Id: kern_mutex.c,v 1.1.1.1 2012/03/29 17:21:22 uid42307 Exp $
 *	and BSDI $Id: kern_mutex.c,v 1.1.1.1 2012/03/29 17:21:22 uid42307 Exp $
 */

/*
 * Machine independent bits of mutex implementation.
 */

#include <sys/bsd_cdefs.h>
//__FBSDID("$FreeBSD$");

//#include "opt_adaptive_mutexes.h"
//#include "opt_ddb.h"
//#include "bsd_opt_global.h"
//#include "opt_kdtrace.h"
//#include "opt_sched.h"

#include <sys/bsd_param.h>
#include <sys/bsd_systm.h>
//#include <sys/bus.h>
//#include <sys/conf.h>
//#include <sys/kdb.h>
#include <sys/bsd_kernel.h>
//#include <sys/ktr.h>
#include <sys/bsd_lock.h>
#include <sys/bsd_malloc.h>
#include <sys/bsd_mutex.h>
//#include <sys/proc.h>
//#include <sys/resourcevar.h>
//#include <sys/sched.h>
//#include <sys/sbuf.h>
//#include <sys/sysctl.h>
//#include <sys/turnstile.h>
//#include <sys/vmmeter.h>
//#include <sys/lock_profile.h>

#include <machine/bsd_atomic.h>
//#include <machine/bus.h>
//#include <machine/cpu.h>

//#include <ddb/ddb.h>

//#include <fs/devfs/devfs_int.h>

//#include <vm/vm.h>
//#include <vm/vm_extern.h>

#include "pthread.h"
#include "host_serv.h"

#if defined(SMP) && !defined(NO_ADAPTIVE_MUTEXES)
#define	ADAPTIVE_MUTEXES
#endif


//#define UPTCP_MUTEX_DEBUG
#ifndef UPTCP_MUTEX_DEBUG
#define UPTCP_MUTEX_DEBUG_PRINT(m, op)		
#else
extern	pthread_mutex_t global_mutex;
#define UPTCP_MUTEX_DEBUG_PRINT(m, op)		\
	do{			\
		pthread_mutex_lock(&global_mutex);	\
		printf("%10p - %10s [t=%10p, o=%10p] r=%d - %s : ", 		\
		(m), (op), (void*)pthread_self(), (void*)(m)->lock_object.lo_owner, \
		(m)->lock_object.lo_data, (m)->lock_object.lo_name);\
		dump_stackframe(6);\
		pthread_mutex_unlock(&global_mutex);	\
	}while(0)							
#endif


/*
 * Internal utility macros.
 */
#define mtx_unowned(m)	((m)->mtx_lock == MTX_UNOWNED)

#define	mtx_destroyed(m) ((m)->mtx_lock == MTX_DESTROYED)

#define	mtx_owner(m)	((struct thread *)((m)->mtx_lock & ~MTX_FLAGMASK))

static void	assert_mtx(struct lock_object *lock, int what);
#ifdef DDB
static void	db_show_mtx(struct lock_object *lock);
#endif
static void	lock_mtx(struct lock_object *lock, int how);
static void	lock_spin(struct lock_object *lock, int how);
#ifdef KDTRACE_HOOKS
static int	owner_mtx(struct lock_object *lock, struct thread **owner);
#endif
static int	unlock_mtx(struct lock_object *lock);
static int	unlock_spin(struct lock_object *lock);

/*
 * Lock classes for sleep and spin mutexes.
 */
struct lock_class lock_class_mtx_sleep = {
	.lc_name = "sleep mutex",
	.lc_flags = LC_SLEEPLOCK | LC_RECURSABLE,
	.lc_assert = assert_mtx,
#ifdef DDB
	.lc_ddb_show = db_show_mtx,
#endif
	.lc_lock = lock_mtx,
	.lc_unlock = unlock_mtx,
#ifdef KDTRACE_HOOKS
	.lc_owner = owner_mtx,
#endif
};
struct lock_class lock_class_mtx_spin = {
	.lc_name = "spin mutex",
	.lc_flags = LC_SPINLOCK | LC_RECURSABLE,
	.lc_assert = assert_mtx,
#ifdef DDB
	.lc_ddb_show = db_show_mtx,
#endif
	.lc_lock = lock_spin,
	.lc_unlock = unlock_spin,
#ifdef KDTRACE_HOOKS
	.lc_owner = owner_mtx,
#endif
};


void
lock_init(struct lock_object *lock, struct lock_class *class, const char *name,
    const char *type, int flags)
{
	
}

void
lock_destroy(struct lock_object *lock)
{

}

/*
 * System-wide mutexes
 */
struct mtx blocked_lock;
struct mtx Giant;

void
assert_mtx(struct lock_object *lock, int what)
{

	mtx_assert((struct mtx *)lock, what);
}

void
lock_mtx(struct lock_object *lock, int how)
{

	mtx_lock((struct mtx *)lock);
}

void
lock_spin(struct lock_object *lock, int how)
{

	panic("spin locks can only use msleep_spin");
}

int
unlock_mtx(struct lock_object *lock)
{
	struct mtx *m;

	m = (struct mtx *)lock;
	mtx_assert(m, MA_OWNED | MA_NOTRECURSED);
	mtx_unlock(m);
	return (0);
}

int
unlock_spin(struct lock_object *lock)
{

	panic("spin locks can only use msleep_spin");
}

#ifdef KDTRACE_HOOKS
int
owner_mtx(struct lock_object *lock, struct thread **owner)
{
	struct mtx *m = (struct mtx *)lock;

	*owner = mtx_owner(m);
	return (mtx_unowned(m) == 0);
}
#endif


#if 0
/*
 * Function versions of the inlined __mtx_* macros.  These are used by
 * modules and can also be called from assembly language if needed.
 */
void
_mtx_lock_flags(struct mtx *m, int opts, const char *file, int line)
{

	MPASS(curthread != NULL);
	KASSERT(m->mtx_lock != MTX_DESTROYED,
	    ("mtx_lock() of destroyed mutex @ %s:%d", file, line));
	KASSERT(LOCK_CLASS(&m->lock_object) == &lock_class_mtx_sleep,
	    ("mtx_lock() of spin mutex %s @ %s:%d", m->lock_object.lo_name,
	    file, line));
	WITNESS_CHECKORDER(&m->lock_object, opts | LOP_NEWORDER | LOP_EXCLUSIVE,
	    file, line, NULL);

	_get_sleep_lock(m, curthread, opts, file, line);
	LOCK_LOG_LOCK("LOCK", &m->lock_object, opts, m->mtx_recurse, file,
	    line);
	WITNESS_LOCK(&m->lock_object, opts | LOP_EXCLUSIVE, file, line);
	curthread->td_locks++;
}

void
_mtx_unlock_flags(struct mtx *m, int opts, const char *file, int line)
{
	MPASS(curthread != NULL);
	KASSERT(m->mtx_lock != MTX_DESTROYED,
	    ("mtx_unlock() of destroyed mutex @ %s:%d", file, line));
	KASSERT(LOCK_CLASS(&m->lock_object) == &lock_class_mtx_sleep,
	    ("mtx_unlock() of spin mutex %s @ %s:%d", m->lock_object.lo_name,
	    file, line));
	curthread->td_locks--;
	WITNESS_UNLOCK(&m->lock_object, opts | LOP_EXCLUSIVE, file, line);
	LOCK_LOG_LOCK("UNLOCK", &m->lock_object, opts, m->mtx_recurse, file,
	    line);
	mtx_assert(m, MA_OWNED);

	if (m->mtx_recurse == 0)
		LOCKSTAT_PROFILE_RELEASE_LOCK(LS_MTX_UNLOCK_RELEASE, m);
	_rel_sleep_lock(m, curthread, opts, file, line);
}

void
_mtx_lock_spin_flags(struct mtx *m, int opts, const char *file, int line)
{

	MPASS(curthread != NULL);
	KASSERT(m->mtx_lock != MTX_DESTROYED,
	    ("mtx_lock_spin() of destroyed mutex @ %s:%d", file, line));
	KASSERT(LOCK_CLASS(&m->lock_object) == &lock_class_mtx_spin,
	    ("mtx_lock_spin() of sleep mutex %s @ %s:%d",
	    m->lock_object.lo_name, file, line));
	if (mtx_owned(m))
		KASSERT((m->lock_object.lo_flags & LO_RECURSABLE) != 0,
	    ("mtx_lock_spin: recursed on non-recursive mutex %s @ %s:%d\n",
		    m->lock_object.lo_name, file, line));
	WITNESS_CHECKORDER(&m->lock_object, opts | LOP_NEWORDER | LOP_EXCLUSIVE,
	    file, line, NULL);
	_get_spin_lock(m, curthread, opts, file, line);
	LOCK_LOG_LOCK("LOCK", &m->lock_object, opts, m->mtx_recurse, file,
	    line);
	WITNESS_LOCK(&m->lock_object, opts | LOP_EXCLUSIVE, file, line);
}

void
_mtx_unlock_spin_flags(struct mtx *m, int opts, const char *file, int line)
{

	MPASS(curthread != NULL);
	KASSERT(m->mtx_lock != MTX_DESTROYED,
	    ("mtx_unlock_spin() of destroyed mutex @ %s:%d", file, line));
	KASSERT(LOCK_CLASS(&m->lock_object) == &lock_class_mtx_spin,
	    ("mtx_unlock_spin() of sleep mutex %s @ %s:%d",
	    m->lock_object.lo_name, file, line));
	WITNESS_UNLOCK(&m->lock_object, opts | LOP_EXCLUSIVE, file, line);
	LOCK_LOG_LOCK("UNLOCK", &m->lock_object, opts, m->mtx_recurse, file,
	    line);
	mtx_assert(m, MA_OWNED);

	_rel_spin_lock(m);
}

/*
 * The important part of mtx_trylock{,_flags}()
 * Tries to acquire lock `m.'  If this function is called on a mutex that
 * is already owned, it will recursively acquire the lock.
 */
int
_mtx_trylock(struct mtx *m, int opts, const char *file, int line)
{
#ifdef LOCK_PROFILING
	uint64_t waittime = 0;
	int contested = 0;
#endif
	int rval;

	MPASS(curthread != NULL);
	KASSERT(m->mtx_lock != MTX_DESTROYED,
	    ("mtx_trylock() of destroyed mutex @ %s:%d", file, line));
	KASSERT(LOCK_CLASS(&m->lock_object) == &lock_class_mtx_sleep,
	    ("mtx_trylock() of spin mutex %s @ %s:%d", m->lock_object.lo_name,
	    file, line));

	if (mtx_owned(m) && (m->lock_object.lo_flags & LO_RECURSABLE) != 0) {
		m->mtx_recurse++;
		atomic_set_ptr(&m->mtx_lock, MTX_RECURSED);
		rval = 1;
	} else
		rval = _obtain_lock(m, (uintptr_t)curthread);

	LOCK_LOG_TRY("LOCK", &m->lock_object, opts, rval, file, line);
	if (rval) {
		WITNESS_LOCK(&m->lock_object, opts | LOP_EXCLUSIVE | LOP_TRYLOCK,
		    file, line);
		curthread->td_locks++;
		if (m->mtx_recurse == 0)
			LOCKSTAT_PROFILE_OBTAIN_LOCK_SUCCESS(LS_MTX_LOCK_ACQUIRE,
			    m, contested, waittime, file, line);

	}

	return (rval);
}

/*
 * _mtx_lock_sleep: the tougher part of acquiring an MTX_DEF lock.
 *
 * We call this if the lock is either contested (i.e. we need to go to
 * sleep waiting for it), or if we need to recurse on it.
 */
void
_mtx_lock_sleep(struct mtx *m, uintptr_t tid, int opts, const char *file,
    int line)
{
	struct turnstile *ts;
	uintptr_t v;
#ifdef ADAPTIVE_MUTEXES
	volatile struct thread *owner;
#endif
#ifdef KTR
	int cont_logged = 0;
#endif
#ifdef LOCK_PROFILING
	int contested = 0;
	uint64_t waittime = 0;
#endif
#ifdef KDTRACE_HOOKS
	uint64_t spin_cnt = 0;
	uint64_t sleep_cnt = 0;
	int64_t sleep_time = 0;
#endif

	if (mtx_owned(m)) {
		KASSERT((m->lock_object.lo_flags & LO_RECURSABLE) != 0,
	    ("_mtx_lock_sleep: recursed on non-recursive mutex %s @ %s:%d\n",
		    m->lock_object.lo_name, file, line));
		m->mtx_recurse++;
		atomic_set_ptr(&m->mtx_lock, MTX_RECURSED);
		if (LOCK_LOG_TEST(&m->lock_object, opts))
			CTR1(KTR_LOCK, "_mtx_lock_sleep: %p recursing", m);
		return;
	}

	lock_profile_obtain_lock_failed(&m->lock_object,
		    &contested, &waittime);
	if (LOCK_LOG_TEST(&m->lock_object, opts))
		CTR4(KTR_LOCK,
		    "_mtx_lock_sleep: %s contested (lock=%p) at %s:%d",
		    m->lock_object.lo_name, (void *)m->mtx_lock, file, line);

	while (!_obtain_lock(m, tid)) {
#ifdef KDTRACE_HOOKS
		spin_cnt++;
#endif
#ifdef ADAPTIVE_MUTEXES
		/*
		 * If the owner is running on another CPU, spin until the
		 * owner stops running or the state of the lock changes.
		 */
		v = m->mtx_lock;
		if (v != MTX_UNOWNED) {
			owner = (struct thread *)(v & ~MTX_FLAGMASK);
			if (TD_IS_RUNNING(owner)) {
				if (LOCK_LOG_TEST(&m->lock_object, 0))
					CTR3(KTR_LOCK,
					    "%s: spinning on %p held by %p",
					    __func__, m, owner);
				while (mtx_owner(m) == owner &&
				    TD_IS_RUNNING(owner)) {
					cpu_spinwait();
#ifdef KDTRACE_HOOKS
					spin_cnt++;
#endif
				}
				continue;
			}
		}
#endif

		ts = turnstile_trywait(&m->lock_object);
		v = m->mtx_lock;

		/*
		 * Check if the lock has been released while spinning for
		 * the turnstile chain lock.
		 */
		if (v == MTX_UNOWNED) {
			turnstile_cancel(ts);
			continue;
		}

#ifdef ADAPTIVE_MUTEXES
		/*
		 * The current lock owner might have started executing
		 * on another CPU (or the lock could have changed
		 * owners) while we were waiting on the turnstile
		 * chain lock.  If so, drop the turnstile lock and try
		 * again.
		 */
		owner = (struct thread *)(v & ~MTX_FLAGMASK);
		if (TD_IS_RUNNING(owner)) {
			turnstile_cancel(ts);
			continue;
		}
#endif

		/*
		 * If the mutex isn't already contested and a failure occurs
		 * setting the contested bit, the mutex was either released
		 * or the state of the MTX_RECURSED bit changed.
		 */
		if ((v & MTX_CONTESTED) == 0 &&
		    !atomic_cmpset_ptr(&m->mtx_lock, v, v | MTX_CONTESTED)) {
			turnstile_cancel(ts);
			continue;
		}

		/*
		 * We definitely must sleep for this lock.
		 */
		mtx_assert(m, MA_NOTOWNED);

#ifdef KTR
		if (!cont_logged) {
			CTR6(KTR_CONTENTION,
			    "contention: %p at %s:%d wants %s, taken by %s:%d",
			    (void *)tid, file, line, m->lock_object.lo_name,
			    WITNESS_FILE(&m->lock_object),
			    WITNESS_LINE(&m->lock_object));
			cont_logged = 1;
		}
#endif

		/*
		 * Block on the turnstile.
		 */
#ifdef KDTRACE_HOOKS
		sleep_time -= lockstat_nsecs();
#endif
		turnstile_wait(ts, mtx_owner(m), TS_EXCLUSIVE_QUEUE);
#ifdef KDTRACE_HOOKS
		sleep_time += lockstat_nsecs();
		sleep_cnt++;
#endif
	}
#ifdef KTR
	if (cont_logged) {
		CTR4(KTR_CONTENTION,
		    "contention end: %s acquired by %p at %s:%d",
		    m->lock_object.lo_name, (void *)tid, file, line);
	}
#endif
	LOCKSTAT_PROFILE_OBTAIN_LOCK_SUCCESS(LS_MTX_LOCK_ACQUIRE, m, contested,
	    waittime, file, line);
#ifdef KDTRACE_HOOKS
	if (sleep_time)
		LOCKSTAT_RECORD1(LS_MTX_LOCK_BLOCK, m, sleep_time);

	/*
	 * Only record the loops spinning and not sleeping. 
	 */
	if (spin_cnt > sleep_cnt)
		LOCKSTAT_RECORD1(LS_MTX_LOCK_SPIN, m, (spin_cnt - sleep_cnt));
#endif
}

static void
_mtx_lock_spin_failed(struct mtx *m)
{
	struct thread *td;

	td = mtx_owner(m);

	/* If the mutex is unlocked, try again. */
	if (td == NULL)
		return;

	printf( "spin lock %p (%s) held by %p (tid %d) too long\n",
	    m, m->lock_object.lo_name, td, td->td_tid);
#ifdef WITNESS
	witness_display_spinlock(&m->lock_object, td);
#endif
	panic("spin lock held too long");
}

#ifdef SMP
/*
 * _mtx_lock_spin: the tougher part of acquiring an MTX_SPIN lock.
 *
 * This is only called if we need to actually spin for the lock. Recursion
 * is handled inline.
 */
void
_mtx_lock_spin(struct mtx *m, uintptr_t tid, int opts, const char *file,
    int line)
{
	int i = 0;
#ifdef LOCK_PROFILING
	int contested = 0;
	uint64_t waittime = 0;
#endif

	if (LOCK_LOG_TEST(&m->lock_object, opts))
		CTR1(KTR_LOCK, "_mtx_lock_spin: %p spinning", m);

	lock_profile_obtain_lock_failed(&m->lock_object, &contested, &waittime);
	while (!_obtain_lock(m, tid)) {

		/* Give interrupts a chance while we spin. */
		spinlock_exit();
		while (m->mtx_lock != MTX_UNOWNED) {
			if (i++ < 10000000) {
				cpu_spinwait();
				continue;
			}
			if (i < 60000000 || kdb_active || panicstr != NULL)
				DELAY(1);
			else
				_mtx_lock_spin_failed(m);
			cpu_spinwait();
		}
		spinlock_enter();
	}

	if (LOCK_LOG_TEST(&m->lock_object, opts))
		CTR1(KTR_LOCK, "_mtx_lock_spin: %p spin done", m);

	LOCKSTAT_PROFILE_OBTAIN_LOCK_SUCCESS(LS_MTX_SPIN_LOCK_ACQUIRE, m,
	    contested, waittime, (file), (line));
	LOCKSTAT_RECORD1(LS_MTX_SPIN_LOCK_SPIN, m, i);
}
#endif /* SMP */

void
_thread_lock_flags(struct thread *td, int opts, const char *file, int line)
{
	struct mtx *m;
	uintptr_t tid;
	int i;
#ifdef LOCK_PROFILING
	int contested = 0;
	uint64_t waittime = 0;
#endif
#ifdef KDTRACE_HOOKS
	uint64_t spin_cnt = 0;
#endif

	i = 0;
	tid = (uintptr_t)curthread;
	for (;;) {
retry:
		spinlock_enter();
		m = td->td_lock;
		KASSERT(m->mtx_lock != MTX_DESTROYED,
		    ("thread_lock() of destroyed mutex @ %s:%d", file, line));
		KASSERT(LOCK_CLASS(&m->lock_object) == &lock_class_mtx_spin,
		    ("thread_lock() of sleep mutex %s @ %s:%d",
		    m->lock_object.lo_name, file, line));
		if (mtx_owned(m))
			KASSERT((m->lock_object.lo_flags & LO_RECURSABLE) != 0,
	    ("thread_lock: recursed on non-recursive mutex %s @ %s:%d\n",
			    m->lock_object.lo_name, file, line));
		WITNESS_CHECKORDER(&m->lock_object,
		    opts | LOP_NEWORDER | LOP_EXCLUSIVE, file, line, NULL);
		while (!_obtain_lock(m, tid)) {
#ifdef KDTRACE_HOOKS
			spin_cnt++;
#endif
			if (m->mtx_lock == tid) {
				m->mtx_recurse++;
				break;
			}
			lock_profile_obtain_lock_failed(&m->lock_object,
			    &contested, &waittime);
			/* Give interrupts a chance while we spin. */
			spinlock_exit();
			while (m->mtx_lock != MTX_UNOWNED) {
				if (i++ < 10000000)
					cpu_spinwait();
				else if (i < 60000000 ||
				    kdb_active || panicstr != NULL)
					DELAY(1);
				else
					_mtx_lock_spin_failed(m);
				cpu_spinwait();
				if (m != td->td_lock)
					goto retry;
			}
			spinlock_enter();
		}
		if (m == td->td_lock)
			break;
		_rel_spin_lock(m);	/* does spinlock_exit() */
#ifdef KDTRACE_HOOKS
		spin_cnt++;
#endif
	}
	if (m->mtx_recurse == 0)
		LOCKSTAT_PROFILE_OBTAIN_LOCK_SUCCESS(LS_MTX_SPIN_LOCK_ACQUIRE,
		    m, contested, waittime, (file), (line));
	LOCK_LOG_LOCK("LOCK", &m->lock_object, opts, m->mtx_recurse, file,
	    line);
	WITNESS_LOCK(&m->lock_object, opts | LOP_EXCLUSIVE, file, line);
	LOCKSTAT_RECORD1(LS_THREAD_LOCK_SPIN, m, spin_cnt);
}

struct mtx *
thread_lock_block(struct thread *td)
{
	struct mtx *lock;

	spinlock_enter();
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	lock = td->td_lock;
	td->td_lock = &blocked_lock;
	mtx_unlock_spin(lock);

	return (lock);
}

void
thread_lock_unblock(struct thread *td, struct mtx *new)
{
	mtx_assert(new, MA_OWNED);
	MPASS(td->td_lock == &blocked_lock);
	atomic_store_rel_ptr((volatile void *)&td->td_lock, (uintptr_t)new);
	spinlock_exit();
}

void
thread_lock_set(struct thread *td, struct mtx *new)
{
	struct mtx *lock;

	mtx_assert(new, MA_OWNED);
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	lock = td->td_lock;
	td->td_lock = new;
	mtx_unlock_spin(lock);
}

/*
 * _mtx_unlock_sleep: the tougher part of releasing an MTX_DEF lock.
 *
 * We are only called here if the lock is recursed or contested (i.e. we
 * need to wake up a blocked thread).
 */
void
_mtx_unlock_sleep(struct mtx *m, int opts, const char *file, int line)
{
	struct turnstile *ts;

	if (mtx_recursed(m)) {
		if (--(m->mtx_recurse) == 0)
			atomic_clear_ptr(&m->mtx_lock, MTX_RECURSED);
		if (LOCK_LOG_TEST(&m->lock_object, opts))
			CTR1(KTR_LOCK, "_mtx_unlock_sleep: %p unrecurse", m);
		return;
	}

	/*
	 * We have to lock the chain before the turnstile so this turnstile
	 * can be removed from the hash list if it is empty.
	 */
	turnstile_chain_lock(&m->lock_object);
	ts = turnstile_lookup(&m->lock_object);
	if (LOCK_LOG_TEST(&m->lock_object, opts))
		CTR1(KTR_LOCK, "_mtx_unlock_sleep: %p contested", m);
	MPASS(ts != NULL);
	turnstile_broadcast(ts, TS_EXCLUSIVE_QUEUE);
	_release_lock_quick(m);

	/*
	 * This turnstile is now no longer associated with the mutex.  We can
	 * unlock the chain lock so a new turnstile may take it's place.
	 */
	turnstile_unpend(ts, TS_EXCLUSIVE_LOCK);
	turnstile_chain_unlock(&m->lock_object);
}

/*
 * All the unlocking of MTX_SPIN locks is done inline.
 * See the _rel_spin_lock() macro for the details.
 */

/*
 * The backing function for the INVARIANTS-enabled mtx_assert()
 */
#ifdef INVARIANT_SUPPORT
void
_mtx_assert(struct mtx *m, int what, const char *file, int line)
{

	if (panicstr != NULL || dumping)
		return;
	switch (what) {
	case MA_OWNED:
	case MA_OWNED | MA_RECURSED:
	case MA_OWNED | MA_NOTRECURSED:
		if (!mtx_owned(m))
			panic("mutex %s not owned at %s:%d",
			    m->lock_object.lo_name, file, line);
		if (mtx_recursed(m)) {
			if ((what & MA_NOTRECURSED) != 0)
				panic("mutex %s recursed at %s:%d",
				    m->lock_object.lo_name, file, line);
		} else if ((what & MA_RECURSED) != 0) {
			panic("mutex %s unrecursed at %s:%d",
			    m->lock_object.lo_name, file, line);
		}
		break;
	case MA_NOTOWNED:
		if (mtx_owned(m))
			panic("mutex %s owned at %s:%d",
			    m->lock_object.lo_name, file, line);
		break;
	default:
		panic("unknown mtx_assert at %s:%d", file, line);
	}
}
#endif

/*
 * The MUTEX_DEBUG-enabled mtx_validate()
 *
 * Most of these checks have been moved off into the LO_INITIALIZED flag
 * maintained by the witness code.
 */
#ifdef MUTEX_DEBUG

void	mtx_validate(struct mtx *);

void
mtx_validate(struct mtx *m)
{

/*
 * XXX: When kernacc() does not require Giant we can reenable this check
 */
#ifdef notyet
	/*
	 * Can't call kernacc() from early init386(), especially when
	 * initializing Giant mutex, because some stuff in kernacc()
	 * requires Giant itself.
	 */
	if (!cold)
		if (!kernacc((caddr_t)m, sizeof(m),
		    VM_PROT_READ | VM_PROT_WRITE))
			panic("Can't read and write to mutex %p", m);
#endif
}
#endif
#endif //0

/*
 * General init routine used by the MTX_SYSINIT() macro.
 */
void
mtx_sysinit(void *arg)
{
	struct mtx_args *margs = arg;

	mtx_init(margs->ma_mtx, margs->ma_desc, NULL, margs->ma_opts);
}

/*
 * Mutex initialization routine; initialize lock `m' of type contained in
 * `opts' with options contained in `opts' and name `name.'  The optional
 * lock type `type' is used as a general lock category name for use with
 * witness.
 */
void
mtx_init(struct mtx *m, const char *name, const char *type, int opts)
{
	struct lock_class *class;
	int flags;

	MPASS((opts & ~(MTX_SPIN | MTX_QUIET | MTX_RECURSE |
		MTX_NOWITNESS | MTX_DUPOK | MTX_NOPROFILE)) == 0);
	ASSERT_ATOMIC_LOAD_PTR(m->mtx_lock,
	    ("%s: mtx_lock not aligned for %s: %p", __func__, name,
	    &m->mtx_lock));

#ifdef MUTEX_DEBUG
	/* Diagnostic and error correction */
	mtx_validate(m);
#endif

	/* Determine lock class and lock flags. */
	if (opts & MTX_SPIN)
		class = &lock_class_mtx_spin;
	else
		class = &lock_class_mtx_sleep;
	flags = 0;
	if (opts & MTX_QUIET)
		flags |= LO_QUIET;
	if (opts & MTX_RECURSE)
		flags |= LO_RECURSABLE;
	if ((opts & MTX_NOWITNESS) == 0)
		flags |= LO_WITNESS;
	if (opts & MTX_DUPOK)
		flags |= LO_DUPOK;
	if (opts & MTX_NOPROFILE)
		flags |= LO_NOPROFILE;

	/* Initialize mutex. */
	m->mtx_lock = MTX_UNOWNED;
	m->mtx_recurse = 0;

	//lock_init(&m->lock_object, class, name, type, flags);

	m->lock_object.lo_name = name;
	m->lock_object.lo_flags |= flags | LO_INITIALIZED;
	m->lock_object.ext_lock = host_pthread_mutex_init(m->lock_object.lo_flags);
}

/*
 * Remove lock `m' from all_mtx queue.  We don't allow MTX_QUIET to be
 * passed in as a flag here because if the corresponding mtx_init() was
 * called with MTX_QUIET set, then it will already be set in the mutex's
 * flags.
 */
void
mtx_destroy(struct mtx *m)
{
#if 0
	if (!mtx_owned(m))
		MPASS(mtx_unowned(m));
	else {
		MPASS((m->mtx_lock & (MTX_RECURSED|MTX_CONTESTED)) == 0);

		/* Perform the non-mtx related part of mtx_unlock_spin(). */
		if (LOCK_CLASS(&m->lock_object) == &lock_class_mtx_spin)
			spinlock_exit();
		else
			curthread->td_locks--;

		lock_profile_release_lock(&m->lock_object);
		/* Tell witness this isn't locked to make it happy. */
		WITNESS_UNLOCK(&m->lock_object, LOP_EXCLUSIVE, __FILE__,
		    __LINE__);
	}
#endif //0

	m->mtx_lock = MTX_DESTROYED;
	//lock_destroy(&m->lock_object);
	
	m->lock_object.lo_flags &= ~LO_INITIALIZED;
	host_pthread_mutex_destory((void*)&m->lock_object);	
}

/*
 * Intialize the mutex code and system mutexes.  This is called from the MD
 * startup code prior to mi_startup().  The per-CPU data space needs to be
 * setup before this is called.
 */
void
mutex_init(void)
{

	/* Setup turnstiles so that sleep mutexes work. */
	//init_turnstiles();
	host_pthread_mutexattr_init();

	/*
	 * Initialize mutexes.
	 */
	mtx_init(&Giant, "Giant", NULL, MTX_DEF | MTX_RECURSE);
	mtx_init(&blocked_lock, "blocked lock", NULL, MTX_SPIN);
	blocked_lock.mtx_lock = 0xdeadc0de;	/* Always blocked. */
//	mtx_init(&proc0.p_mtx, "process lock", NULL, MTX_DEF | MTX_DUPOK);
//	mtx_init(&proc0.p_slock, "process slock", NULL, MTX_SPIN | MTX_RECURSE);
//	mtx_init(&devmtx, "cdev", NULL, MTX_DEF);
	mtx_lock(&Giant);
}

SYSINIT(fd_socket_init, SI_SUB_LOCKMGR, SI_ORDER_MIDDLE, mutex_init, NULL);


void mtx_lock(struct mtx *m)
{
	UPTCP_MUTEX_DEBUG_PRINT(m, "mtx_lock");
	host_pthread_mutex_lock((void*)&m->lock_object);
}


void mtx_lock_spin(struct mtx* m)
{
	
	UPTCP_MUTEX_DEBUG_PRINT(m, "mtx_lock_spin");
	host_pthread_mutex_lock(&m->lock_object);
}


int   mtx_trylock(struct mtx* m)
{
	UPTCP_MUTEX_DEBUG_PRINT(m, "mtx_trylock");
	int error = host_pthread_mutex_trylock((void*)&m->lock_object);

	return error;
}

void mtx_unlock(struct mtx *m)
{
	UPTCP_MUTEX_DEBUG_PRINT(m, "mtx_unlock");
	
	host_pthread_mutex_unlock((void*)&m->lock_object);
}

void mtx_unlock_spin(struct mtx * m)
{
	UPTCP_MUTEX_DEBUG_PRINT(m, "mtx_unlock_spin");
	
	host_pthread_mutex_unlock((void*)&m->lock_object);
}



#ifdef DDB
void
db_show_mtx(struct lock_object *lock)
{
	struct thread *td;
	struct mtx *m;

	m = (struct mtx *)lock;

	db_printf(" flags: {");
	if (LOCK_CLASS(lock) == &lock_class_mtx_spin)
		db_printf("SPIN");
	else
		db_printf("DEF");
	if (m->lock_object.lo_flags & LO_RECURSABLE)
		db_printf(", RECURSE");
	if (m->lock_object.lo_flags & LO_DUPOK)
		db_printf(", DUPOK");
	db_printf("}\n");
	db_printf(" state: {");
	if (mtx_unowned(m))
		db_printf("UNOWNED");
	else if (mtx_destroyed(m))
		db_printf("DESTROYED");
	else {
		db_printf("OWNED");
		if (m->mtx_lock & MTX_CONTESTED)
			db_printf(", CONTESTED");
		if (m->mtx_lock & MTX_RECURSED)
			db_printf(", RECURSED");
	}
	db_printf("}\n");
	if (!mtx_unowned(m) && !mtx_destroyed(m)) {
		td = mtx_owner(m);
		db_printf(" owner: %p (tid %d, pid %d, \"%s\")\n", td,
		    td->td_tid, td->td_proc->p_pid, td->td_name);
		if (mtx_recursed(m))
			db_printf(" recursed: %d\n", m->mtx_recurse);
	}
}
#endif
