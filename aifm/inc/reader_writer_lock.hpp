#pragma once

#include <sync.h>

#include "helpers.hpp"
#include "rcu_lock.hpp"

namespace far_memory {

class ReaderWriterLock;

class ReaderLock {
private:
  ReaderWriterLock &lock_;
  ReaderLock(ReaderWriterLock &lock);
  friend class ReaderWriterLock;

public:
  ~ReaderLock();
  NOT_COPYABLE(ReaderLock);
  NOT_MOVEABLE(ReaderLock);
};

class WriterLock {
private:
  ReaderWriterLock &lock_;
  WriterLock(ReaderWriterLock &lock);
  friend class ReaderWriterLock;

public:
  ~WriterLock();
  NOT_COPYABLE(WriterLock);
  NOT_MOVEABLE(WriterLock);
};

// Reader side is very fast. Move most overheads to the writer side.
class ReaderWriterLock {
private:
  bool writer_barrier_;
  RCULock rcu_lock_;
  rt::Mutex mutex_;

public:
  ReaderWriterLock();
  NOT_COPYABLE(ReaderWriterLock);
  NOT_MOVEABLE(ReaderWriterLock);
  void lock_reader();
  void lock_writer();
  void unlock_reader();
  void unlock_writer();
  ReaderLock get_reader_lock();
  WriterLock get_writer_lock();
};

} // namespace far_memory

#include "internal/reader_writer_lock.ipp"
