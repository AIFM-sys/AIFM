/*
 * defs.h - internal runtime definitions
 */

#pragma once

#include <base/stddef.h>
#include <base/list.h>
#include <base/mem.h>
#include <base/tcache.h>
#include <base/gen.h>
#include <base/lrpc.h>
#include <base/thread.h>
#include <base/time.h>
#include <net/ethernet.h>
#include <net/ip.h>
#include <iokernel/control.h>
#include <net/mbufq.h>
#include <runtime/net.h>
#include <runtime/runtime.h>
#include <runtime/thread.h>
#include <runtime/rcu.h>
#include <runtime/preempt.h>


/*
 * constant limits
 * TODO: make these configurable?
 */

#define DISABLE_PMC

#define RUNTIME_MAX_THREADS		100000
#define RUNTIME_STACK_SIZE		256 * KB
#define RUNTIME_GUARD_SIZE		256 * KB
#define RUNTIME_RQ_SIZE			32
#define RUNTIME_SOFTIRQ_LOCAL_BUDGET	16
#define RUNTIME_SOFTIRQ_REMOTE_BUDGET	16
#define RUNTIME_MAX_TIMERS		4096
#define RUNTIME_SCHED_POLL_ITERS	0
#define RUNTIME_SCHED_MIN_POLL_US	10
#define RUNTIME_WATCHDOG_US		50
#define RUNTIME_PMC_US			10

/*
 * Trap frame support
 */

/*
 * See the "System V Application Binary Interface" for a full explation of
 * calling and argument passing conventions.
 */

struct thread_tf {
	/* argument registers, can be clobbered by callee */
	uint64_t rdi; /* first argument */
	uint64_t rsi;
	uint64_t rdx;
	uint64_t rcx;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;

	/* callee-saved registers */
	uint64_t rbx;
	uint64_t rbp;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;

	/* special-purpose registers */
	uint64_t rax;	/* holds return value */
	uint64_t rip;	/* instruction pointer */
	uint64_t rsp;	/* stack pointer */
};

#define ARG0(tf)        ((tf)->rdi)
#define ARG1(tf)        ((tf)->rsi)
#define ARG2(tf)        ((tf)->rdx)
#define ARG3(tf)        ((tf)->rcx)
#define ARG4(tf)        ((tf)->r8)
#define ARG5(tf)        ((tf)->r9)


/*
 * Thread support
 */

enum {
	THREAD_STATE_RUNNING = 0,
	THREAD_STATE_RUNNABLE,
	THREAD_STATE_SLEEPING,
};

struct stack;

struct thread {
	struct thread_tf	tf;
	struct list_node	link;
	struct stack		*stack;
	unsigned int		main_thread:1;
	unsigned int		state;
	unsigned int		stack_busy;
	unsigned int		last_cpu;
	unsigned int            status;
};

typedef void (*runtime_fn_t)(void);

/* assembly helper routines from switch.S */
extern void __jmp_thread(struct thread_tf *tf) __noreturn;
extern void __jmp_thread_direct(struct thread_tf *oldtf,
				struct thread_tf *newtf,
				unsigned int *stack_busy);
extern void __jmp_runtime(struct thread_tf *tf, runtime_fn_t fn,
			  void *stack);
extern void __jmp_runtime_nosave(runtime_fn_t fn, void *stack) __noreturn;


/*
 * Stack support
 */

#define STACK_PTR_SIZE	(RUNTIME_STACK_SIZE / sizeof(uintptr_t))
#define GUARD_PTR_SIZE	(RUNTIME_GUARD_SIZE / sizeof(uintptr_t))

struct stack {
	uintptr_t	usable[STACK_PTR_SIZE];
	uintptr_t	guard[GUARD_PTR_SIZE]; /* unreadable and unwritable */
};

DECLARE_PERTHREAD(struct tcache_perthread, stack_pt);

/**
 * stack_alloc - allocates a stack
 *
 * Stack allocation is extremely cheap, think less than taking a lock.
 *
 * Returns an unitialized stack.
 */
