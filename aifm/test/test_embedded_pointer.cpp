extern "C" {
#include <runtime/runtime.h>
}

#include "deref_scope.hpp"
#include "device.hpp"
#include "manager.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

using namespace far_memory;
using namespace std;

constexpr uint64_t kCacheSize = 256 * Region::kSize;
constexpr uint64_t kFarMemSize = (1ULL << 33); // 8 GB.
constexpr uint64_t kNumGCThreads = 12;
constexpr uint64_t kEntrySize = 4096;
constexpr uint64_t kWorkingSetSize = (1ULL << 30); // 1 GB.
constexpr uint64_t kArrayDim1Len = 64;
constexpr uint64_t kArrayDim2Len = kWorkingSetSize / kArrayDim1Len / kEntrySize;
constexpr uint8_t kDSID = 128;

struct Data_t {
  uint8_t data[kEntrySize];
};

struct ArrayDim1 {
  UniquePtr<Data_t> ptrs[kArrayDim1Len];
};

template <int N> struct ArrayDim2 { UniquePtr<ArrayDim1> ptrs[N]; };

namespace far_memory {
class FarMemTest {
public:
  void do_work(FarMemManager *manager) {
    FarMemManager::EvacNotifier evac_notifier_fn =
        [&](Object obj, FarMemManager::WriteObjectFn write_obj_fn) -> bool {
      auto *array_data_dim1 =
          reinterpret_cast<ArrayDim1 *>(obj.get_data_addr());
      for (uint32_t i = 0; i < kArrayDim1Len; i++) {
        array_data_dim1->ptrs[i].evacuate();
      }
      write_obj_fn(sizeof(ArrayDim1));
      return false;
    };
    FarMemManager::CopyNotifier copy_notifier_fn = [&](Object dest,
                                                       Object src) -> void {
      auto *dest_array_data_dim1 =
          reinterpret_cast<ArrayDim1 *>(dest.get_data_addr());
      auto *src_array_data_dim1 =
          reinterpret_cast<ArrayDim1 *>(src.get_data_addr());
      for (uint32_t i = 0; i < kArrayDim1Len; i++) {
        dest_array_data_dim1->ptrs[i] = std::move(src_array_data_dim1->ptrs[i]);
      }
    };
    manager->register_eval_notifier(kDSID, evac_notifier_fn);
    manager->register_copy_notifier(kDSID, copy_notifier_fn);

    auto array_dim2 = std::make_unique<ArrayDim2<kArrayDim2Len>>();
    for (uint32_t i = 0; i < kArrayDim2Len; i++) {
      array_dim2->ptrs[i] = manager->allocate_unique_ptr<ArrayDim1>(kDSID);
      DerefScope scope;
      auto *array_data_dim1 = array_dim2->ptrs[i].deref_mut(scope);
      for (uint32_t j = 0; j < kArrayDim1Len; j++) {
        array_data_dim1->ptrs[j] = manager->allocate_unique_ptr<Data_t>();
        auto *data = array_data_dim1->ptrs[j].deref_mut(scope);
        for (uint32_t k = 0; k < kEntrySize; k++) {
          data->data[k] = i + j + k;
        }
      }
    }

    for (uint32_t i = 0; i < kArrayDim2Len; i++) {
      DerefScope scope;
      auto *array_data_dim1 = array_dim2->ptrs[i].deref(scope);
      for (uint32_t j = 0; j < kArrayDim1Len; j++) {
        auto *data =
            const_cast<UniquePtr<Data_t> *>(&(array_data_dim1->ptrs[j]))
                ->deref(scope);
        for (uint32_t k = 0; k < kEntrySize; k++) {
          if (data->data[k] != static_cast<uint8_t>(i + j + k)) {
            goto fail;
          }
        }
      }
    }

    cout << "Passed" << endl;
    return;

  fail:
    cout << "Failed" << endl;
    return;
  }
};
} // namespace far_memory

void _main(void *arg) {
  auto manager = std::unique_ptr<FarMemManager>(FarMemManagerFactory::build(
      kCacheSize, kNumGCThreads, new FakeDevice(kFarMemSize)));
  FarMemTest t;
  t.do_work(manager.get());
}

int main(int argc, char *argv[]) {
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
