#pragma once

#include "helpers.hpp"

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

class WriterLockNp {
private:
  ReaderWriterLock &lock_;
  WriterLockNp(ReaderWriterLock &lock);
  friend class ReaderWriterLock;

public:
  ~WriterLockNp();
  NOT_COPYABLE(WriterLockNp);
  NOT_MOVEABLE(WriterLockNp);
};

// Reader side is very fast. Move most overheads to the writer side.
class ReaderWriterLock {
private:
  bool writer_locked_ = false;
  CachelineAligned(int32_t) reader_cnts_[helpers::kNumCPUs];

public:
  ReaderWriterLock();
  NOT_COPYABLE(ReaderWriterLock);
  NOT_MOVEABLE(ReaderWriterLock);
  void lock_reader();
  void lock_writer();
  void lock_writer_np();
  void unlock_reader();
  void unlock_writer();
  void unlock_writer_np();
  ReaderLock get_reader_lock();
  WriterLock get_writer_lock();
  WriterLockNp get_writer_lock_np();
};

} // namespace far_memory

#include "internal/reader_writer_lock.ipp"
