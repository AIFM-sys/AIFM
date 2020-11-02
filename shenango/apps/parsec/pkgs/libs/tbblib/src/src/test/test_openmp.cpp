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

// Test mixing OpenMP and TBB

/* SCR #471 
 Bellow is workaround to compile test within enviroment of Intel Compiler
 but by Microsoft Compiler. So, there is wrong "omp.h" file included and
 manifest section is missed from .exe file - restoring here.

 As of Visual Studio 2010, crtassem.h is no longer shipped.
 */
#if !defined(__INTEL_COMPILER) && _MSC_VER >= 1400 && _MSC_VER < 1600
    #include <crtassem.h>
    #if !defined(_OPENMP)
        #define _OPENMP
        #if defined(_DEBUG)
            #pragma comment(lib, "vcompd")
        #else   // _DEBUG
            #pragma comment(lib, "vcomp")
        #endif  // _DEBUG
    #endif // _OPENMP

    #if defined(_DEBUG)
        #if defined(_M_IX86)
            #pragma comment(linker,"/manifestdependency:\"type='win32' "            \
                "name='" __LIBRARIES_ASSEMBLY_NAME_PREFIX ".DebugOpenMP' "         \
                "version='" _CRT_ASSEMBLY_VERSION "' "                          \
                "processorArchitecture='x86' "                                  \
                "publicKeyToken='" _VC_ASSEMBLY_PUBLICKEYTOKEN "'\"")
        #elif defined(_M_AMD64)
            #pragma comment(linker,"/manifestdependency:\"type='win32' "            \
                "name='" __LIBRARIES_ASSEMBLY_NAME_PREFIX ".DebugOpenMP' "         \
                "version='" _CRT_ASSEMBLY_VERSION "' "                          \
                "processorArchitecture='amd64' "                                  \
                "publicKeyToken='" _VC_ASSEMBLY_PUBLICKEYTOKEN "'\"")
        #elif defined(_M_IA64)
            #pragma comment(linker,"/manifestdependency:\"type='win32' "            \
                "name='" __LIBRARIES_ASSEMBLY_NAME_PREFIX ".DebugOpenMP' "         \
                "version='" _CRT_ASSEMBLY_VERSION "' "                          \
                "processorArchitecture='ia64' "                                  \
                "publicKeyToken='" _VC_ASSEMBLY_PUBLICKEYTOKEN "'\"")
        #endif
    #else   // _DEBUG
        #if defined(_M_IX86)
            #pragma comment(linker,"/manifestdependency:\"type='win32' "            \
                "name='" __LIBRARIES_ASSEMBLY_NAME_PREFIX ".OpenMP' "              \
                "version='" _CRT_ASSEMBLY_VERSION "' "                          \
                "processorArchitecture='x86' "                                  \
                "publicKeyToken='" _VC_ASSEMBLY_PUBLICKEYTOKEN "'\"")
        #elif defined(_M_AMD64)
            #pragma comment(linker,"/manifestdependency:\"type='win32' "            \
                "name='" __LIBRARIES_ASSEMBLY_NAME_PREFIX ".OpenMP' "              \
                "version='" _CRT_ASSEMBLY_VERSION "' "                          \
                "processorArchitecture='amd64' "                                  \
                "publicKeyToken='" _VC_ASSEMBLY_PUBLICKEYTOKEN "'\"")
        #elif defined(_M_IA64)
            #pragma comment(linker,"/manifestdependency:\"type='win32' "            \
                "name='" __LIBRARIES_ASSEMBLY_NAME_PREFIX ".OpenMP' "              \
                "version='" _CRT_ASSEMBLY_VERSION "' "                          \
                "processorArchitecture='ia64' "                                  \
                "publicKeyToken='" _VC_ASSEMBLY_PUBLICKEYTOKEN "'\"")
        #endif
    #endif  // _DEBUG
    #define _OPENMP_NOFORCE_MANIFEST
#endif

#include <omp.h>


typedef short T;

void SerialConvolve( T c[], const T a[], int m, const T b[], int n ) {
    for( int i=0; i<m+n-1; ++i ) {
        int start = i<n ? 0 : i-n+1;
        int finish = i<m ? i+1 : m; 
        T sum = 0;
        for( int j=start; j<finish; ++j ) 
            sum += a[j]*b[i-j];
        c[i] = sum;
    }
}

