#include "deref_scope.hpp"
#include "manager.hpp"

namespace far_memory {
Status expected_status = InScopeV0;

void DerefScope::mutator_wait_for_gc_cache() {
  FarMemManagerFactory::get()->mutator_wait_for_gc_cache();
}

} // namespace far_memory
