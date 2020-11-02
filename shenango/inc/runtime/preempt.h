/*
 * preempt.h - support for kthread preemption
 */

#pragma once

#include <base/stddef.h>

extern volatile __thread unsigned int preempt_cnt;
extern void preempt(void);

#define PREEMPT_NOT_PENDING	(1 << 31)

/**
 * preempt_disable - disables preemption
 *
 * Can be nested.
 */
static inline void preempt_disable(void)
{
	asm volatile("addl $1, %%fs:preempt_cnt@tpoff" : : : "memory", "cc");
	barrier();
}

/**
 * preempt_enable_nocheck - reenables preemption without checking for conditions
 *
 * Can be nested.
 */
static inline void preempt_enable_nocheck(void)
{
	barrier();
	asm volatile("subl $1, %%fs:preempt_cnt@tpoff" : : : "memory", "cc");
}

/**
 * preempt_enable - reenables preemption
 *
 * Can be nested.
 */
static inline void preempt_enable(void)
{
	preempt_enable_nocheck();
	if (unlikely(preempt_cnt == 0))
		preempt();
}

/**
 * preempt_needed - returns true if a preemption event is stuck waiting
 */
static inline bool preempt_needed(void)
{
	return (preempt_cnt & PREEMPT_NOT_PENDING) == 0;
}

/**
 * preempt_enabled - returns true if preemption is enabled
 */
static inline bool preempt_enabled(void)
{
	return (preempt_cnt & ~PREEMPT_NOT_PENDING) == 0;
}

/**
 * assert_preempt_disabled - asserts that preemption is disabled
 */
static inline void assert_preempt_disabled(void)
{
	assert(!preempt_enabled());
}

/**
 * clear_preempt_needed - clear the flag that indicates a preemption request is
 * pending
 */
static inline void clear_preempt_needed(void)
{
	preempt_cnt |= PREEMPT_NOT_PENDING;
}