static inline struct stack *stack_alloc(void)
{
	return tcache_alloc(&perthread_get(stack_pt));
}

/**
 * stack_free - frees a stack
 * @s: the stack to free
 */
static inline void stack_free(struct stack *s)
{
	tcache_free(&perthread_get(stack_pt), (void *)s);
}

#define RSP_ALIGNMENT	16

static inline void assert_rsp_aligned(uint64_t rsp)
{
	/*
	 * The stack must be 16-byte aligned at process entry according to
	 * the System V Application Binary Interface (section 3.4.1).
	 *
	 * The callee assumes a return address has been pushed on the aligned
	 * stack by CALL, so we look for an 8 byte offset.
	 */
	assert(rsp % RSP_ALIGNMENT == sizeof(void *));
}

/**
 * stack_init_to_rsp - sets up an exit handler and returns the top of the stack
 * @s: the stack to initialize
 * @exit_fn: exit handler that is called when the top of the call stack returns
 *
 * Returns the top of the stack as a stack pointer.
 */
static inline uint64_t stack_init_to_rsp(struct stack *s, void (*exit_fn)(void))
{
	uint64_t rsp;

	s->usable[STACK_PTR_SIZE - 1] = (uintptr_t)exit_fn;
	rsp = (uint64_t)&s->usable[STACK_PTR_SIZE - 1];
	assert_rsp_aligned(rsp);
	return rsp;
}

/**
 * stack_init_to_rsp_with_buf - sets up an exit handler and returns the top of
 * the stack, reserving space for a buffer above
 * @s: the stack to initialize
 * @buf: a pointer to store the buffer pointer
 * @buf_len: the length of the buffer to reserve
 * @exit_fn: exit handler that is called when the top of the call stack returns
 *
 * Returns the top of the stack as a stack pointer.
 */
static inline uint64_t
stack_init_to_rsp_with_buf(struct stack *s, void **buf, size_t buf_len,
			   void (*exit_fn)(void))
{
	uint64_t rsp, pos = STACK_PTR_SIZE;

	/* reserve the buffer */
	pos -= div_up(buf_len, sizeof(uint64_t));
	pos = align_down(pos, RSP_ALIGNMENT / sizeof(uint64_t));
	*buf = (void *)&s->usable[pos];

	/* setup for usage as stack */
	s->usable[--pos] = (uintptr_t)exit_fn;
	rsp = (uint64_t)&s->usable[pos];
	assert_rsp_aligned(rsp);
	return rsp;
}


/*
 * ioqueues
 */

struct iokernel_control {
	int fd;
	mem_key_t key;
	struct control_hdr *hdr;
	struct thread_spec *threads;
	const struct iokernel_info *iok_info;
	void *tx_buf;
	size_t tx_len;
};

extern struct iokernel_control iok;
extern void *iok_shm_alloc(size_t size, size_t alignment, shmptr_t *shm_out);

/*
 * Direct hardware queue support
 */

struct hardware_q {
	void		*descriptor_table;
	uint32_t		*consumer_idx;
	uint32_t		*shadow_tail;
	uint32_t		descriptor_log_size;
	uint32_t		nr_descriptors;
	uint32_t		parity_byte_offset;
	uint32_t		parity_bit_mask;
};

static inline bool hardware_q_pending(struct hardware_q *q)
{
	uint32_t tail, idx, parity, hd_parity;
	unsigned char *addr;

	tail = ACCESS_ONCE(*q->consumer_idx);
	idx = tail & (q->nr_descriptors - 1);
	parity = !!(tail & q->nr_descriptors);
	addr = q->descriptor_table +
		     (idx << q->descriptor_log_size) + q->parity_byte_offset;
	hd_parity = !!(ACCESS_ONCE(*addr) & q->parity_bit_mask);

	return parity == hd_parity;
}

/*
 * Storage support
 */

#ifdef DIRECT_STORAGE

