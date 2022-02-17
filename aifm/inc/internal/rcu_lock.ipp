extern "C" {
#include <asm/atomic.h>
#include <base/assert.h>
#include <base/compiler.h>
#include <runtime/preempt.h>
#include <runtime/thread.h>
}

namespace far_memory {

FORCE_INLINE void RCULock::reader_lock() {
  while (unlikely(ACCESS_ONCE(sync_barrier_))) {
    thread_yield();
  }
  __reader_lock();
}

FORCE_INLINE RCULock::RCULock() : sync_barrier_(false) {
  memset(aligned_cnts_, 0, sizeof(aligned_cnts_));
}

} // namespace far_memory
