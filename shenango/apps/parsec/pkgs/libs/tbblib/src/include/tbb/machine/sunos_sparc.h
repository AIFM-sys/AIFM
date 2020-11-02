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


#ifndef __TBB_machine_H
#error Do not include this file directly; include tbb_machine.h instead
#endif

#include <stdint.h>
#include <unistd.h>

#include <sched.h> // sched_yield

#define __TBB_WORDSIZE 8
#define __TBB_BIG_ENDIAN 1

#define __TBB_release_consistency_helper() __asm__ __volatile__ ("": : :"memory")

inline void __TBB_rel_acq_fence() { __asm__ __volatile__("membar #LoadLoad|#LoadStore|#StoreStore|#StoreLoad": : : "memory"); }

//--------------------------------------------------
// Compare and swap
//--------------------------------------------------

/**
 * Atomic CAS for 32 bit values, if *ptr==comparand, then *ptr=value, returns *ptr
 * @param ptr pointer to value in memory to be swapped with value if *ptr==comparand
 * @param value value to assign *ptr to if *ptr==comparand
 * @param comparand value to compare with *ptr
 ( @return value originally in memory at ptr, regardless of success
*/
static inline int32_t __TBB_machine_cmpswp4(volatile void *ptr, int32_t value, int32_t comparand ){
  int32_t result;
  __asm__ __volatile__(
                       "cas\t[%5],%4,%1"
                       : "=m"(*(int32_t *)ptr), "=r"(result)
                       : "m"(*(int32_t *)ptr), "1"(value), "r"(comparand), "r"(ptr)
                       : "memory");
  return result;
}

/**
 * Atomic CAS for 64 bit values, if *ptr==comparand, then *ptr=value, returns *ptr
 * @param ptr pointer to value in memory to be swapped with value if *ptr==comparand
 * @param value value to assign *ptr to if *ptr==comparand
 * @param comparand value to compare with *ptr
 ( @return value originally in memory at ptr, regardless of success
 */
static inline int64_t __TBB_machine_cmpswp8(volatile void *ptr, int64_t value, int64_t comparand ){
  int64_t result;
  __asm__ __volatile__(
                       "casx\t[%5],%4,%1"
               : "=m"(*(int64_t *)ptr), "=r"(result)
               : "m"(*(int64_t *)ptr), "1"(value), "r"(comparand), "r"(ptr)
               : "memory");
  return result;
}

//---------------------------------------------------
// Fetch and add
//---------------------------------------------------

/**
 * Atomic fetch and add for 32 bit values, in this case implemented by continuously checking success of atomicity
 * @param ptr pointer to value to add addend to
 * @param addened value to add to *ptr
 * @return value at ptr before addened was added
 */
static inline int32_t __TBB_machine_fetchadd4(volatile void *ptr, int32_t addend){
  int32_t result;
  __asm__ __volatile__ (                                 
                        "0:\t add\t %3, %4, %0\n"    // do addition
                        "\t cas\t [%2], %3, %0\n"        // cas to store result in memory
                        "\t cmp\t %3, %0\n"            // check if value from memory is original
                        "\t bne,a,pn\t %%icc, 0b\n"        // if not try again
                        "\t mov %0, %3\n"            // use branch delay slot to move new value in memory to be added
               : "=&r"(result), "=m"(*(int32_t *)ptr)
               : "r"(ptr), "r"(*(int32_t *)ptr), "r"(addend), "m"(*(int32_t *)ptr)
               : "ccr", "memory");
  return result;
}

/**
 * Atomic fetch and add for 64 bit values, in this case implemented by continuously checking success of atomicity
 * @param ptr pointer to value to add addend to
 * @param addened value to add to *ptr
 * @return value at ptr before addened was added
 */
static inline int64_t __TBB_machine_fetchadd8(volatile void *ptr, int64_t addend){
  int64_t result;
  __asm__ __volatile__ (
                        "0:\t add\t %3, %4, %0\n"    // do addition
                        "\t casx\t [%2], %3, %0\n"        // cas to store result in memory
                        "\t cmp\t %3, %0\n"            // check if value from memory is original
                        "\t bne,a,pn\t %%xcc, 0b\n"        // if not try again
                        "\t mov %0, %3\n"            // use branch delay slot to move new value in memory to be added
                : "=&r"(result), "=m"(*(int64_t *)ptr)
                : "r"(ptr), "r"(*(int64_t *)ptr), "r"(addend), "m"(*(int64_t *)ptr)
                : "ccr", "memory");
  return result;
}

//--------------------------------------------------------
// Logarithm (base two, integer)
//--------------------------------------------------------

static inline int64_t __TBB_machine_lg( uint64_t x ) {
    uint64_t count;
    // one hot encode
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    x |= (x >> 32);
    // count 1's
    __asm__ ("popc %1, %0" : "=r"(count) : "r"(x) );
    return count-1;
}

//--------------------------------------------------------