extern bool cfg_storage_enabled;

struct storage_q {

	spinlock_t lock;

	unsigned int outstanding_reqs;
	void *spdk_qp_handle;

	struct hardware_q hq;

	unsigned long pad[1];
};

static inline bool storage_available_completions(struct storage_q *q)
{
	return cfg_storage_enabled && hardware_q_pending(&q->hq);
}

static inline bool storage_pending_completions(struct storage_q *q)
{
	extern unsigned long device_latency_us;
	return cfg_storage_enabled && q->outstanding_reqs > 0 &&
	       device_latency_us <= 10;
}

extern int storage_proc_completions(struct storage_q *q,
	    unsigned int budget, struct thread **wakeable_threads);
#else

struct storage_q {};

static inline bool storage_available_completions(struct storage_q *q)
{
	return false;
}

static inline bool storage_pending_completions(struct storage_q *q)
{
	return false;
}


static inline int storage_proc_completions(struct storage_q *q,
	    unsigned int budget, struct thread **wakeable_threads)
{
	return 0;
}

#endif


/*
 * Per-kernel-thread State
 */

/*
 * These are per-kthread stat counters. It's recommended that most counters be
 * monotonically increasing, as that decouples the counters from any particular
 * collection time period. However, it may not be possible to represent all
 * counters this way.
 *
 * Don't use these enums directly. Instead, use the STAT() macro.
 */
enum {
	/* scheduler counters */
	STAT_RESCHEDULES = 0,
	STAT_SCHED_CYCLES,
	STAT_PROGRAM_CYCLES,
	STAT_THREADS_STOLEN,
	STAT_SOFTIRQS_STOLEN,
	STAT_SOFTIRQS_LOCAL,
	STAT_PARKS,
	STAT_PREEMPTIONS,
	STAT_CORE_MIGRATIONS,
	STAT_LOCAL_RUNS,
	STAT_REMOTE_RUNS,
	STAT_LOCAL_WAKES,
	STAT_REMOTE_WAKES,
	STAT_RQ_OVERFLOW,

	/* network stack counters */
	STAT_RX_BYTES,
	STAT_RX_PACKETS,
	STAT_TX_BYTES,
	STAT_TX_PACKETS,
	STAT_DROPS,
	STAT_RX_TCP_IN_ORDER,
	STAT_RX_TCP_OUT_OF_ORDER,
	STAT_RX_TCP_TEXT_CYCLES,
	STAT_TXQ_OVERFLOW,

	/* directpath stats */
	STAT_FLOW_STEERING_CYCLES,
	STAT_RX_HW_DROP,

	/* total number of counters */
	STAT_NR,
};

struct timer_idx;

struct kthread {
	/* 1st cache-line */
	spinlock_t		lock;
	uint32_t		kthread_idx;
	uint32_t		rq_head;
	uint32_t		rq_tail;
	struct list_head	rq_overflow;
	struct lrpc_chan_in	rxq;
	int			park_efd;
	bool			parked;

	/* 2nd cache-line */
	struct q_ptrs		*q_ptrs;
	struct mbufq		txpktq_overflow;
	struct mbufq		txcmdq_overflow;
	unsigned int		rcu_gen;
	unsigned int		curr_cpu;
	uint32_t		l_rq_head;
	uint32_t 		l_rq_tail;
	thread_t 		**rq_ptr;

	/* 3rd cache-line */
	struct lrpc_chan_out	txpktq;
	struct lrpc_chan_out	txcmdq;

	/* 4th-7th cache-line */
	thread_t		*rq[RUNTIME_RQ_SIZE];

	/* 8th cache-line */
	spinlock_t		timer_lock;
	unsigned int		timern;
	struct timer_idx	*timers;
	bool			prioritized_status;
	unsigned long		pad2[5];

	/* 9th cache-line, storage nvme queues */
	struct storage_q		storage_q;

