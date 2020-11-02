/*
    Copyright 2005-2010 Intel Corporation.  All Rights Reserved.

    This file is part of Threading Building Blocks.

    Threading Building Blocks is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    Threading Building Blocks is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Threading Building Blocks; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/

#include "spin_rw_mutex_v2.h"
#include "tbb/tbb_machine.h"
#include "../tbb/itt_notify.h"

namespace tbb {

using namespace internal;

static inline bool CAS(volatile uintptr_t &addr, uintptr_t newv, uintptr_t oldv) {
    return __TBB_CompareAndSwapW((volatile void *)&addr, (intptr_t)newv, (intptr_t)oldv) == (intptr_t)oldv;
}

//! Signal that write lock is released
void spin_rw_mutex::internal_itt_releasing(spin_rw_mutex *mutex) {
    ITT_NOTIFY(sync_releasing, mutex);
#if !DO_ITT_NOTIFY
    (void)mutex;
#endif
}

bool spin_rw_mutex::internal_acquire_writer(spin_rw_mutex *mutex)
{
    ITT_NOTIFY(sync_prepare, mutex);
    atomic_backoff backoff;
    for(;;) {
        state_t s = mutex->state;
        if( !(s & BUSY) ) { // no readers, no writers
            if( CAS(mutex->state, WRITER, s) )
                break; // successfully stored writer flag
            backoff.reset(); // we could be very close to complete op.
        } else if( !(s & WRITER_PENDING) ) { // no pending writers
            __TBB_AtomicOR(&mutex->state, WRITER_PENDING);
        }
        backoff.pause();
    }
    ITT_NOTIFY(sync_acquired, mutex);
    __TBB_ASSERT( (mutex->state & BUSY)==WRITER, "invalid state of a write lock" );
    return false;
}

//! Signal that write lock is released
void spin_rw_mutex::internal_release_writer(spin_rw_mutex *mutex) {
    __TBB_ASSERT( (mutex->state & BUSY)==WRITER, "invalid state of a write lock" );
    ITT_NOTIFY(sync_releasing, mutex);
    mutex->state = 0; 
}

//! Acquire lock on given mutex.
void spin_rw_mutex::internal_acquire_reader(spin_rw_mutex *mutex) {
    ITT_NOTIFY(sync_prepare, mutex);
    atomic_backoff backoff;
    for(;;) {
        state_t s = mutex->state;
        if( !(s & (WRITER|WRITER_PENDING)) ) { // no writer or write requests
            if( CAS(mutex->state, s+ONE_READER, s) )
                break; // successfully stored increased number of readers
            backoff.reset(); // we could be very close to complete op.
        }
        backoff.pause();
    }
    ITT_NOTIFY(sync_acquired, mutex);
    __TBB_ASSERT( mutex->state & READERS, "invalid state of a read lock: no readers" );
    __TBB_ASSERT( !(mutex->state & WRITER), "invalid state of a read lock: active writer" );
}

//! Upgrade reader to become a writer.
/** Returns true if the upgrade happened without re-acquiring the lock and false if opposite */
bool spin_rw_mutex::internal_upgrade(spin_rw_mutex *mutex) {
    state_t s = mutex->state;
    __TBB_ASSERT( s & READERS, "invalid state before upgrade: no readers " );
    __TBB_ASSERT( !(s & WRITER), "invalid state before upgrade: active writer " );
    // check and set writer-pending flag
    // required conditions: either no pending writers, or we are the only reader
    // (with multiple readers and pending writer, another upgrade could have been requested)
    while( (s & READERS)==ONE_READER || !(s & WRITER_PENDING) ) {
        if( CAS(mutex->state, s | WRITER_PENDING, s) )
        {
            atomic_backoff backoff;
            ITT_NOTIFY(sync_prepare, mutex);
            while( (mutex->state & READERS) != ONE_READER ) // more than 1 reader
                backoff.pause();
            // the state should be 0...0110, i.e. 1 reader and waiting writer;
            // both new readers and writers are blocked
            __TBB_ASSERT(mutex->state == (ONE_READER | WRITER_PENDING),"invalid state when upgrading to writer");
            mutex->state = WRITER;
            ITT_NOTIFY(sync_acquired, mutex);
            __TBB_ASSERT( (mutex->state & BUSY) == WRITER, "invalid state after upgrade" );
            return true; // successfully upgraded
        } else {
            s = mutex->state; // re-read
        }
    }
    // slow reacquire
    internal_release_reader(mutex);
    return internal_acquire_writer(mutex); // always returns false
}

void spin_rw_mutex::internal_downgrade(spin_rw_mutex *mutex) {
    __TBB_ASSERT( (mutex->state & BUSY) == WRITER, "invalid state before downgrade" );
    ITT_NOTIFY(sync_releasing, mutex);
    mutex->state = ONE_READER;
    __TBB_ASSERT( mutex->state & READERS, "invalid state after downgrade: no readers" );
    __TBB_ASSERT( !(mutex->state & WRITER), "invalid state after downgrade: active writer" );
}

void spin_rw_mutex::internal_release_reader(spin_rw_mutex *mutex)
{
    __TBB_ASSERT( mutex->state & READERS, "invalid state of a read lock: no readers" );
    __TBB_ASSERT( !(mutex->state & WRITER), "invalid state of a read lock: active writer" );
    ITT_NOTIFY(sync_releasing, mutex); // release reader
    __TBB_FetchAndAddWrelease((volatile void *)&(mutex->state),-(intptr_t)ONE_READER);
}

bool spin_rw_mutex::internal_try_acquire_writer( spin_rw_mutex * mutex )
{
// for a writer: only possible to acquire if no active readers or writers
    state_t s = mutex->state; // on Itanium, this volatile load has acquire semantic
    if( !(s & BUSY) ) // no readers, no writers; mask is 1..1101
        if( CAS(mutex->state, WRITER, s) ) {
            ITT_NOTIFY(sync_acquired, mutex);
            return true; // successfully stored writer flag
        }
    return false;
}

bool spin_rw_mutex::internal_try_acquire_reader( spin_rw_mutex * mutex )
{
// for a reader: acquire if no active or waiting writers
    state_t s = mutex->state;    // on Itanium, a load of volatile variable has acquire semantic
    while( !(s & (WRITER|WRITER_PENDING)) ) // no writers
        if( CAS(mutex->state, s+ONE_READER, s) ) {
            ITT_NOTIFY(sync_acquired, mutex);
            return true; // successfully stored increased number of readers
        }
    return false;
}

} // namespace tbb
