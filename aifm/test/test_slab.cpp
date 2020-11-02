extern "C" {
#include <runtime/runtime.h>
}

#include "helpers.hpp"
#include "slab.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>

using namespace far_memory;

namespace far_memory {

constexpr static uint32_t kAllocateMemSize = (1 << 30);

class FarMemTest {
public:
  void run() {
    std::cout << "Running " << __FILE__ "..." << std::endl;

    preempt_enable();

    auto base_ptr =
        static_cast<uint8_t *>(helpers::allocate_hugepage(kAllocateMemSize));
    auto cur_ptr = base_ptr;
    auto slab = Slab(cur_ptr, kAllocateMemSize);

    cur_ptr += Slab::kReplenishChunkSize * Slab::kMinSlabClassSize;
    TEST_ASSERT(slab.allocate(Slab::kMinSlabClassSize - 1) ==
                cur_ptr - Slab::kMinSlabClassSize);
    TEST_ASSERT(slab.allocate(Slab::kMinSlabClassSize) ==
                cur_ptr - Slab::kMinSlabClassSize * 2);
    TEST_ASSERT(slab.slabs_[get_core_num()][0].size() ==
                Slab::kReplenishChunkSize - 2);

    cur_ptr += Slab::kReplenishChunkSize * Slab::kMaxSlabClassSize;
    TEST_ASSERT(slab.allocate(Slab::kMaxSlabClassSize / 2 + 1) ==
                cur_ptr - Slab::kMaxSlabClassSize);
    TEST_ASSERT(
        slab.slabs_[get_core_num()][helpers::bsr_32(Slab::kMaxSlabClassSize >>
                                                    Slab::kMinSlabClassShift)]
            .size() == Slab::kReplenishChunkSize - 1);

    TEST_ASSERT(slab.cur_ - slab.base_.get() == cur_ptr - base_ptr);

    slab.free(cur_ptr - Slab::kMaxSlabClassSize, Slab::kMaxSlabClassSize);
    TEST_ASSERT(slab.allocate(Slab::kMaxSlabClassSize) ==
                cur_ptr - Slab::kMaxSlabClassSize);

    preempt_disable();

    std::cout << "Passed" << std::endl;
  }
};
} // namespace far_memory

void _main(void *args) {
  FarMemTest ts;
  ts.run();
}

int main(int argc, char **argv) {
  int ret;

  if (argc < 2) {
    std::cerr << "usage: [cfg_file]" << std::endl;
    return -EINVAL;
  }

  ret = runtime_init(argv[1], _main, NULL);
  if (ret) {
    std::cerr << "failed to start runtime" << std::endl;
    return ret;
  }

  return 0;
}