	/* 10th cache-line, direct path queues */
	struct hardware_q		*directpath_rxq;
	struct direct_txq		*directpath_txq;
	unsigned long		pad4[6];

	/* 11th-14th cache-line */
	thread_t		*h_rq[RUNTIME_RQ_SIZE];

	/* 15th cache-line, statistics counters */
	uint64_t		stats[STAT_NR];
};

/* compile-time verification of cache-line alignment */
BUILD_ASSERT(offsetof(struct kthread, lock) % CACHE_LINE_SIZE == 0);
BUILD_ASSERT(offsetof(struct kthread, q_ptrs) % CACHE_LINE_SIZE == 0);
BUILD_ASSERT(offsetof(struct kthread, txpktq) % CACHE_LINE_SIZE == 0);
BUILD_ASSERT(offsetof(struct kthread, rq) % CACHE_LINE_SIZE == 0);
BUILD_ASSERT(offsetof(struct kthread, timer_lock) % CACHE_LINE_SIZE == 0);
BUILD_ASSERT(offsetof(struct kthread, storage_q) % CACHE_LINE_SIZE == 0);
BUILD_ASSERT(offsetof(struct kthread, directpath_rxq) % CACHE_LINE_SIZE == 0);
BUILD_ASSERT(offsetof(struct kthread, stats) % CACHE_LINE_SIZE == 0);

extern __thread struct kthread *mykthread;
extern __thread unsigned int __curr_cpu;

/**
 * myk - returns the per-kernel-thread data
 */
static inline struct kthread *myk(void)
{
	return mykthread;
}

/**
 * getk - returns the per-kernel-thread data and disables preemption
 *
 * WARNING: If you're using myk() instead of getk(), that's a bug if preemption
 * is enabled. The local kthread can change at anytime.
 */
static inline struct kthread *getk(void)
{
	preempt_disable();
	return mykthread;
}

/**
 * putk - reenables preemption after calling getk()
 */
static inline void putk(void)
{
	preempt_enable();
}

DECLARE_SPINLOCK(klock);
extern unsigned int spinks;
extern unsigned int nrks;
extern struct kthread *ks[NCPU];

extern void kthread_park(bool voluntary);
extern void kthread_wait_to_attach(void);

struct cpu_record {
	struct kthread *recent_kthread;
	unsigned long sibling_core;
	unsigned long pad[6];
};

BUILD_ASSERT(sizeof(struct cpu_record) == CACHE_LINE_SIZE);

extern struct cpu_record cpu_map[NCPU];
extern int preferred_socket;
void pmc_periodic(struct kthread *k);

/**
 * STAT - gets a stat counter
 *
 * e.g. STAT(DROPS)++;
 *
 * Deliberately could race with preemption.
 */
#define STAT(counter) (myk()->stats[STAT_ ## counter])


/*
 * Softirq support
 */

/* the maximum number of events to handle in a softirq invocation */
#define SOFTIRQ_MAX_BUDGET	128

extern bool disable_watchdog;

extern thread_t *softirq_run_thread(struct kthread *k, unsigned int budget);
extern void softirq_run(unsigned int budget);


/*
 * Network stack
 */

struct net_cfg {
	struct shm_region	tx_region;
	struct shm_region	rx_region;
	uint32_t		addr;
	uint32_t		netmask;
	uint32_t		gateway;
	struct eth_addr		mac;
	uint8_t			pad[14];
} __packed;

BUILD_ASSERT(sizeof(struct net_cfg) == CACHE_LINE_SIZE);

extern struct net_cfg netcfg;

#define MAX_ARP_STATIC_ENTRIES 1024
struct cfg_arp_static_entry {
	uint32_t ip;
	struct eth_addr addr;
};
extern int arp_static_count;
extern struct cfg_arp_static_entry static_entries[MAX_ARP_STATIC_ENTRIES];

extern void __net_recurrent(void);
extern void net_rx_softirq(struct rx_net_hdr **hdrs, unsigned int nr);
extern void net_rx_softirq_direct(struct mbuf **ms, unsigned int nr);