#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#include "tbb/parallel_reduce.h"
#include "tbb/task_scheduler_init.h"
#include "harness.h"

using namespace tbb;

#if _MSC_VER && !defined(__INTEL_COMPILER)
    // Suppress overzealous warning about short+=short
    #pragma warning( push )
    #pragma warning( disable: 4244 )
#endif

class InnerBody: NoAssign {
    const T* my_a;
    const T* my_b;
    const int i;
public:
    T sum;
    InnerBody( T /*c*/[], const T a[], const T b[], int i ) :
        my_a(a), my_b(b), sum(0), i(i)
    {}
    InnerBody( InnerBody& x, split ) :
        my_a(x.my_a), my_b(x.my_b), sum(0), i(x.i)
    { 
    }
    void join( InnerBody& x ) {sum += x.sum;}
    void operator()( const blocked_range<int>& range ) {
        for( int j=range.begin(); j!=range.end(); ++j ) 
            sum += my_a[j]*my_b[i-j];
    }
};

#if _MSC_VER && !defined(__INTEL_COMPILER)
    #pragma warning( pop )
#endif

//! Test OpenMMP loop around TBB loop
void OpenMP_TBB_Convolve( T c[], const T a[], int m, const T b[], int n ) {
    REMARK("testing OpenMP loop around TBB loop\n");
#pragma omp parallel 
    {
        task_scheduler_init init;
#pragma omp for
        for( int i=0; i<m+n-1; ++i ) {
            int start = i<n ? 0 : i-n+1;
            int finish = i<m ? i+1 : m; 
            InnerBody body(c,a,b,i);
            parallel_reduce( blocked_range<int>(start,finish,10), body );
            c[i] = body.sum;
        }
    }
}

class OuterBody: NoAssign {
    const T* my_a;
    const T* my_b;
    T* my_c;
    const int m;
    const int n;
public:
    T sum;
    OuterBody( T c[], const T a[], int m_, const T b[], int n_ ) :
        my_c(c), my_a(a), my_b(b), m(m_), n(n_)
    {}
    void operator()( const blocked_range<int>& range ) const {
        for( int i=range.begin(); i!=range.end(); ++i ) {
            int start = i<n ? 0 : i-n+1;
            int finish = i<m ? i+1 : m; 
            T sum = 0;
#pragma omp parallel for reduction(+:sum)
            for( int j=start; j<finish; ++j ) 
                sum += my_a[j]*my_b[i-j];
            my_c[i] = sum;
        }
    }
};

//! Test TBB loop around OpenMP loop
void TBB_OpenMP_Convolve( T c[], const T a[], int m, const T b[], int n ) {
    REMARK("testing TBB loop around OpenMP loop\n");
    parallel_for( blocked_range<int>(0,m+n-1,10), OuterBody( c, a, m, b, n ) );
}

#include <stdio.h>

const int M = 17*17;
const int N = 13*13;

int TestMain () {
#ifdef _PGO_INSTRUMENT
    REPORT("Known issue: test_openmp.exe has problems if compiled with -prof-genx; skipping\n");
    return Harness::Skipped;
#endif
    MinThread = 1;
    for( int p=MinThread; p<=MaxThread; ++p ) {
        T a[M];
        T b[N];
        for( int m=1; m<=M; m*=17 ) {
            for( int n=1; n<=M; n*=13 ) {
                for( int i=0; i<m; ++i ) a[i] = T(1+i/5);
                for( int i=0; i<n; ++i ) b[i] = T(1+i/7);
                T expected[M+N];
                SerialConvolve( expected, a, m, b, n );
                task_scheduler_init init(p);
                T actual[M+N];
                for( int k = 0; k<2; ++k ) {
                    memset( actual, -1, sizeof(actual) );
                    switch(k) {
                        case 0: 
                            TBB_OpenMP_Convolve( actual, a, m, b, n ); 
                            break;
                        case 1: 
                            OpenMP_TBB_Convolve( actual, a, m, b, n ); 
                            break;
                    }
                    for( int i=0; i<m+n-1; ++i ) {
                        ASSERT( actual[i]==expected[i], NULL );
                    }
                }
            }
        } 
    }
    return Harness::Done;
}
