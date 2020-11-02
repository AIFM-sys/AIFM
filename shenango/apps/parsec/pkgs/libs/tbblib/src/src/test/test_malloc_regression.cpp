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

#define HARNESS_NO_PARSE_COMMAND_LINE 1

#include <stdio.h>
#include "tbb/scalable_allocator.h"

class minimalAllocFree {
public:
    void operator()(int size) const {
        tbb::scalable_allocator<char> a;
        char* str = a.allocate( size );
        a.deallocate( str, size );
    }
};

#include "harness.h"

template<typename Body, typename Arg>
void RunThread(const Body& body, const Arg& arg) {
    NativeParallelForTask<Arg,Body> job(arg, body);
    job.start();
    job.wait_to_finish();
}

#include "harness_memory.h"

// The regression test for bug #1518 where thread boot strap allocations "leaked"
bool TestBootstrapLeak() {
    /* In the bug 1518, each thread leaked ~384 bytes.
       Initially, scalable allocator maps 1MB. Thus it is necessary to take out most of this space.
       1MB is chunked into 16K blocks; of those, one block is for thread boot strap, and one more 
       should be reserved for the test body. 62 blocks left, each can serve 15 objects of 1024 bytes.
    */
    const int alloc_size = 1024;
    const int take_out_count = 15*62;

    tbb::scalable_allocator<char> a;
    char* array[take_out_count];
    for( int i=0; i<take_out_count; ++i )
        array[i] = a.allocate( alloc_size );

    RunThread( minimalAllocFree(), alloc_size ); // for threading library to take some memory
    size_t memory_in_use = GetMemoryUsage();
    // Wait for memory usage data to "stabilize". The test number (1000) has nothing underneath.
    for( int i=0; i<1000; i++) {
        if( GetMemoryUsage()!=memory_in_use ) {
            memory_in_use = GetMemoryUsage();
            i = -1;
        }
    }

    ptrdiff_t memory_leak = 0;
    // Notice that 16K boot strap memory block is enough to serve 42 threads.
    const int num_thread_runs = 200;
    for (int run=0; run<3; run++) {
        memory_in_use = GetMemoryUsage();
        for( int i=0; i<num_thread_runs; ++i )
            RunThread( minimalAllocFree(), alloc_size );

        memory_leak = GetMemoryUsage() - memory_in_use;
        if (!memory_leak)
            break;
    }
    if( memory_leak>0 ) { // possibly too strong?
        REPORT( "Error: memory leak of up to %ld bytes\n", static_cast<long>(memory_leak));
    }

    for( int i=0; i<take_out_count; ++i )
        a.deallocate( array[i], alloc_size );

    return memory_leak<=0;
}

bool TestReallocMsize(size_t startSz) {
    bool passed = true;

    char *buf = (char*)scalable_malloc(startSz);
    ASSERT(buf, "");
    size_t realSz = scalable_msize(buf);
    ASSERT(realSz>=startSz, "scalable_msize must be not less then allocated size");
    memset(buf, 'a', realSz-1);
    buf[realSz-1] = 0;
    char *buf1 = (char*)scalable_realloc(buf, 2*realSz);
    ASSERT(buf1, "");
    ASSERT(scalable_msize(buf1)>=2*realSz, 
           "scalable_msize must be not less then allocated size");
    buf1[2*realSz-1] = 0;
    if ( strspn(buf1, "a") < realSz-1 ) {
        REPORT( "Error: data broken for %d Bytes object.\n", startSz);
        passed = false;
    }
    scalable_free(buf1);

    return passed;
}

int TestMain () {
    bool passed = true;
    // Check whether memory usage data can be obtained; if not, skip test_bootstrap_leak.
    if( GetMemoryUsage() )
        passed &= TestBootstrapLeak();

    for (size_t a=1, b=1, sum=1; sum<=64*1024; ) {
        passed &= TestReallocMsize(sum);
        a = b;
        b = sum;
        sum = a+b;
    }
    for (size_t a=2; a<=64*1024; a*=2)
        passed &= TestReallocMsize(a);
    
    ASSERT( passed, "Test failed" );
    return Harness::Done;
}
