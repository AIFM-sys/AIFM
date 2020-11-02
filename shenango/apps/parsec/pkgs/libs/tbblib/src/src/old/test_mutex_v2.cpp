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

//------------------------------------------------------------------------
// Test TBB mutexes when used with parallel_for.h
//
// Usage: test_Mutex.exe [-v] nthread
//
// The -v option causes timing information to be printed.
//
// Compile with _OPENMP and -openmp
//------------------------------------------------------------------------
#include "tbb/atomic.h"
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#include "tbb/tick_count.h"
#include "../test/harness.h"
#include "spin_rw_mutex_v2.h"
#include <cstdlib>
#include <cstdio>

#if __linux__
#define STD std
#else
#define STD   /* Cater to broken Windows compilers that are missing "std". */
#endif /* __linux__ */

// This test deliberately avoids a "using tbb" statement,
// so that the error of putting types in the wrong namespace will be caught.

template<typename M>
struct Counter {
    typedef M mutex_type;
    M mutex;
    volatile long value;
};

//! Function object for use with parallel_for.h.
template<typename C>
struct AddOne {
    C& counter;
    /** Increments counter once for each iteration in the iteration space. */
    void operator()( tbb::blocked_range<size_t>& range ) const {
        for( size_t i=range.begin(); i!=range.end(); ++i ) {
            if( i&1 ) {
                // Try implicit acquire and explicit release
                typename C::mutex_type::scoped_lock lock(counter.mutex);
                counter.value = counter.value+1;
                lock.release();
            } else {
                // Try explicit acquire and implicit release
                typename C::mutex_type::scoped_lock lock;
                lock.acquire(counter.mutex);
                counter.value = counter.value+1;
            }
        }
    }
    AddOne( C& counter_ ) : counter(counter_) {}
};

//! Generic test of a TBB mutex type M.
/** Does not test features specific to reader-writer locks. */
template<typename M>
void Test( const char * name ) {
    if( Verbose ) {
        printf("%s time = ",name);
        fflush(stdout);
    }
    Counter<M> counter;
    counter.value = 0;
    const int n = 100000;
    tbb::tick_count t0 = tbb::tick_count::now();
    tbb::parallel_for(tbb::blocked_range<size_t>(0,n,10000),AddOne<Counter<M> >(counter));
    tbb::tick_count t1 = tbb::tick_count::now();
    if( Verbose )
        printf("%g usec\n",(t1-t0).seconds());
    if( counter.value!=n )
        STD::printf("ERROR for %s: counter.value=%ld\n",name,counter.value);
}

template<typename M, size_t N>
struct Invariant {
    typedef M mutex_type;
    M mutex;
    const char* mutex_name;
    volatile long value[N];
    volatile long single_value;
    Invariant( const char* mutex_name_ ) :
        mutex_name(mutex_name_)
    {
    single_value = 0;
        for( size_t k=0; k<N; ++k )
            value[k] = 0;
    }
    void update() {
        for( size_t k=0; k<N; ++k )
            ++value[k];
    }
    bool value_is( long expected_value ) const {
        long tmp;
        for( size_t k=0; k<N; ++k )
//            if( value[k]!=expected_value )
//                return false;
            if( (tmp=value[k])!=expected_value ) {
                printf("ATTN! %ld!=%ld\n", tmp, expected_value);
                return false;
            }
        return true;
    }
    bool is_okay() {
        return value_is( value[0] );
    }
};

