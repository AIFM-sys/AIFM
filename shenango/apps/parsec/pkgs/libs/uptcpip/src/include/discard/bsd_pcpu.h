/*-
 * Copyright (c) Peter Wemm
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

#ifndef _BSD_MACHINE_PCPU_H_
#define	_BSD_MACHINE_PCPU_H_

#ifndef _BSD_SYS_CDEFS_H_
#error "sys/cdefs.h is a prerequisite for this file"
#endif

#include <machine/bsd_segments.h>
#include <machine/bsd_tss.h>

/*
 * The SMP parts are setup in pmap.c and locore.s for the BSP, and
 * mp_machdep.c sets up the data for the AP's to "see" when they awake.
 * The reason for doing it via a struct is so that an array of pointers
 * to each CPU's data can be set up for things like "check curproc on all
 * other processors"
 */

#ifdef XEN
#ifndef NR_VIRQS
#define	NR_VIRQS	24
#endif
#ifndef NR_IPIS
#define	NR_IPIS		2
#endif

/* These are peridically updated in shared_info, and then copied here. */
struct shadow_time_info {
	uint64_t tsc_timestamp;     /* TSC at last update of time vals.  */
	uint64_t system_timestamp;  /* Time, in nanosecs, since boot.    */
	uint32_t tsc_to_nsec_mul;
	uint32_t tsc_to_usec_mul;
	int tsc_shift;
	uint32_t version;
};


#define	PCPU_MD_FIELDS							\
	char	pc_monitorbuf[128] __aligned(128); /* cache line */	\
	struct	pcpu *pc_prvspace;	/* Self-reference */		\
	struct	pmap *pc_curpmap;					\
	struct	i386tss pc_common_tss;					\
	struct	segment_descriptor pc_common_tssd;			\
	struct	segment_descriptor *pc_tss_gdt;				\
	struct	segment_descriptor *pc_fsgs_gdt;			\
	vm_paddr_t 	*pc_pdir_shadow;				\
	int	pc_currentldt;						\
	u_int   pc_acpi_id;		/* ACPI CPU id */		\
	u_int	pc_apic_id;						\
	int	pc_private_tss;		/* Flag indicating private tss*/\
        u_int     pc_cr3;		/* track cr3 for R1/R3*/	\
        u_int     pc_pdir;                                              \
        u_int     pc_lazypmap;                                          \
        u_int     pc_rendezvous;                                        \
        u_int     pc_cpuast;						\
	uint64_t  pc_processed_system_time;				\
	struct shadow_time_info pc_shadow_time;				\
	int	pc_resched_irq;						\
	int	pc_callfunc_irq;					\
        int	pc_virq_to_irq[NR_VIRQS];				\
	int	pc_ipi_to_irq[NR_IPIS]
	

	
#else
#define	PCPU_MD_FIELDS							\
	char	pc_monitorbuf[128] __aligned(128); /* cache line */	\
	struct	pcpu *pc_prvspace;	/* Self-reference */		\
	struct	pmap *pc_curpmap;					\
	struct	i386tss pc_common_tss;					\
	struct	segment_descriptor pc_common_tssd;			\
	struct	segment_descriptor *pc_tss_gdt;				\
	struct	segment_descriptor *pc_fsgs_gdt;			\
	int	pc_currentldt;						\
	u_int   pc_acpi_id;		/* ACPI CPU id */		\
	u_int	pc_apic_id;						\
	int	pc_private_tss		/* Flag indicating private tss */

#endif

#ifdef _FREEBSD_KERNEL

#ifdef lint

extern struct pcpu *pcpup;

#define	PCPU_GET(member)	(pcpup->pc_ ## member)
#define	PCPU_ADD(member, val)	(pcpup->pc_ ## member += (val))
#define	PCPU_INC(member)	PCPU_ADD(member, 1)
#define	PCPU_PTR(member)	(&pcpup->pc_ ## member)
#define	PCPU_SET(member, val)	(pcpup->pc_ ## member = (val))

#elif defined(__GNUCLIKE_ASM) && defined(__GNUCLIKE___TYPEOF)

/*
 * Evaluates to the byte offset of the per-cpu variable name.
 */
#define	__pcpu_offset(name)						\
	__offsetof(struct pcpu, name)

/*
 * Evaluates to the type of the per-cpu variable name.
 */
#define	__pcpu_type(name)						\
	__typeof(((struct pcpu *)0)->name)

/*
 * Evaluates to the address of the per-cpu variable name.
 */
#define	__PCPU_PTR(name) __extension__ ({				\
	__pcpu_type(name) *__p;						\
									\
	__asm __volatile("movl %%fs:%1,%0; addl %2,%0"			\
	    : "=r" (__p)						\
	    : "m" (*(struct pcpu *)(__pcpu_offset(pc_prvspace))),	\
	      "i" (__pcpu_offset(name)));				\
									\
	__p;								\
})

