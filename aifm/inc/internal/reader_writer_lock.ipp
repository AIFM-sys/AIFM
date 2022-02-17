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

FORCE_INLINE ReaderWriterLock::ReaderWriterLock() { writer_barrier_ = false; }

FORCE_INLINE void ReaderWriterLock::lock_reader() {
retry:
  rcu_lock_.reader_lock();
  if (unlikely(ACCESS_ONCE(writer_barrier_))) {
    rcu_lock_.reader_unlock();
    do {
      thread_yield();
    } while (unlikely(ACCESS_ONCE(writer_barrier_)));
    goto retry;
  }
}

FORCE_INLINE void ReaderWriterLock::unlock_reader() {
  rcu_lock_.reader_unlock();
}

FORCE_INLINE void ReaderWriterLock::lock_writer() {
  mutex_.Lock();
  writer_barrier_ = true;
  barrier();
  rcu_lock_.writer_sync();
}

FORCE_INLINE void ReaderWriterLock::unlock_writer() {
  writer_barrier_ = false;
  barrier();
  mutex_.Unlock();
}

FORCE_INLINE ReaderLock ReaderWriterLock::get_reader_lock() {
  return ReaderLock(*this);
}

FORCE_INLINE WriterLock ReaderWriterLock::get_writer_lock() {
  return WriterLock(*this);
}

} // namespace far_memory
