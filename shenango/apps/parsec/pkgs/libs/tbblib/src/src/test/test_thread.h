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

#include "tbb/tbb_thread.h"
#include "tbb/atomic.h"

#define HARNESS_NO_PARSE_COMMAND_LINE 1
#include "harness_report.h"
#include "harness_assert.h"

static const int THRDS = 3;
static const int THRDS_DETACH = 2;
static tbb::atomic<int> sum;
static tbb::atomic<int> BaseCount;
static THREAD::id real_ids[THRDS+THRDS_DETACH];

class Base {
    mutable int copy_throws;
    friend void RunTests();
    friend void CheckExceptionSafety();
    void operator=( const Base& );   // Deny access
protected:
    Base() : copy_throws(100) {++BaseCount;}
    Base( const Base& c ) : copy_throws(c.copy_throws) {
        if( --copy_throws<=0 ) 
            __TBB_THROW(0);
        ++BaseCount; 
    }
    ~Base() {--BaseCount;}
};

template<int N>
class Data: Base {
    Data();                          // Deny access
    explicit Data(int v) : value(v) {}

    friend void RunTests();
    friend void CheckExceptionSafety();
public:
    int value;
};


#include "harness_barrier.h"

class ThreadFunc: Base {
    ThreadFunc() {}

    static Harness::SpinBarrier init_barrier;

    friend void RunTests();
public:
    void operator()(){
        real_ids[0] = THIS_THREAD::get_id();
        init_barrier.wait();
        
        sum.fetch_and_add(1);
    }
    void operator()(int num){
        real_ids[num] = THIS_THREAD::get_id();
        init_barrier.wait();

        sum.fetch_and_add(num);
    }
    void operator()(int num, Data<0> dx) { 
        real_ids[num] = THIS_THREAD::get_id();

        const double WAIT = .1;
        const double SHORT_TOLERANCE = 1e-8;
#if _WIN32 || _WIN64
        const double LONG_TOLERANCE = 0.120;  // maximal scheduling quantum for Windows Server
#else
        const double LONG_TOLERANCE = 0.200;  // reasonable upper bound
#endif
        tbb::tick_count t0 = tbb::tick_count::now();
        tbb::this_tbb_thread::sleep( tbb::tick_count::interval_t(WAIT) );
        tbb::tick_count t1 = tbb::tick_count::now();
        double delta = (t1-t0).seconds() - WAIT;
        if(delta+SHORT_TOLERANCE <= 0.0)
            REPORT("ERROR: Sleep interval too short (%g outside short tolerance(%g))\n", (t1-t0).seconds(), WAIT - SHORT_TOLERANCE);
        if(delta > LONG_TOLERANCE)
            REPORT("WARNING: Sleep interval too long (%g outside long tolerance(%g))\n", (t1-t0).seconds(), WAIT + LONG_TOLERANCE);

        init_barrier.wait();

        sum.fetch_and_add(num);
        sum.fetch_and_add(dx.value);
    }
    void operator()(Data<0> d) {
        tbb::this_tbb_thread::sleep( tbb::tick_count::interval_t(d.value*1.) );
    }
};

Harness::SpinBarrier ThreadFunc::init_barrier(THRDS);

void CheckRelations( const THREAD::id ids[], int n, bool duplicates_allowed ) {
    for( int i=0; i<n; ++i ) {
        const THREAD::id x = ids[i];
        for( int j=0; j<n; ++j ) {
            const THREAD::id y = ids[j];
            ASSERT( (x==y)==!(x!=y), NULL );
            ASSERT( (x<y)==!(x>=y), NULL );
            ASSERT( (x>y)==!(x<=y), NULL );
            ASSERT( (x<y)+(x==y)+(x>y)==1, NULL );
            ASSERT( x!=y || i==j || duplicates_allowed, NULL );
            for( int k=0; k<n; ++k ) {
                const THREAD::id z = ids[j];
                ASSERT( !(x<y && y<z) || x<z, "< is not transitive" );
            }    
        }
    }
}

class AnotherThreadFunc: Base {
public:
    void operator()() {}
    void operator()(const Data<1>&) {}
    void operator()(const Data<1>&, const Data<2>&) {}
    friend void CheckExceptionSafety();
};