//! Function object for use with parallel_for.h.
template<typename I>
struct TwiddleInvariant {
    I& invariant;
    /** Increments counter once for each iteration in the iteration space. */
    void operator()( tbb::blocked_range<size_t>& range ) const {
        for( size_t i=range.begin(); i!=range.end(); ++i ) {
            //! Every 8th access is a write access
            bool write = (i%8)==7;
            bool okay = true;
            bool lock_kept = true;
            if( (i/8)&1 ) {
                // Try implicit acquire and explicit release
                typename I::mutex_type::scoped_lock lock(invariant.mutex,write);
                if( write ) {
                    long my_value = invariant.value[0];
                    invariant.update();
                    if( i%16==7 ) {
                        lock_kept = lock.downgrade_to_reader();
                        if( !lock_kept )
                            my_value = invariant.value[0] - 1;
                        okay = invariant.value_is(my_value+1);
                    }
                } else {
                    okay = invariant.is_okay();
                    if( i%8==3 ) {
                        long my_value = invariant.value[0];
                        lock_kept = lock.upgrade_to_writer();
                        if( !lock_kept )
                            my_value = invariant.value[0];
                        invariant.update();
                        okay = invariant.value_is(my_value+1);
                    }
                }
                lock.release();
            } else {
                // Try explicit acquire and implicit release
                typename I::mutex_type::scoped_lock lock;
                lock.acquire(invariant.mutex,write);
                if( write ) {
                    long my_value = invariant.value[0];
                    invariant.update();
                    if( i%16==7 ) {
                        lock_kept = lock.downgrade_to_reader();
                        if( !lock_kept )
                            my_value = invariant.value[0] - 1;
                        okay = invariant.value_is(my_value+1);
                    }
                } else {
                    okay = invariant.is_okay();
                    if( i%8==3 ) {
                        long my_value = invariant.value[0];
                        lock_kept = lock.upgrade_to_writer();
                        if( !lock_kept )
                            my_value = invariant.value[0];
                        invariant.update();
                        okay = invariant.value_is(my_value+1);
                    }
                }
            }
            if( !okay ) {
                STD::printf( "ERROR for %s at %ld: %s %s %s %s\n",invariant.mutex_name, long(i),
                             write?"write,":"read,", write?(i%16==7?"downgrade,":""):(i%8==3?"upgrade,":""),
                             lock_kept?"lock kept,":"lock not kept,", (i/8)&1?"imp/exp":"exp/imp" );
            }
        }
    }
    TwiddleInvariant( I& invariant_ ) : invariant(invariant_) {}
};

/** This test is generic so that we can test any other kinds of ReaderWriter locks we write later. */
template<typename M>
void TestReaderWriterLock( const char * mutex_name ) {
    if( Verbose ) {
        printf("%s readers & writers time = ",mutex_name);
        fflush(stdout);
    }
    Invariant<M,8> invariant(mutex_name);
    const size_t n = 500000;
    tbb::tick_count t0 = tbb::tick_count::now();
    tbb::parallel_for(tbb::blocked_range<size_t>(0,n,5000),TwiddleInvariant<Invariant<M,8> >(invariant));
    tbb::tick_count t1 = tbb::tick_count::now();
    // There is either a writer or a reader upgraded to a writer for each 4th iteration
    long expected_value = n/4;
    if( !invariant.value_is(expected_value) )
        STD::printf("ERROR for %s: final invariant value is wrong\n",mutex_name);
    if( Verbose )
        printf("%g usec\n",(t1-t0).seconds());
}

/** Test try_acquire functionality of a non-reenterable mutex */
template<typename M>
void TestTryAcquire_OneThread( const char * mutex_name ) {
    M tested_mutex;
    typename M::scoped_lock lock1;
    if( lock1.try_acquire(tested_mutex) )
        lock1.release();
    else
        STD::printf("ERROR for %s: try_acquire failed though it should not\n", mutex_name);
    {
        typename M::scoped_lock lock2(tested_mutex);
        if( lock1.try_acquire(tested_mutex) )
            STD::printf("ERROR for %s: try_acquire succeeded though it should not\n", mutex_name);
    }
    if( lock1.try_acquire(tested_mutex) )
        lock1.release();
    else
        STD::printf("ERROR for %s: try_acquire failed though it should not\n", mutex_name);
}

#include "tbb/task_scheduler_init.h"

int TestMain () {
    for( int p=MinThread; p<=MaxThread; ++p ) {
        tbb::task_scheduler_init init( p );
        if( Verbose )
            printf( "testing with %d workers\n", static_cast<int>(p) );
        // Run each test 3 times.
        for( int i=0; i<3; ++i ) {
            Test<tbb::spin_rw_mutex>( "Spin RW Mutex" );
            
            TestTryAcquire_OneThread<tbb::spin_rw_mutex>("Spin RW Mutex"); // only tests try_acquire for writers
            TestReaderWriterLock<tbb::spin_rw_mutex>( "Spin RW Mutex" );
        }
        if( Verbose )
            printf( "calling destructor for task_scheduler_init\n" );
    }
    return Harness::Done;
}
