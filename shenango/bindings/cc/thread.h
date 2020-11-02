// thread.h - Support for creating and managing threads

#pragma once

extern "C" {
#include <base/assert.h>
#include <runtime/sync.h>
}

#include <functional>

#include "macros.h"
#include "sync.h"

namespace rt {
namespace thread_internal {

struct join_data {
  join_data(std::function<void()>&& func)
  : done_(false), waiter_(nullptr), func_(std::move(func)) {
    spin_lock_init(&lock_);
  }
  join_data(const std::function<void()>& func)
  : done_(false), waiter_(nullptr), func_(func) {
    spin_lock_init(&lock_);
  }
  DISALLOW_COPY_AND_ASSIGN(join_data);

  spinlock_t		lock_;
  bool			done_;
  thread_t		*waiter_;
  std::function<void()>	func_;
};

extern void ThreadTrampoline(void *arg);
extern void ThreadTrampolineWithJoin(void *arg);

} // namespace thread_internal

// Spawns a new thread by copying.
inline void Spawn(const std::function<void()>& func) {
  void *buf;
  thread_t *th = thread_create_with_buf(thread_internal::ThreadTrampoline, &buf,
					sizeof(std::function<void()>));
  if (unlikely(!th)) BUG();
  new(buf) std::function<void()>(func);
  thread_ready(th);
}

// Spawns a new thread by moving.
inline void Spawn(std::function<void()>&& func) {
  void *buf;
  thread_t *th = thread_create_with_buf(thread_internal::ThreadTrampoline, &buf,
					sizeof(std::function<void()>));
  if (unlikely(!th)) BUG();
  new(buf) std::function<void()>(std::move(func));
  thread_ready(th);
}

// Called from a running thread to exit.
inline void Exit(void) {
  thread_exit();
}

// Called from a running thread to yield.
inline void Yield(void) {
  thread_yield();
}

inline void ParkAndSwitch(const std::function<void()> &func) {
  preempt_disable();
  void *buf;
  thread_t *th = thread_create_with_buf(thread_internal::ThreadTrampoline, &buf,
                                        sizeof(std::function<void()>));
  if (unlikely(!th))
    BUG();
  new(buf) std::function<void()>(func);
  thread_park_and_switch(th);
}

inline void ParkAndSwitch(std::function<void()> &&func) {
  preempt_disable();
  void *buf;
  thread_t *th = thread_create_with_buf(thread_internal::ThreadTrampoline, &buf,
                                        sizeof(std::function<void()>));
  if (unlikely(!th))
    BUG();
  new(buf) std::function<void()>(std::move(func));
  thread_park_and_switch(th);
}

// A C++11 style thread class
class Thread {
 public:
  // boilerplate constructors.
  Thread() : join_data_(nullptr) {}
  DISALLOW_COPY_AND_ASSIGN(Thread);
  ~Thread();

  // Move support.
  Thread(Thread&& t) : join_data_(t.join_data_) {t.join_data_ = nullptr;}
  Thread& operator=(Thread&& t) {
    join_data_ = t.join_data_;
    t.join_data_ = nullptr;
    return *this;
  }

  // Spawns a thread by copying a std::function.
  Thread(const std::function<void()> &func, bool rr = false,
         uint32_t status = 0);

  // Spawns a thread by moving a std::function.
  Thread(std::function<void()> &&func, bool rr = false, uint32_t status = 0);

  // Waits for the thread to exit.
  void Join();

  // Detaches the thread, indicating it won't be joined in the future.
  void Detach();

 private:
  thread_internal::join_data *join_data_;
};
} // namespace rt
