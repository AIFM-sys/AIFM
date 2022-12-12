#pragma once

extern "C" {
#include <base/compiler.h>
#include <runtime/thread.h>
}

#include <cstring>

namespace far_memory {

FORCE_INLINE ReaderLock::ReaderLock(ReaderWriterLock &lock) : lock_(lock) {
  lock.lock_reader();
}

FORCE_INLINE ReaderLock::~ReaderLock() { lock_.unlock_reader(); }

FORCE_INLINE WriterLock::WriterLock(ReaderWriterLock &lock) : lock_(lock) {
  lock.lock_writer();
}

FORCE_INLINE WriterLock::~WriterLock() { lock_.unlock_writer(); }

FORCE_INLINE WriterLockNp::WriterLockNp(ReaderWriterLock &lock) : lock_(lock) {
  lock.lock_writer_np();
}

FORCE_INLINE WriterLockNp::~WriterLockNp() { lock_.unlock_writer_np(); }

FORCE_INLINE ReaderWriterLock::ReaderWriterLock() {
  memset(reader_cnts_, 0, sizeof(reader_cnts_));
}

FORCE_INLINE void ReaderWriterLock::lock_reader() {
  while (unlikely(ACCESS_ONCE(writer_locked_))) {
    thread_yield();
  }
  preempt_disable();
  auto core_num = get_core_num();
  ACCESS_ONCE(reader_cnts_[core_num].data)++;
  preempt_enable();
}

FORCE_INLINE void ReaderWriterLock::unlock_reader() {
  preempt_disable();
  auto core_num = get_core_num();
  ACCESS_ONCE(reader_cnts_[core_num].data)--;
  preempt_enable();
}

FORCE_INLINE void ReaderWriterLock::lock_writer_np() {
retry:
  preempt_disable();
  while (!__sync_bool_compare_and_swap(&writer_locked_, false, true)) {
    preempt_enable();
    thread_yield();
    goto retry;
  }
  int32_t sum = 0;
  FOR_ALL_SOCKET0_CORES(i) { sum += ACCESS_ONCE(reader_cnts_[i].data); }
  if (sum) {
    store_release(&writer_locked_, false);
    preempt_enable();
    thread_yield();
    goto retry;
  }
}

FORCE_INLINE void ReaderWriterLock::unlock_writer_np() {
  store_release(&writer_locked_, false);
  preempt_enable();
  assert(preempt_enabled());
}

FORCE_INLINE void ReaderWriterLock::lock_writer() {
  while (!__sync_bool_compare_and_swap(&writer_locked_, false, true)) {
    thread_yield();
  }
retry:
  int32_t sum = 0;
  FOR_ALL_SOCKET0_CORES(i) { sum += ACCESS_ONCE(reader_cnts_[i].data); }
  if (sum) {
    thread_yield();
    goto retry;
  }
}

FORCE_INLINE void ReaderWriterLock::unlock_writer() {
  store_release(&writer_locked_, false);
}

FORCE_INLINE ReaderLock ReaderWriterLock::get_reader_lock() {
  return ReaderLock(*this);
}

FORCE_INLINE WriterLock ReaderWriterLock::get_writer_lock() {
  return WriterLock(*this);
}

FORCE_INLINE WriterLockNp ReaderWriterLock::get_writer_lock_np() {
  return WriterLockNp(*this);
}
} // namespace far_memory