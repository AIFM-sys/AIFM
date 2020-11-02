/*-
 * Copyright (c) 2007 Stephan Uphoff <ups@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _BSD_SYS__RMLOCK_H_
#define	_BSD_SYS__RMLOCK_H_

/* 
 * XXXUPS remove as soon as we have per cpu variable
 * linker sets and  can define rm_queue in _rm_lock.h
*/
//#include <sys/bsd_pcpu.h>
/*
 * Mostly reader/occasional writer lock.
 */

LIST_HEAD(rmpriolist,rm_priotracker);

struct rmlock {
	struct lock_object lock_object; 
	volatile int 	rm_noreadtoken;
	LIST_HEAD(,rm_priotracker) rm_activeReaders;
	struct mtx	rm_lock;

};


struct rm_priotracker {
//	struct rm_queue rmp_cpuQueue; /* Must be first */
	struct rmlock *rmp_rmlock;
	struct thread *rmp_thread;
	int rmp_flags;
	LIST_ENTRY(rm_priotracker) rmp_qentry;
};

#endif /* !_BSD_SYS__RMLOCK_H_ */
