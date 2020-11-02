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

#include "tbb/tick_count.h"
#include "harness.h"
#include <cstdio>

//! Assert that two times in seconds are very close.
void AssertNear( double x, double y ) {
    ASSERT( -1.0E-10 <= x-y && x-y <=1.0E-10, NULL );
}

//! Test arithmetic operators on tick_count::interval_t
void TestArithmetic( const tbb::tick_count& t0, const tbb::tick_count& t1, const tbb::tick_count& t2 ) {
    tbb::tick_count::interval_t i= t1-t0;
    tbb::tick_count::interval_t j = t2-t1;
    tbb::tick_count::interval_t k = t2-t0;
    AssertSameType( tbb::tick_count::interval_t(), i-j );
    AssertSameType( tbb::tick_count::interval_t(), i+j );
    ASSERT( i.seconds()>1E-9, NULL );
    ASSERT( j.seconds()>1E-9, NULL );
    ASSERT( k.seconds()>2E-9, NULL );
    AssertNear( (i+j).seconds(), k.seconds() );
    AssertNear( (k-j).seconds(), i.seconds() );
    AssertNear( ((k-j)+(j-i)).seconds(), k.seconds()-i.seconds() );
    tbb::tick_count::interval_t sum;
    sum += i;
    sum += j;
    AssertNear( sum.seconds(), k.seconds() );
    sum -= i;
    AssertNear( sum.seconds(), j.seconds() );
    sum -= j;
    AssertNear( sum.seconds(), 0.0 );
}

//------------------------------------------------------------------------
// Test for overhead in calls to tick_count 
//------------------------------------------------------------------------

//! Wait for given duration.
/** The duration parameter is in units of seconds. */
static void WaitForDuration( double duration ) {
    tbb::tick_count start = tbb::tick_count::now();
    while( (tbb::tick_count::now()-start).seconds() < duration )
        continue;
}

//! Test that average timer overhead is within acceptable limit.
/** The 'tolerance' value inside the test specifies the limit. */
void TestSimpleDelay( int ntrial, double duration, double tolerance ) {
    double total_worktime = 0;
    // Iteration -1 warms up the code cache.
    for( int trial=-1; trial<ntrial; ++trial ) {
        tbb::tick_count t0 = tbb::tick_count::now();
        if( duration ) WaitForDuration(duration);
        tbb::tick_count t1 = tbb::tick_count::now();
        if( trial>=0 ) {
            total_worktime += (t1-t0).seconds(); 
        }
    }
    // Compute average worktime and average delta
    double worktime = total_worktime/ntrial;
    double delta = worktime-duration;
    REMARK("worktime=%g delta=%g tolerance=%g\n", worktime, delta, tolerance);

    // Check that delta is acceptable
    if( delta<0 ) 
        REPORT("ERROR: delta=%g < 0\n",delta); 
    if( delta>tolerance )
        REPORT("%s: delta=%g > %g=tolerance where duration=%g\n",delta>3*tolerance?"ERROR":"Warning",delta,tolerance,duration);
}

//------------------------------------------------------------------------
// Test for subtracting calls to tick_count from different threads.
//------------------------------------------------------------------------

#include "tbb/atomic.h"
const int MAX_NTHREAD = 1000;
static tbb::atomic<int> Counter;
static volatile bool Flag;
static tbb::tick_count tick_count_array[MAX_NTHREAD];

struct TickCountDifferenceBody {
    void operator()( int id ) const {
        if( --Counter==0 ) Flag = true;
        while( !Flag ) continue;
        tick_count_array[id] = tbb::tick_count::now();
    }
};

//! Test that two tick_count values recorded on different threads can be meaningfully subtracted.
void TestTickCountDifference( int n ) {
    double tolerance = 3E-4;
    for( int trial=0; trial<10; ++trial ) {
        Counter = n;
        Flag = false;
        NativeParallelFor( n, TickCountDifferenceBody() ); 
        ASSERT( Counter==0, NULL ); 
        for( int i=0; i<n; ++i )
            for( int j=0; j<i; ++j ) {
                double diff = (tick_count_array[i]-tick_count_array[j]).seconds();
                if( diff<0 ) diff = -diff;
                if( diff>tolerance ) {
                    REPORT("%s: cross-thread tick_count difference = %g > %g = tolerance\n",
                           diff>3*tolerance?"ERROR":"Warning",diff,tolerance);
                }
            }
    }
}

int TestMain () {
    tbb::tick_count t0 = tbb::tick_count::now();
    TestSimpleDelay(/*ntrial=*/1000000,/*duration=*/0,    /*tolerance=*/2E-6);
    tbb::tick_count t1 = tbb::tick_count::now();
    TestSimpleDelay(/*ntrial=*/10,     /*duration=*/0.125,/*tolerance=*/5E-6);
    tbb::tick_count t2 = tbb::tick_count::now();
    TestArithmetic(t0,t1,t2);

    for( int n=MinThread; n<=MaxThread; ++n ) {
        TestTickCountDifference(n);
    }
    return Harness::Done;
}
