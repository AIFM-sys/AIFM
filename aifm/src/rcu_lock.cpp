#include "rcu_lock.hpp"

namespace far_memory {

void RCULock::writer_sync() {
  sync_barrier_ = true;
  barrier();

  int32_t sum;
  int32_t snapshot_vers[helpers::kNumCPUs];

retry:
  sum = 0;
  FOR_ALL_SOCKET0_CORES(i) {
    Cnt cnt;
    cnt.raw = ACCESS_ONCE(aligned_cnts_[i].data.raw);
    snapshot_vers[i] = cnt.data.ver;
    sum += cnt.data.c;
  }
  if (sum != 0) {
    thread_yield();
    goto retry;
  }
  FOR_ALL_SOCKET0_CORES(i) {
    if (unlikely(ACCESS_ONCE(aligned_cnts_[i].data.data.ver) !=
                 snapshot_vers[i])) {
      goto retry;
    }
  }

  ACCESS_ONCE(sync_barrier_) = false;
}

void RCULock::__reader_lock() {
  preempt_disable();
  int core = get_core_num();
  Cnt cnt;
  cnt.raw = aligned_cnts_[core].data.raw;
  cnt.data.c++;
  cnt.data.ver++;
  aligned_cnts_[core].data.data = cnt.data;
  preempt_enable();
}

void RCULock::reader_unlock() {
  preempt_disable();
  int core = get_core_num();
  Cnt cnt;
  cnt.raw = aligned_cnts_[core].data.raw;
  cnt.data.c--;
  cnt.data.ver++;
  aligned_cnts_[core].data.data = cnt.data;
  preempt_enable();
}
} // namespace far_memory
