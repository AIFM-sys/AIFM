/*-
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and William Jolitz of UUNET Technologies Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Derived from hp300 version by Mike Hibler, this version by William
 * Jolitz uses a recursive map [a pde points to the page directory] to
 * map the page tables using the pagetables themselves. This is done to
 * reduce the impact on kernel virtual memory for lots of sparse address
 * space, and to reduce the cost of memory to each process.
 *
 *	from: hp300: @(#)pmap.h	7.2 (Berkeley) 12/16/90
 *	from: @(#)pmap.h	7.4 (Berkeley) 5/12/91
 * $FreeBSD: src/sys/i386/include/pmap.h,v 1.140.2.2.2.1 2009/10/25 01:10:29 kensmith Exp $
 */

#ifndef _BSD_MACHINE_PMAP_H_
#define	_BSD_MACHINE_PMAP_H_

/*
 * Page-directory and page-table entries follow this format, with a few
 * of the fields not present here and there, depending on a lot of things.
 */
				/* ---- Intel Nomenclature ---- */
#define	PG_V		0x001	/* P	Valid			*/
#define PG_RW		0x002	/* R/W	Read/Write		*/
#define PG_U		0x004	/* U/S  User/Supervisor		*/
#define	PG_NC_PWT	0x008	/* PWT	Write through		*/
#define	PG_NC_PCD	0x010	/* PCD	Cache disable		*/
#define PG_A		0x020	/* A	Accessed		*/
#define	PG_M		0x040	/* D	Dirty			*/
#define	PG_PS		0x080	/* PS	Page size (0=4k,1=4M)	*/
#define	PG_PTE_PAT	0x080	/* PAT	PAT index		*/
#define	PG_G		0x100	/* G	Global			*/
#define	PG_AVAIL1	0x200	/*    /	Available for system	*/
#define	PG_AVAIL2	0x400	/*   <	programmers use		*/
#define	PG_AVAIL3	0x800	/*    \				*/
#define	PG_PDE_PAT	0x1000	/* PAT	PAT index		*/
#ifdef PAE
#define	PG_NX		(1ull<<63) /* No-execute */
#endif


/* Our various interpretations of the above */
#define PG_W		PG_AVAIL1	/* "Wired" pseudoflag */
#define	PG_MANAGED	PG_AVAIL2
#ifdef PAE
#define	PG_FRAME	(0x000ffffffffff000ull)
#define	PG_PS_FRAME	(0x000fffffffe00000ull)
#else
#define	PG_FRAME	(~PAGE_MASK)
#define	PG_PS_FRAME	(0xffc00000)
#endif
#define	PG_PROT		(PG_RW|PG_U)	/* all protection bits . */
#define PG_N		(PG_NC_PWT|PG_NC_PCD)	/* Non-cacheable */

/* Page level cache control fields used to determine the PAT type */
#define PG_PDE_CACHE	(PG_PDE_PAT | PG_NC_PWT | PG_NC_PCD)
#define PG_PTE_CACHE	(PG_PTE_PAT | PG_NC_PWT | PG_NC_PCD)

/*
 * Promotion to a 2 or 4MB (PDE) page mapping requires that the corresponding
 * 4KB (PTE) page mappings have identical settings for the following fields:
 */
#define PG_PTE_PROMOTE	(PG_MANAGED | PG_W | PG_G | PG_PTE_PAT | \
	    PG_M | PG_A | PG_NC_PCD | PG_NC_PWT | PG_U | PG_RW | PG_V)

/*
 * Page Protection Exception bits
 */

#define PGEX_P		0x01	/* Protection violation vs. not present */
#define PGEX_W		0x02	/* during a Write cycle */
#define PGEX_U		0x04	/* access from User mode (UPL) */
#define PGEX_RSV	0x08	/* reserved PTE field is non-zero */
#define PGEX_I		0x10	/* during an instruction fetch */

/*
 * Size of Kernel address space.  This is the number of page table pages
 * (4MB each) to use for the kernel.  256 pages == 1 Gigabyte.
 * This **MUST** be a multiple of 4 (eg: 252, 256, 260, etc).
 * For PAE, the page table page unit size is 2MB.  This means that 512 pages
 * is 1 Gigabyte.  Double everything.  It must be a multiple of 8 for PAE.
 */
#ifndef KVA_PAGES
#ifdef PAE
#define KVA_PAGES	512
#else
#define KVA_PAGES	256
#endif
#endif

/*
 * Pte related macros
 */