static inline void __TBB_machine_or( volatile void *ptr, uint64_t addend ) {
  __asm__ __volatile__ (
                        "0:\t or\t %2, %3, %%g1\n" // do addition
                        "\t casx\t [%1], %2, %%g1\n"            // cas to store result in memory
                        "\t cmp\t %2, %%g1\n"                   // check if value from memory is original
                        "\t bne,a,pn\t %%xcc, 0b\n" // if not try again
                        "\t mov %%g1, %2\n"                     // use branch delay slot to move new value in memory to be added
                : "=m"(*(int64_t *)ptr)
                : "r"(ptr), "r"(*(int64_t *)ptr), "r"(addend), "m"(*(int64_t *)ptr)
                : "ccr", "g1", "memory");
}

static inline void __TBB_machine_and( volatile void *ptr, uint64_t addend ) {
  __asm__ __volatile__ (
                        "0:\t and\t %2, %3, %%g1\n"        // do addition
                        "\t casx\t [%1], %2, %%g1\n"            // cas to store result in memory
                        "\t cmp\t %2, %%g1\n"                   // check if value from memory is original
                        "\t bne,a,pn\t %%xcc, 0b\n"         // if not try again
                        "\t mov %%g1, %2\n"                     // use branch delay slot to move new value in memory to be added
                : "=m"(*(int64_t *)ptr)
                : "r"(ptr), "r"(*(int64_t *)ptr), "r"(addend), "m"(*(int64_t *)ptr)
                : "ccr", "g1", "memory");
}


static inline void __TBB_machine_pause( int32_t delay ) {
    // do nothing, inlined, doesnt matter
}

// put 0xff in memory location, return memory value,
//  generic trylockbyte puts 0x01, however this is fine
//  because all that matters is that 0 is unlocked
static inline bool __TBB_machine_trylockbyte(unsigned char &flag){
    unsigned char result;
    __asm__ __volatile__ (
            "ldstub\t [%2], %0\n"
        : "=r"(result), "=m"(flag)
        : "r"(&flag), "m"(flag)
        : "memory");
    return result == 0;
}


// Machine specific atomic operations

//#define __TBB_CompareAndSwap1(P,V,C) __TBB_machine_cmpswp1(P,V,C)  // use generic version in tbb_machine.h
//#define __TBB_CompareAndSwap2(P,V,C) __TBB_machine_cmpswp2(P,V,C)  // use generic version in tbb_machine.h
#define __TBB_CompareAndSwap4(P,V,C) __TBB_machine_cmpswp4(P,V,C)
#define __TBB_CompareAndSwap8(P,V,C) __TBB_machine_cmpswp8(P,V,C)
#define __TBB_CompareAndSwapW(P,V,C) __TBB_machine_cmpswp8(P,V,C)

//#define __TBB_FetchAndAdd1(P,V) __TBB_machine_fetchadd1(P,V)       // use generic version in tbb_machine.h
//#define __TBB_FetchAndAdd2(P,V) __TBB_machine_fetchadd2(P,V)       // use generic version in tbb_machine.h
#define __TBB_FetchAndAdd4(P,V) __TBB_machine_fetchadd4(P,V)
#define __TBB_FetchAndAdd8(P,V)  __TBB_machine_fetchadd8(P,V)
#define __TBB_FetchAndAddW(P,V)  __TBB_machine_fetchadd8(P,V)

// use generic version in tbb_machine.h
//#define __TBB_FetchAndStore1(P,V) __TBB_machine_fetchstore1(P,V)  
//#define __TBB_FetchAndStore2(P,V) __TBB_machine_fetchstore2(P,V)
//#define __TBB_FetchAndStore4(P,V) __TBB_machine_fetchstore4(P,V)
//#define __TBB_FetchAndStore8(P,V)  __TBB_machine_fetchstore8(P,V)
//#define __TBB_FetchAndStoreW(P,V)  __TBB_machine_fetchstore8(P,V)

#define __TBB_Store8(P,V) (*P = V)
#define __TBB_Load8(P)    (*P)

#define __TBB_AtomicOR(P,V) __TBB_machine_or(P,V)
#define __TBB_AtomicAND(P,V) __TBB_machine_and(P,V)

// Definition of other functions
#define __TBB_Pause(V) __TBB_machine_pause(V)
#define __TBB_Log2(V)    __TBB_machine_lg(V)

// Special atomic functions
#define __TBB_FetchAndAddWrelease(P,V) __TBB_FetchAndAddW(P,V)
#define __TBB_FetchAndIncrementWacquire(P) __TBB_FetchAndAddW(P,1)
#define __TBB_FetchAndDecrementWrelease(P) __TBB_FetchAndAddW(P,-1)

// Definition of Lock functions
// Repeatedly runs TryLockByte, no need to implement
#undef __TBB_LockByte

#define __TBB_TryLockByte(P) __TBB_machine_trylockbyte(P)

#define __TBB_Yield() sched_yield()