/*
 * Evaluates to the value of the per-cpu variable name.
 */
#define	__PCPU_GET(name) __extension__ ({				\
	__pcpu_type(name) __res;					\
	struct __s {							\
		u_char	__b[MIN(sizeof(__pcpu_type(name)), 4)];		\
	} __s;								\
									\
	if (sizeof(__res) == 1 || sizeof(__res) == 2 ||			\
	    sizeof(__res) == 4) {					\
		__asm __volatile("mov %%fs:%1,%0"			\
		    : "=r" (__s)					\
		    : "m" (*(struct __s *)(__pcpu_offset(name))));	\
		*(struct __s *)(void *)&__res = __s;			\
	} else {							\
		__res = *__PCPU_PTR(name);				\
	}								\
	__res;								\
})

/*
 * Adds a value of the per-cpu counter name.  The implementation
 * must be atomic with respect to interrupts.
 */
#define	__PCPU_ADD(name, val) do {					\
	__pcpu_type(name) __val;					\
	struct __s {							\
		u_char	__b[MIN(sizeof(__pcpu_type(name)), 4)];		\
	} __s;								\
									\
	__val = (val);							\
	if (sizeof(__val) == 1 || sizeof(__val) == 2 ||			\
	    sizeof(__val) == 4) {					\
		__s = *(struct __s *)(void *)&__val;			\
		__asm __volatile("add %1,%%fs:%0"			\
		    : "=m" (*(struct __s *)(__pcpu_offset(name)))	\
		    : "r" (__s));					\
	} else								\
		*__PCPU_PTR(name) += __val;				\
} while (0)

/*
 * Increments the value of the per-cpu counter name.  The implementation
 * must be atomic with respect to interrupts.
 */
#define	__PCPU_INC(name) do {						\
	CTASSERT(sizeof(__pcpu_type(name)) == 1 ||			\
	    sizeof(__pcpu_type(name)) == 2 ||				\
	    sizeof(__pcpu_type(name)) == 4);				\
	if (sizeof(__pcpu_type(name)) == 1) {				\
		__asm __volatile("incb %%fs:%0"				\
		    : "=m" (*(__pcpu_type(name) *)(__pcpu_offset(name)))\
		    : "m" (*(__pcpu_type(name) *)(__pcpu_offset(name))));\
	} else if (sizeof(__pcpu_type(name)) == 2) {			\
		__asm __volatile("incw %%fs:%0"				\
		    : "=m" (*(__pcpu_type(name) *)(__pcpu_offset(name)))\
		    : "m" (*(__pcpu_type(name) *)(__pcpu_offset(name))));\
	} else if (sizeof(__pcpu_type(name)) == 4) {			\
		__asm __volatile("incl %%fs:%0"				\
		    : "=m" (*(__pcpu_type(name) *)(__pcpu_offset(name)))\
		    : "m" (*(__pcpu_type(name) *)(__pcpu_offset(name))));\
	}								\
} while (0)

/*
 * Sets the value of the per-cpu variable name to value val.
 */
#define	__PCPU_SET(name, val) {						\
	__pcpu_type(name) __val;					\
	struct __s {							\
		u_char	__b[MIN(sizeof(__pcpu_type(name)), 4)];		\
	} __s;								\
									\
	__val = (val);							\
	if (sizeof(__val) == 1 || sizeof(__val) == 2 ||			\
	    sizeof(__val) == 4) {					\
		__s = *(struct __s *)(void *)&__val;			\
		__asm __volatile("mov %1,%%fs:%0"			\
		    : "=m" (*(struct __s *)(__pcpu_offset(name)))	\
		    : "r" (__s));					\
	} else {							\
		*__PCPU_PTR(name) = __val;				\
	}								\
}

#define	PCPU_GET(member)	__PCPU_GET(pc_ ## member)
#define	PCPU_ADD(member, val)	__PCPU_ADD(pc_ ## member, val)
#define	PCPU_INC(member)	__PCPU_INC(pc_ ## member)
#define	PCPU_PTR(member)	__PCPU_PTR(pc_ ## member)
#define	PCPU_SET(member, val)	__PCPU_SET(pc_ ## member, val)

static __inline struct thread *
__curthread(void)
{
	struct thread *td;

	__asm __volatile("movl %%fs:0,%0" : "=r" (td));
	return (td);
}
#define	curthread		(__curthread())

#else /* !lint || defined(__GNUCLIKE_ASM) && defined(__GNUCLIKE___TYPEOF) */

#error "this file needs to be ported to your compiler"

#endif /* lint, etc. */

#endif /* _FREEBSD_KERNEL */

#endif /* !_BSD_MACHINE_PCPU_H_ */