#define VADDR(pdi, pti) ((vm_offset_t)(((pdi)<<PDRSHIFT)|((pti)<<PAGE_SHIFT)))

/* Initial number of kernel page tables. */
#ifndef NKPT
#ifdef PAE
/* 152 page tables needed to map 16G (76B "struct vm_page", 2M page tables). */
#define	NKPT		240
#else
/* 18 page tables needed to map 4G (72B "struct vm_page", 4M page tables). */
#define	NKPT		30
#endif
#endif

#ifndef NKPDE
#define NKPDE	(KVA_PAGES)	/* number of page tables/pde's */
#endif

/*
 * The *PTDI values control the layout of virtual memory
 *
 * XXX This works for now, but I am not real happy with it, I'll fix it
 * right after I fix locore.s and the magic 28K hole
 */
#define	KPTDI		(NPDEPTD-NKPDE)	/* start of kernel virtual pde's */
#define	PTDPTDI		(KPTDI-NPGPTD)	/* ptd entry that points to ptd! */

/*
 * XXX doesn't really belong here I guess...
 */
#define ISA_HOLE_START    0xa0000
#define ISA_HOLE_LENGTH (0x100000-ISA_HOLE_START)

#ifndef LOCORE

#include <sys/bsd_queue.h>
#include <sys/bsd__lock.h>
#include <sys/bsd__mutex.h>

#ifdef PAE

typedef uint64_t pdpt_entry_t;
typedef uint64_t pd_entry_t;
typedef uint64_t pt_entry_t;

#define	PTESHIFT	(3)
#define	PDESHIFT	(3)

#else

typedef uint32_t pd_entry_t;
typedef uint32_t pt_entry_t;

#define	PTESHIFT	(2)
#define	PDESHIFT	(2)

#endif

/*
 * Address of current address space page table maps and directories.
 */
#ifdef _FREEBSD_KERNEL
extern pt_entry_t PTmap[];
extern pd_entry_t PTD[];
extern pd_entry_t PTDpde[];

#ifdef PAE
extern pdpt_entry_t *IdlePDPT;
#endif
extern pd_entry_t *IdlePTD;	/* physical address of "Idle" state directory */

/*
 * virtual address to page table entry and
 * to physical address.
 * Note: these work recursively, thus vtopte of a pte will give
 * the corresponding pde that in turn maps it.
 */
#define	vtopte(va)	(PTmap + i386_btop(va))
#define	vtophys(va)	pmap_kextract((vm_offset_t)(va))

#ifdef XEN
#include <sys/bsd_param.h>
#include <machine/xen/xen-bsd_os.h>
#include <machine/xen/bsd_xenvar.h>
#include <machine/xen/bsd_xenpmap.h>

extern pt_entry_t pg_nx;

#define PG_FREEBSD_KERNEL  (PG_V | PG_A | PG_RW | PG_M)

#define MACH_TO_VM_PAGE(ma) PHYS_TO_VM_PAGE(xpmap_mtop((ma)))
#define VM_PAGE_TO_MACH(m) xpmap_ptom(VM_PAGE_TO_PHYS((m)))

static __inline vm_paddr_t
pmap_kextract_ma(vm_offset_t va)
{
        vm_paddr_t ma;
        if ((ma = PTD[va >> PDRSHIFT]) & PG_PS) {
                ma = (ma & ~(NBPDR - 1)) | (va & (NBPDR - 1));
        } else {
                ma = (*vtopte(va) & PG_FRAME) | (va & PAGE_MASK);
        }
        return ma;
}

static __inline vm_paddr_t
pmap_kextract(vm_offset_t va)
{
        return xpmap_mtop(pmap_kextract_ma(va));
}
#define vtomach(va)     pmap_kextract_ma(((vm_offset_t) (va)))

vm_paddr_t pmap_extract_ma(struct pmap *pmap, vm_offset_t va);

void    pmap_kenter_ma(vm_offset_t va, vm_paddr_t pa);
void    pmap_map_readonly(struct pmap *pmap, vm_offset_t va, int len);
void    pmap_map_readwrite(struct pmap *pmap, vm_offset_t va, int len);

static __inline pt_entry_t
pte_load_store(pt_entry_t *ptep, pt_entry_t v)
{
	pt_entry_t r;

	v = xpmap_ptom(v);
	r = *ptep;
	PT_SET_VA(ptep, v, TRUE);
	return (r);
}

