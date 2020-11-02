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

#include <new>

// Do not include task.h directly. Use scheduler_common.h instead
#include "scheduler_common.h"
#include "governor.h"
#include "scheduler.h"
#include "itt_notify.h"

#include "tbb/cache_aligned_allocator.h"
#include "tbb/partitioner.h"

namespace tbb {

using namespace std;

namespace internal {

//------------------------------------------------------------------------
// Methods of allocate_root_proxy
//------------------------------------------------------------------------
task& allocate_root_proxy::allocate( size_t size ) {
    internal::generic_scheduler* v = governor::local_scheduler();
    __TBB_ASSERT( v, "thread did not activate a task_scheduler_init object?" );
#if __TBB_TASK_GROUP_CONTEXT
    task_prefix& p = v->innermost_running_task->prefix();

    ITT_STACK_CREATE(p.context->itt_caller);
#endif
    // New root task becomes part of the currently running task's cancellation context
    return v->allocate_task( size, __TBB_CONTEXT_ARG(NULL, p.context) );
}

void allocate_root_proxy::free( task& task ) {
    internal::generic_scheduler* v = governor::local_scheduler();
    __TBB_ASSERT( v, "thread does not have initialized task_scheduler_init object?" );
#if __TBB_TASK_GROUP_CONTEXT
    // No need to do anything here as long as there is no context -> task connection
#endif /* __TBB_TASK_GROUP_CONTEXT */
    v->free_task<local_task>( task );
}

#if __TBB_TASK_GROUP_CONTEXT
//------------------------------------------------------------------------
// Methods of allocate_root_with_context_proxy
//------------------------------------------------------------------------
task& allocate_root_with_context_proxy::allocate( size_t size ) const {
    internal::generic_scheduler* v = governor::local_scheduler();
    __TBB_ASSERT( v, "thread did not activate a task_scheduler_init object?" );
    task_prefix& p = v->innermost_running_task->prefix();
    task& t = v->allocate_task( size, __TBB_CONTEXT_ARG(NULL, &my_context) );
    // The supported usage model prohibits concurrent initial binding. Thus we 
    // do not need interlocked operations or fences here.
    if ( my_context.my_kind == task_group_context::binding_required ) {
        __TBB_ASSERT ( my_context.my_owner, "Context without owner" );
        __TBB_ASSERT ( !my_context.my_parent, "Parent context set before initial binding" );
        // If we are in the outermost task dispatch loop of a master thread, then
        // there is nothing to bind this context to, and we skip the binding part.
        if ( v->innermost_running_task != v->dummy_task ) {
            // By not using the fence here we get faster code in case of normal execution 
            // flow in exchange of a bit higher probability that in cases when cancellation 
            // is in flight we will take deeper traversal branch. Normally cache coherency 
            // mechanisms are efficient enough to deliver updated value most of the time.
            uintptr_t local_count_snapshot = ((generic_scheduler*)my_context.my_owner)->local_cancel_count;
            __TBB_store_with_release(my_context.my_parent, p.context);
            uintptr_t global_count_snapshot = __TBB_load_with_acquire(global_cancel_count);
            if ( !my_context.my_cancellation_requested ) {
                if ( local_count_snapshot == global_count_snapshot ) {
                    // It is possible that there is active cancellation request in our 
                    // parents chain. Fortunately the equality of the local and global 
                    // counters means that if this is the case it's already been propagated
                    // to our parent.
                    my_context.my_cancellation_requested = p.context->my_cancellation_requested;
                } else {
                    // Another thread was propagating cancellation request at the moment 
                    // when we set our parent, but since we do not use locks we could've 
                    // been skipped. So we have to make sure that we get the cancellation 
                    // request if one of our ancestors has been canceled.
                    my_context.propagate_cancellation_from_ancestors();
                }
            }
        }
        my_context.my_kind = task_group_context::binding_completed;
    }
    // else the context either has already been associated with its parent or is isolated
    ITT_STACK_CREATE(my_context.itt_caller);
    return t;
}

void allocate_root_with_context_proxy::free( task& task ) const {
    internal::generic_scheduler* v = governor::local_scheduler();
    __TBB_ASSERT( v, "thread does not have initialized task_scheduler_init object?" );
    // No need to do anything here as long as unbinding is performed by context destructor only.
    v->free_task<local_task>( task );
}
#endif /* __TBB_TASK_GROUP_CONTEXT */

//------------------------------------------------------------------------
// Methods of allocate_continuation_proxy
//------------------------------------------------------------------------
task& allocate_continuation_proxy::allocate( size_t size ) const {
    task& t = *((task*)this);
    __TBB_ASSERT( AssertOkay(t), NULL );
    generic_scheduler* s = governor::local_scheduler();
    task* parent = t.parent();
    t.prefix().parent = NULL;
    return s->allocate_task( size, __TBB_CONTEXT_ARG(parent, t.prefix().context) );
}

void allocate_continuation_proxy::free( task& mytask ) const {
    // Restore the parent as it was before the corresponding allocate was called.
    ((task*)this)->prefix().parent = mytask.parent();
    governor::local_scheduler()->free_task<local_task>(mytask);
}

//------------------------------------------------------------------------
// Methods of allocate_child_proxy
//------------------------------------------------------------------------
task& allocate_child_proxy::allocate( size_t size ) const {
    task& t = *((task*)this);
    __TBB_ASSERT( AssertOkay(t), NULL );
    generic_scheduler* s = governor::local_scheduler();
    return s->allocate_task( size, __TBB_CONTEXT_ARG(&t, t.prefix().context) );
}

void allocate_child_proxy::free( task& mytask ) const {
    governor::local_scheduler()->free_task<local_task>(mytask);
}

//------------------------------------------------------------------------
// Methods of allocate_additional_child_of_proxy
//------------------------------------------------------------------------
task& allocate_additional_child_of_proxy::allocate( size_t size ) const {
    parent.increment_ref_count();
    generic_scheduler* s = governor::local_scheduler();
    return s->allocate_task( size, __TBB_CONTEXT_ARG(&parent, parent.prefix().context) );
}

void allocate_additional_child_of_proxy::free( task& task ) const {
    // Undo the increment.  We do not check the result of the fetch-and-decrement.
    // We could consider be spawning the task if the fetch-and-decrement returns 1.
    // But we do not know that was the programmer's intention.
    // Furthermore, if it was the programmer's intention, the program has a fundamental
    // race condition (that we warn about in Reference manual), because the
    // reference count might have become zero before the corresponding call to
    // allocate_additional_child_of_proxy::allocate.
    parent.internal_decrement_ref_count();
    governor::local_scheduler()->free_task<local_task>(task);
}

//------------------------------------------------------------------------
// Support for auto_partitioner
//------------------------------------------------------------------------
size_t get_initial_auto_partitioner_divisor() {
    const size_t X_FACTOR = 4;
    return X_FACTOR * (governor::max_number_of_workers()+1);
}

//------------------------------------------------------------------------
// Methods of affinity_partitioner_base_v3
//------------------------------------------------------------------------
void affinity_partitioner_base_v3::resize( unsigned factor ) {
    // Check factor to avoid asking for number of workers while there might be no arena.
    size_t new_size = factor ? factor*(governor::max_number_of_workers()+1) : 0;
    if( new_size!=my_size ) {
        if( my_array ) {
            NFS_Free( my_array );
            // Following two assignments must be done here for sake of exception safety.
            my_array = NULL;
            my_size = 0;
        }
        if( new_size ) {
            my_array = static_cast<affinity_id*>(NFS_Allocate(new_size,sizeof(affinity_id), NULL ));
            memset( my_array, 0, sizeof(affinity_id)*new_size );
            my_size = new_size;
        }
    } 
}

} // namespace internal

using namespace tbb::internal;

//------------------------------------------------------------------------
// task
//------------------------------------------------------------------------

void task::internal_set_ref_count( int count ) {
    __TBB_ASSERT( count>=0, "count must not be negative" );
    __TBB_ASSERT( !(prefix().extra_state & es_ref_count_active), "ref_count race detected" );
    ITT_NOTIFY(sync_releasing, &prefix().ref_count);
    prefix().ref_count = count;
}

internal::reference_count task::internal_decrement_ref_count() {
    ITT_NOTIFY( sync_releasing, &prefix().ref_count );
    internal::reference_count k = __TBB_FetchAndDecrementWrelease( &prefix().ref_count );
    __TBB_ASSERT( k>=1, "task's reference count underflowed" );
    if( k==1 )
        ITT_NOTIFY( sync_acquired, &prefix().ref_count );
    return k-1;
}

task& task::self() {
    generic_scheduler *v = governor::local_scheduler();
    __TBB_ASSERT( v->assert_okay(), NULL );
    __TBB_ASSERT( v->innermost_running_task, NULL );
    return *v->innermost_running_task;
}

bool task::is_owned_by_current_thread() const {
    return true;
}

void interface5::internal::task_base::destroy( task& victim ) {
    __TBB_ASSERT( victim.prefix().ref_count== (ConcurrentWaitsEnabled(victim) ? 1 : 0), "Task being destroyed must not have children" );
    __TBB_ASSERT( victim.state()==task::allocated, "illegal state for victim task" );
    task* parent = victim.parent();
    victim.~task();
    if( parent ) {
        __TBB_ASSERT( parent->state()==task::allocated, "attempt to destroy child of running or corrupted parent?" );
        parent->internal_decrement_ref_count();
        // Despite last reference to *parent removed, it should not be destroyed (documented behavior).
    }
    governor::local_scheduler()->free_task<no_hint>( victim );
}

void task::spawn_and_wait_for_all( task_list& list ) {
    generic_scheduler* s = governor::local_scheduler();
    task* t = list.first;
    if( t ) {
        if( &t->prefix().next!=list.next_ptr )
            s->local_spawn( *t->prefix().next, *list.next_ptr );
        list.clear();
    }
    s->local_wait_for_all( *this, t );
}

/** Defined out of line so that compiler does not replicate task's vtable. 
    It's pointless to define it inline anyway, because all call sites to it are virtual calls
    that the compiler is unlikely to optimize. */
void task::note_affinity( affinity_id ) {
}

} // namespace tbb