struct trans_entry;
struct net_driver_ops {
	int (*rx_batch)(struct hardware_q *rxq, struct mbuf **ms, unsigned int budget);
	int (*tx_single)(struct mbuf *m);
	int (*steer_flows)(unsigned int *new_fg_assignment);
	int (*register_flow)(unsigned int affininty, struct trans_entry *e, void **handle_out);
	int (*deregister_flow)(struct trans_entry *e, void *handle);
	uint32_t (*get_flow_affinity)(uint8_t ipproto, uint16_t local_port, struct netaddr remote);
	int (*rxq_has_work)(struct hardware_q *rxq);
};

extern struct net_driver_ops net_ops;

#ifdef DIRECTPATH

extern bool cfg_directpath_enabled;
struct direct_txq {};

static inline bool rx_pending(struct hardware_q *rxq)
{
	return cfg_directpath_enabled && net_ops.rxq_has_work(rxq);
}

extern size_t directpath_rx_buf_pool_sz(unsigned int nrqs);

#else

static inline bool rx_pending(struct hardware_q *rxq)
{
	return false;
}

static inline size_t directpath_rx_buf_pool_sz(unsigned int nrqs)
{
	return 0;
}

#endif



/*
 * Timer support
 */

extern void timer_softirq(struct kthread *k, unsigned int budget);
extern void timer_merge(struct kthread *r);
extern uint64_t timer_earliest_deadline(void);

struct timer_idx {
	uint64_t		deadline_us;
	struct timer_entry	*e;
};

/**
 * timer_needed - returns true if pending timers have to be handled
 * @k: the kthread to check
 */
static inline bool timer_needed(struct kthread *k)
{
	/* deliberate race condition */
	return k->timern > 0 && k->timers[0].deadline_us <= microtime();
}


static inline bool softirq_work_available(struct kthread *k)
{
	bool work_available;

	work_available = rx_pending(k->directpath_rxq) ||
	  !lrpc_empty(&k->rxq) || timer_needed(k);

	work_available |= storage_available_completions(&k->storage_q);

	return work_available;
}

static inline bool timer_available_soon(struct kthread *k, uint64_t now)
{
	return k->timern > 0 &&
		(k->timers[0].deadline_us - 10) * cycles_per_us + start_tsc <= now;
}

/**
 * softirq_work_soon - returns true if a softirq is likely
 * to be ready in the next 10us
 *
 * @k: the kthread to check
 * @now: current timestamp
 */
static inline bool softirq_work_soon(struct kthread *k, uint64_t now)
{
	return storage_pending_completions(&k->storage_q) ||
		timer_available_soon(k, now);
}

/*
 * Init
 */

/* per-thread initialization */
extern int kthread_init_thread(void);
extern int ioqueues_init_thread(void);
extern int stack_init_thread(void);
extern int timer_init_thread(void);
extern int sched_init_thread(void);
extern int stat_init_thread(void);
extern int net_init_thread(void);
extern int smalloc_init_thread(void);
extern int storage_init_thread(void);
extern int directpath_init_thread(void);

/* global initialization */
extern int kthread_init(void);
extern int ioqueues_init(void);
extern int stack_init(void);
extern int sched_init(void);
extern int preempt_init(void);
extern int net_init(void);
extern int arp_init(void);
extern int trans_init(void);
extern int smalloc_init(void);
extern int storage_init(void);
extern int directpath_init(void);

/* late initialization */
extern int ioqueues_register_iokernel(void);
extern int arp_init_late(void);
extern int stat_init_late(void);
extern int tcp_init_late(void);
extern int rcu_init_late(void);
extern int directpath_init_late(void);

/* configuration loading */
extern int cfg_load(const char *path);

/* runtime entry helpers */
extern void sched_start(void) __noreturn;
extern int thread_spawn_main(thread_fn_t fn, void *arg);
extern void thread_cede(void);
extern void join_kthread(struct kthread *k);