static __inline pt_entry_t
pte_load_store_ma(pt_entry_t *ptep, pt_entry_t v)
{
	pt_entry_t r;

	r = *ptep;
	PT_SET_VA_MA(ptep, v, TRUE);
	return (r);
}

#define	pte_load_clear(ptep)	pte_load_store((ptep), (pt_entry_t)0ULL)

#define	pte_store(ptep, pte)	pte_load_store((ptep), (pt_entry_t)pte)
#define	pte_store_ma(ptep, pte)	pte_load_store_ma((ptep), (pt_entry_t)pte)
#define	pde_store_ma(ptep, pte)	pte_load_store_ma((ptep), (pt_entry_t)pte)

#elif !defined(XEN)
/*
 *	Routine:	pmap_kextract
 *	Function:
 *		Extract the physical page address associated
 *		kernel virtual address.
 */
static __inline vm_paddr_t
pmap_kextract(vm_offset_t va)
{
	vm_paddr_t pa;

	if ((pa = PTD[va >> PDRSHIFT]) & PG_PS) {
		pa = (pa & PG_PS_FRAME) | (va & PDRMASK);
	} else {
		pa = *vtopte(va);
		pa = (pa & PG_FRAME) | (va & PAGE_MASK);
	}
	return pa;
}

#define PT_UPDATES_FLUSH()
#endif

#if defined(PAE) && !defined(XEN)

#define	pde_cmpset(pdep, old, new) \
				atomic_cmpset_64((pdep), (old), (new))

static __inline pt_entry_t
pte_load(pt_entry_t *ptep)
{
	pt_entry_t r;

	__asm __volatile(
	    "lock; cmpxchg8b %1"
	    : "=A" (r)
	    : "m" (*ptep), "a" (0), "d" (0), "b" (0), "c" (0));
	return (r);
}

static __inline pt_entry_t
pte_load_store(pt_entry_t *ptep, pt_entry_t v)
{
	pt_entry_t r;

	r = *ptep;
	__asm __volatile(
	    "1:\n"
	    "\tlock; cmpxchg8b %1\n"
	    "\tjnz 1b"
	    : "+A" (r)
	    : "m" (*ptep), "b" ((uint32_t)v), "c" ((uint32_t)(v >> 32)));
	return (r);
}

/* XXXRU move to atomic.h? */
static __inline int
atomic_cmpset_64(volatile uint64_t *dst, uint64_t exp, uint64_t src)
{
	int64_t res = exp;

	__asm __volatile (
	"	lock ;			"
	"	cmpxchg8b %2 ;		"
	"	setz	%%al ;		"
	"	movzbl	%%al,%0 ;	"
	"# atomic_cmpset_64"
	: "+A" (res),			/* 0 (result) */
	  "=m" (*dst)			/* 1 */
	: "m" (*dst),			/* 2 */
	  "b" ((uint32_t)src),
	  "c" ((uint32_t)(src >> 32)));

	return (res);
}

#define	pte_load_clear(ptep)	pte_load_store((ptep), (pt_entry_t)0ULL)

#define	pte_store(ptep, pte)	pte_load_store((ptep), (pt_entry_t)pte)

extern pt_entry_t pg_nx;

#elif !defined(PAE) && !defined (XEN)

#define	pde_cmpset(pdep, old, new) \
				atomic_cmpset_int((pdep), (old), (new))

static __inline pt_entry_t
pte_load(pt_entry_t *ptep)
{
	pt_entry_t r;

	r = *ptep;
	return (r);
}

static __inline pt_entry_t
pte_load_store(pt_entry_t *ptep, pt_entry_t pte)
{
	__asm volatile("xchgl %0, %1" : "+m" (*ptep), "+r" (pte));
	return (pte);
}

#define	pte_load_clear(pte)	atomic_readandclear_int(pte)

static __inline void
pte_store(pt_entry_t *ptep, pt_entry_t pte)
{

	*ptep = pte;
}

#endif /* PAE */

#define	pte_clear(ptep)		pte_store((ptep), (pt_entry_t)0ULL)

#define	pde_store(pdep, pde)	pte_store((pdep), (pde))

#endif /* _FREEBSD_KERNEL */

/*
 * Pmap stuff
 */
struct	pv_entry;
struct	pv_chunk;

struct md_page {
	TAILQ_HEAD(,pv_entry)	pv_list;
	int			pat_mode;
};