#if TBB_USE_EXCEPTIONS
void CheckExceptionSafety() { 
    int original_count = BaseCount;
    // d loops over number of copies before throw occurs 
    for( int d=1; d<=3; ++d ) {
        // Try all combinations of throw/nothrow for f, x, and y's copy constructor.
        for( int i=0; i<8; ++i ) {
            { 
                const AnotherThreadFunc f = AnotherThreadFunc();
                if( i&1 ) f.copy_throws = d;
                const Data<1> x(0);
                if( i&2 ) x.copy_throws = d;
                const Data<2> y(0);
                if( i&4 ) y.copy_throws = d;
                bool exception_caught = false;
                for( int j=0; j<3; ++j ) {
                    try { 
                        switch(j) {
                            case 0: {THREAD t(f); t.join();} break;
                            case 1: {THREAD t(f,x); t.join();} break;
                            case 2: {THREAD t(f,x,y); t.join();} break;
                        }
                    } catch(...) {
                        exception_caught = true;
                    } 
                    ASSERT( !exception_caught||(i&((1<<(j+1))-1))!=0, NULL );
                }
            }
// Intel Compiler sometimes fails to destroy all implicitly generated copies 
// of an object when a copy constructor throws an exception.
// Problem was reported as Quad issue 482935.
// This #if should be removed or tightened when the bug is fixed.
#if !((_WIN32 || _WIN64) && defined(__INTEL_COMPILER))
            ASSERT( BaseCount==original_count, "object leak detected" );
#endif
        }
    }
}
#endif /* TBB_USE_EXCEPTIONS */

#include <cstdio>

void RunTests() {

    ThreadFunc t;
    Data<0> d100(100), d1(1), d0(0);
    THREAD::id id;
    THREAD::id id0, uniq_ids[THRDS];
    
    THREAD thrs[THRDS];
    THREAD thr;
    THREAD thr0(t);
    THREAD thr1(t, 2);
    THREAD thr2(t, 1, d100);
    
    ASSERT( thr0.get_id() != id, NULL );
    id0 = thr0.get_id();
    tbb::move(thrs[0], thr0);
    ASSERT( thr0.get_id() == id, NULL );
    ASSERT( thrs[0].get_id() == id0, NULL );

    THREAD::native_handle_type h1 = thr1.native_handle();
    THREAD::native_handle_type h2 = thr2.native_handle();
    THREAD::id id1 = thr1.get_id();
    THREAD::id id2 = thr2.get_id();
    tbb::swap(thr1, thr2);
    ASSERT( thr1.native_handle() == h2, NULL );
    ASSERT( thr2.native_handle() == h1, NULL );
    ASSERT( thr1.get_id() == id2, NULL );
    ASSERT( thr2.get_id() == id1, NULL );
    thr1.swap(thr2);
    ASSERT( thr1.native_handle() == h1, NULL );
    ASSERT( thr2.native_handle() == h2, NULL );
    ASSERT( thr1.get_id() == id1, NULL );
    ASSERT( thr2.get_id() == id2, NULL );
    thr1.swap(thr2);

    tbb::move(thrs[1], thr1);
    ASSERT( thr1.get_id() == id, NULL );

    tbb::move(thrs[2], thr2);
    ASSERT( thr2.get_id() == id, NULL );

    for (int i=0; i<THRDS; i++)
        uniq_ids[i] = thrs[i].get_id();

    ASSERT( thrs[2].joinable(), NULL );

    for (int i=0; i<THRDS; i++)
        thrs[i].join();
    for (int i=0; i<THRDS; i++)
        ASSERT(  real_ids[i] == uniq_ids[i], NULL );

    int current_sum = sum;
    ASSERT( current_sum == 104, NULL );
    ASSERT( ! thrs[2].joinable(), NULL );
    ASSERT( BaseCount==4, "object leak detected" );

#if TBB_USE_EXCEPTIONS
    CheckExceptionSafety(); 
#endif

    // Note: all tests involving BaseCount should be put before the tests
    // involing detached threads, because there is no way of knowing when 
    // a detached thread destroys its arguments.

    THREAD thr_detach_0(t, d0);
    real_ids[THRDS] = thr_detach_0.get_id();
    thr_detach_0.detach();
    ASSERT( thr_detach_0.get_id() == id, NULL );

    THREAD thr_detach_1(t, d1);
    real_ids[THRDS+1] = thr_detach_1.get_id();
    thr_detach_1.detach();
    ASSERT( thr_detach_1.get_id() == id, NULL );

    CheckRelations(real_ids, THRDS+THRDS_DETACH, true);

    CheckRelations(uniq_ids, THRDS, false);

    for (int i=0; i<2; i++) {
        AnotherThreadFunc empty_func;
        THREAD thr_to(empty_func), thr_from(empty_func);
        THREAD::id from_id = thr_from.get_id();
        if (i) thr_to.join(); 
        thr_to = thr_from;
        ASSERT( thr_from.get_id() == THREAD::id(), NULL );
        ASSERT( thr_to.get_id() == from_id, NULL );
    }

    ASSERT( THREAD::hardware_concurrency() > 0, NULL);
}

typedef bool (*id_relation)( THREAD::id, THREAD::id );

id_relation CheckSignatures() {
    id_relation r[6] = {&tbb::operator==,
                        &tbb::operator!=,
                        &tbb::operator<,
                        &tbb::operator>,
                        &tbb::operator<=,
                        &tbb::operator>=};
    return r[1];
}
