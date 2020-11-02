#ifndef OSDEP_H_
#define OSDEP_H_

#ifndef SHENANGO
#include <pthread.h>

#define dedup_mutex_t pthread_mutex_t
#define dedup_mutex_init pthread_mutex_init
#define dedup_mutex_destroy pthread_mutex_destroy
#define dedup_mutex_lock pthread_mutex_lock
#define dedup_mutex_unlock pthread_mutex_unlock

#define dedup_spinlock_t pthread_spinlock_t
#define dedup_spin_init pthread_spin_init
#define dedup_spin_destroy pthread_spin_destroy
#define dedup_spin_lock pthread_spin_lock
#define dedup_spin_unlock pthread_spin_unlock

#define dedup_cond_t pthread_cond_t
#define dedup_cond_init pthread_cond_init
#define dedup_cond_destroy pthread_cond_destroy
#define dedup_cond_wait pthread_cond_wait
#define dedup_cond_broadcast pthread_cond_broadcast
#define dedup_cond_signal pthread_cond_signal

#define dedup_thread_t pthread_t
#define dedup_thread_create pthread_create
#define dedup_thread_join pthread_join

#else

#include <base/lock.h>
#include <runtime/sync.h>
#include <shen.h>

#undef assert


typedef mutex_t dedup_mutex_t;

static inline int dedup_mutex_init(mutex_t *a, void *b) {
	mutex_init(a);
	return 0;
}

static inline int dedup_mutex_destroy(mutex_t *a) {
	return 0;
}

#define dedup_mutex_lock mutex_lock
#define dedup_mutex_unlock mutex_unlock

typedef spinlock_t dedup_spinlock_t; 
static inline int dedup_spin_init(spinlock_t *a, void *b) {
	spin_lock_init(a);
	return 0;
}

static inline int dedup_spin_destroy(spinlock_t *a) {
	return 0;
}
#define dedup_spin_lock spin_lock_np
#define dedup_spin_unlock spin_unlock_np

typedef condvar_t dedup_cond_t;
#define dedup_cond_init(c,a) { \
    BUG_ON(a != NULL); \
    condvar_init(c); \
}
#define dedup_cond_destroy(a) ;
#define dedup_cond_wait condvar_wait
#define dedup_cond_broadcast condvar_broadcast
#define dedup_cond_signal condvar_signal

typedef struct join_handle * dedup_thread_t;
#define dedup_thread_create(a,b,c,d) { \
	assert(b == NULL); \
	thread_spawn_joinable(a,c,d); \
}
#define dedup_thread_join thread_join

#define PTHREAD_PROCESS_PRIVATE 0

#endif
#endif