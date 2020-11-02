/*-
 * Copyright (c) 2000-2004 Mark R V Murray
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/bsd_cdefs.h>
//__FBSDID("$FreeBSD$");

#include <sys/bsd_libkern.h>
#include <sys/bsd_param.h>
//#include <sys/bsd_kthread.h>
//#include <sys/bsd_lock.h>
#include <sys/bsd_malloc.h>
//#include <sys/bsd_mutex.h>
//#include <sys/bsd_poll.h>
//#include <sys/bsd_queue.h>
#include <sys/bsd_random.h>
//#include <sys/bsd_selinfo.h>
//#include <sys/bsd_systm.h>
//#include <sys/bsd_sysctl.h>

#include <machine/bsd_cpufunc.h>
#include <machine/bsd_cpu.h>
#include <stdlib.h>


//#include <dev/random/bsd_randomdev_soft.h>

static int read_random_phony(void *, int);

/* Structure holding the desired entropy sources */
struct harvest_select harvest = { 1, 1, 1, 0 };

/* hold the address of the routine which is actually called if
 * the randomdev is loaded
 */
static void (*reap_func)(u_int64_t, const void *, u_int, u_int, u_int,
    enum esource) = NULL;
static int (*read_func)(void *, int) = read_random_phony;

/* Initialise the harvester at load time */
void
random_yarrow_init_harvester(void (*reaper)(u_int64_t, const void *, u_int,
    u_int, u_int, enum esource), int (*reader)(void *, int))
{
	reap_func = reaper;
	read_func = reader;
}

/* Deinitialise the harvester at unload time */
void
random_yarrow_deinit_harvester(void)
{
	reap_func = NULL;
	read_func = read_random_phony;
}

/* Entropy harvesting routine. This is supposed to be fast; do
 * not do anything slow in here!
 * Implemented as in indirect call to allow non-inclusion of
 * the entropy device.
 *
 * XXXRW: get_cyclecount() is cheap on most modern hardware, where cycle
 * counters are built in, but on older hardware it will do a real time clock
 * read which can be quite expensive.
 */
void
random_harvest(void *entropy, u_int count, u_int bits, u_int frac,
    enum esource origin)
{
	if (reap_func)
		(*reap_func)(get_cyclecount(), entropy, count, bits, frac,
		    origin);
}

/* Userland-visible version of read_random */
int
read_random(void *buf, int count)
{
	return ((*read_func)(buf, count));
}

/* If the entropy device is not loaded, make a token effort to
 * provide _some_ kind of randomness. This should only be used
 * inside other RNG's, like arc4random(9).
 */
static int
read_random_phony(void *buf, int count)
{
	u_long randval;
	int size, i;

	/* srandom() is called in kern/init_main.c:proc0_post() */

	/* Fill buf[] with random(9) output */
	for (i = 0; i < count; i+= (int)sizeof(u_long)) {
		randval = random();
		size = MIN(count - i, sizeof(u_long));
		memcpy(&((char *)buf)[i], &randval, (size_t)size);
	}

	return (count);
}

/* Helper routine to enable kproc_exit() to work while the module is
 * being (or has been) unloaded.
 * This routine is in this file because it is always linked into the kernel,
 * and will thus never be unloaded. This is critical for unloadable modules
 * that have threads.
 */
void
random_set_wakeup_exit(void *control)
{
	//wakeup(control);
	//kproc_exit(0);
	/* NOTREACHED */
}
