#pragma once

extern "C" {
#include <runtime/thread.h>
}

#include "helpers.hpp"

namespace far_memory {

enum Status { OutofScope = 0, InScopeV0, InScopeV1, GC };
static_assert(GC == GC_STATUS); // GC_STATUS is defined and used by Shenango
                                // runtime for performance accounting.

extern bool almost_empty;
extern bool gc_master_active;
extern Status expected_status;

class DerefScope {
private:
  NOT_COPYABLE(DerefScope);
  NOT_MOVEABLE(DerefScope);
  static void mutator_wait_for_gc_cache();
  static int32_t get_num_threads(Status status);
  static bool is_status_expected();

  friend class FarMemManager;
  friend class GenericConcurrentHopscotch;

public:
  DerefScope();
  ~DerefScope();
  static void enter();
  static void exit();
  static void renew();
  static bool is_in_deref_scope();
  static Status flip_status(Status status);
};

} // namespace far_memory

#include "internal/deref_scope.ipp"