struct pmap {
	struct mtx		pm_mtx;
	pd_entry_t		*pm_pdir;	/* KVA of page directory */
	TAILQ_HEAD(,pv_chunk)	pm_pvchunk;	/* list of mappings in pmap */
	u_int			pm_active;	/* active on cpus */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
	LIST_ENTRY(pmap) 	pm_list;	/* List of all pmaps */
#ifdef PAE
	pdpt_entry_t		*pm_pdpt;	/* KVA of page director pointer
						   table */
#endif
	vm_page_t		pm_root;	/* spare page table pages */
};

typedef struct pmap	*pmap_t;

#ifdef _FREEBSD_KERNEL
extern struct pmap	kernel_pmap_store;
#define kernel_pmap	(&kernel_pmap_store)

#define	PMAP_LOCK(pmap)		mtx_lock(&(pmap)->pm_mtx)
#define	PMAP_LOCK_ASSERT(pmap, type) \
				mtx_assert(&(pmap)->pm_mtx, (type))
#define	PMAP_LOCK_DESTROY(pmap)	mtx_destroy(&(pmap)->pm_mtx)
#define	PMAP_LOCK_INIT(pmap)	mtx_init(&(pmap)->pm_mtx, "pmap", \
				    NULL, MTX_DEF | MTX_DUPOK)
#define	PMAP_LOCKED(pmap)	mtx_owned(&(pmap)->pm_mtx)
#define	PMAP_MTX(pmap)		(&(pmap)->pm_mtx)
#define	PMAP_TRYLOCK(pmap)	mtx_trylock(&(pmap)->pm_mtx)
#define	PMAP_UNLOCK(pmap)	mtx_unlock(&(pmap)->pm_mtx)
#endif

/*
 * For each vm_page_t, there is a list of all currently valid virtual
 * mappings of that page.  An entry is a pv_entry_t, the list is pv_list.
 */
typedef struct pv_entry {
	vm_offset_t	pv_va;		/* virtual address for mapping */
	TAILQ_ENTRY(pv_entry)	pv_list;
} *pv_entry_t;

/*
 * pv_entries are allocated in chunks per-process.  This avoids the
 * need to track per-pmap assignments.
 */
#define	_NPCM	11
#define	_NPCPV	336
struct pv_chunk {
	pmap_t			pc_pmap;
	TAILQ_ENTRY(pv_chunk)	pc_list;
	uint32_t		pc_map[_NPCM];	/* bitmap; 1 = free */
	uint32_t		pc_spare[2];
	struct pv_entry		pc_pventry[_NPCPV];
};

#ifdef	_FREEBSD_KERNEL

extern caddr_t	CADDR1;
extern pt_entry_t *CMAP1;
extern vm_paddr_t phys_avail[];
extern vm_paddr_t dump_avail[];
extern int pseflag;
extern int pgeflag;
extern char *ptvmmap;		/* poor name! */
extern vm_offset_t virtual_avail;
extern vm_offset_t virtual_end;

#define	pmap_page_get_memattr(m)	((vm_memattr_t)(m)->md.pat_mode)
#define	pmap_unmapbios(va, sz)	pmap_unmapdev((va), (sz))

void	pmap_bootstrap(vm_paddr_t);
int	pmap_cache_bits(int mode, boolean_t is_pde);
int	pmap_change_attr(vm_offset_t, vm_size_t, int);
void	pmap_init_pat(void);
void	pmap_kenter(vm_offset_t va, vm_paddr_t pa);
void	*pmap_kenter_temporary(vm_paddr_t pa, int i);
void	pmap_kremove(vm_offset_t);
void	*pmap_mapbios(vm_paddr_t, vm_size_t);
void	*pmap_mapdev(vm_paddr_t, vm_size_t);
void	*pmap_mapdev_attr(vm_paddr_t, vm_size_t, int);
boolean_t pmap_page_is_mapped(vm_page_t m);
void	pmap_page_set_memattr(vm_page_t m, vm_memattr_t ma);
void	pmap_unmapdev(vm_offset_t, vm_size_t);
pt_entry_t *pmap_pte(pmap_t, vm_offset_t) __pure2;
void	pmap_set_pg(void);
void	pmap_invalidate_page(pmap_t, vm_offset_t);
void	pmap_invalidate_range(pmap_t, vm_offset_t, vm_offset_t);
void	pmap_invalidate_all(pmap_t);
void	pmap_invalidate_cache(void);
void	pmap_invalidate_cache_range(vm_offset_t, vm_offset_t);

#endif /* _FREEBSD_KERNEL */

#endif /* !LOCORE */

#endif /* !_BSD_MACHINE_PMAP_H_ */
