/*
 * thread.h - support for user-level threads
 */

#pragma once

#include <base/types.h>
#include <base/compiler.h>
#include <runtime/preempt.h>
#include <iokernel/control.h>

#define GC_STATUS 3

extern __thread unsigned int __curr_cpu;
extern __thread unsigned int __status;

struct thread;
typedef void (*thread_fn_t)(void *arg);
typedef struct thread thread_t;

/*
 * Low-level routines, these are helpful for bindings and synchronization
 * primitives.
 */

extern void thread_park_and_switch(thread_t *thread);
extern void thread_park_and_unlock_np(spinlock_t *l);
extern void thread_ready(thread_t *thread);
extern void thread_ready_rr(thread_t *thread);
extern thread_t *thread_create(thread_fn_t fn, void *arg);
extern thread_t *thread_create_with_buf(thread_fn_t fn, void **buf, size_t len);

extern __thread thread_t *__self;
#ifndef DEBUG
extern __thread unsigned int __status;
#endif
extern __thread unsigned int kthread_idx;

static inline unsigned int get_current_affinity(void)
{
	return kthread_idx;
}

/**
 * thread_self - gets the currently running thread
 */
inline thread_t *thread_self(void)
{
	return __self;
}

static inline int get_core_num(void) { return ACCESS_ONCE(__curr_cpu); }

#ifndef DEBUG
static inline unsigned int get_self_th_status(void) { return __status; }
static inline void set_self_th_status(unsigned int status)
{
	__status = status;
}
#else
extern unsigned int get_self_th_status(void);
extern void set_self_th_status(unsigned int status);
#endif

/*
 * High-level routines, use this API most of the time.
 */

extern void thread_yield(void);
extern int thread_spawn(thread_fn_t fn, void *arg);
extern void thread_exit(void) __noreturn;
extern void set_th_status(thread_t *thread, uint32_t status);
