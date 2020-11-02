extern "C" {}

#include "dataframe_vector.hpp"
#include "deref_scope.hpp"
#include "device.hpp"
#include "helpers.hpp"
#include "manager.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <unordered_set>
#include <vector>

using namespace far_memory;
using namespace std;

constexpr uint64_t kCacheSize = 512 * Region::kSize;
constexpr uint64_t kFarMemSize = (1ULL << 34); // 16 GB.
constexpr uint64_t kNumGCThreads = 12;
constexpr uint64_t kNumEntries = 256 << 20; // 256 million entries.
constexpr uint64_t kNumElementsPerScope = 1024;

namespace far_memory {
class FarMemTest {
private:
public:
  void do_work(FarMemManager *manager) {
    auto dataframe_vector = manager->allocate_dataframe_vector<long long>();

    for (uint64_t i = 0; i < kNumEntries; i++) {
      DerefScope scope;
      dataframe_vector.push_back(scope, static_cast<long long>(i));
    }

    for (uint64_t i = 0; i < kNumEntries; i++) {
      DerefScope scope;
      TEST_ASSERT(dataframe_vector.at(scope, i) == static_cast<long long>(i));
    }

    {
      DerefScope scope;
      TEST_ASSERT(dataframe_vector.front(scope) == 0);
      TEST_ASSERT(dataframe_vector.back(scope) == kNumEntries - 1);
    }

    {
      DerefScope scope;
      auto it = dataframe_vector.fbegin(scope);
      for (uint64_t i = 0; i < dataframe_vector.size(); i++) {
        if (unlikely(i % kNumElementsPerScope == 0)) {
          scope.exit();
          scope.enter();
          it.renew(scope);
        }
        *it = dataframe_vector.size() - i - 1;
        ++it;
      }
    }

    {
      DerefScope scope;
      auto it = dataframe_vector.cfbegin(scope);
      for (uint64_t i = 0; i < dataframe_vector.size(); i++) {
        if (unlikely(i % kNumElementsPerScope == 0)) {
          scope.exit();
          scope.enter();
          it.renew(scope);
        }
        TEST_ASSERT(*it ==
                    static_cast<long long>(dataframe_vector.size() - i - 1));
        ++it;
      }
    }

    TEST_ASSERT(!dataframe_vector.empty());
    TEST_ASSERT(dataframe_vector.size() == kNumEntries);
    for (uint64_t i = 0; i < kNumEntries; i++) {
      DerefScope scope;
      dataframe_vector.pop_back(scope);
      TEST_ASSERT(dataframe_vector.size() == kNumEntries - 1 - i);
    }
    TEST_ASSERT(dataframe_vector.empty());

    dataframe_vector.reserve(kNumEntries * 2);
    TEST_ASSERT(dataframe_vector.capacity() >= kNumEntries * 2);
    dataframe_vector.resize(kNumEntries * 2 + 1);
    TEST_ASSERT(dataframe_vector.capacity() >= kNumEntries * 2 + 1);
    TEST_ASSERT(dataframe_vector.size() == kNumEntries * 2 + 1);

    dataframe_vector.clear();
    for (uint64_t i = 0; i < kNumEntries / 2; i++) {
      DerefScope scope;
      dataframe_vector.push_back(scope, static_cast<long long>(i));
      dataframe_vector.push_back(scope, static_cast<long long>(i));
    }

    auto unique_vector = dataframe_vector.get_col_unique_values(manager);
    TEST_ASSERT(unique_vector.size() == kNumEntries / 2);
    std::unordered_set<long long> hashset;
    for (uint64_t i = 0; i < kNumEntries / 2; i++) {
      DerefScope scope;
      auto val = unique_vector.at(scope, i);
      TEST_ASSERT(val >= 0 && val < static_cast<long long>(kNumEntries / 2));
      auto insert_pair = hashset.insert(val);
      TEST_ASSERT(insert_pair.second == true);
    }

    auto index_vector =
        manager->allocate_dataframe_vector<unsigned long long>();
    constexpr auto slice_size = kNumEntries / 4;
    for (unsigned long long i = 0; i < slice_size; i++) {
      DerefScope scope;
      index_vector.push_back(scope, i);
    }

    auto slice = dataframe_vector.copy_data_by_idx(manager, index_vector);
    TEST_ASSERT(slice.size() == slice_size);

    for (unsigned long long i = 0; i < slice_size; i++) {
      DerefScope scope;
      auto idx = index_vector.at(scope, i);
      TEST_ASSERT(slice.at(scope, idx) == dataframe_vector.at(scope, idx));
    }

    TEST_ASSERT(slice.end() - slice.begin() ==
                static_cast<int64_t>(slice.size()));
    TEST_ASSERT(slice.begin() - slice.end() ==
                -static_cast<int64_t>(slice.size()));
    auto iter = slice.begin();
    iter += slice.size();
    TEST_ASSERT(iter == slice.end());
    iter--;
    TEST_ASSERT(iter != slice.end());

    unsigned long long cnt = 0;
    for (auto data : slice) {
      DerefScope scope;
      TEST_ASSERT(data == slice.at(scope, cnt++));
    }
    TEST_ASSERT(cnt == slice.size());

    auto slice_copy = manager->allocate_dataframe_vector<long long>();
    slice_copy = slice;
    TEST_ASSERT(slice_copy.size() == slice.size());
    for (unsigned long long i = 0; i < slice.size(); i++) {
      DerefScope scope;
      TEST_ASSERT(slice_copy.at(scope, i) == slice.at(scope, i));
    }

    auto small_vector = manager->allocate_dataframe_vector<short>();
    for (int i = std::numeric_limits<short>::max();
         i >= std::numeric_limits<short>::min(); i--) {
      DerefScope scope;
      small_vector.push_back(scope, static_cast<short>(i));
    }
    auto sorted_indices = small_vector.get_sorted_indices(manager, false);
    auto highest_rank =
        static_cast<uint64_t>(std::numeric_limits<short>::max()) -
        static_cast<uint64_t>(std::numeric_limits<short>::min());
    for (uint64_t i = 0; i < sorted_indices.size(); i++) {
      DerefScope scope;
      TEST_ASSERT(sorted_indices.at(scope, i) == highest_rank - i);
    }
    sorted_indices = small_vector.get_sorted_indices<false>(manager, false);
    for (uint64_t i = 0; i < sorted_indices.size(); i++) {
      DerefScope scope;
      TEST_ASSERT(sorted_indices.at(scope, i) == i);
    }

    {
      int key[] = {1, 1, 1, 3, 3, 4, 4, 5, 5, 5, 5};
      int data[] = {9, 2, 3, 2, 0, 1, 3, 4, 9, 8, 3};
      // Aggregate by max
      // {(1, 9), (3, 2), (4, 3), (5, 9)}
      // Aggregate by min
      // {(1, 2), (3, 0), (4, 1), (5, 3)}
      // Aggregate by median
      // {(1, 3), (3, 1), (4, 2), (5, 6)}
      auto key_vec = manager->allocate_dataframe_vector<int>();
      auto data_vec = manager->allocate_dataframe_vector<int>();
      {
        DerefScope scope;
        for (uint32_t i = 0; i < std::size(key); i++) {
          key_vec.push_back(scope, key[i]);
          data_vec.push_back(scope, data[i]);
        }
      }
      auto agg_max_vec = data_vec.aggregate_max(manager, key_vec);
      TEST_ASSERT(agg_max_vec.size() == 4);
      {
        DerefScope scope;
        TEST_ASSERT(agg_max_vec.at(scope, 0) == 9);
        TEST_ASSERT(agg_max_vec.at(scope, 1) == 2);
        TEST_ASSERT(agg_max_vec.at(scope, 2) == 3);
        TEST_ASSERT(agg_max_vec.at(scope, 3) == 9);
      }
      auto agg_min_vec = data_vec.aggregate_min(manager, key_vec);
      {
        DerefScope scope;
        TEST_ASSERT(agg_min_vec.size() == 4);
        TEST_ASSERT(agg_min_vec.at(scope, 0) == 2);
        TEST_ASSERT(agg_min_vec.at(scope, 1) == 0);
        TEST_ASSERT(agg_min_vec.at(scope, 2) == 1);
        TEST_ASSERT(agg_min_vec.at(scope, 3) == 3);
      }
      auto agg_median_vec = data_vec.aggregate_median(manager, key_vec);
      {
	DerefScope scope;
        TEST_ASSERT(agg_median_vec.size() == 4);
        TEST_ASSERT(agg_median_vec.at(scope, 0) == 3);
        TEST_ASSERT(agg_median_vec.at(scope, 1) == 1);
        TEST_ASSERT(agg_median_vec.at(scope, 2) == 2);
        TEST_ASSERT(agg_median_vec.at(scope, 3) == 6);
      }
    }

    {
      short data[] = {2, 5, 3, 7, 4, 6, 2, 6, 9, 0, -3, -5, -4, 3, -9};
      auto data_vec = manager->allocate_dataframe_vector<short>();
      for (uint32_t i = 0; i < std::size(data); i++) {
        DerefScope scope;
        data_vec.push_back(scope, data[i]);
      }
      auto sorted_indices = data_vec.get_sorted_indices(manager, false);
      auto shuffled_data_vec =
          data_vec.shuffle_data_by_idx(manager, sorted_indices);
      TEST_ASSERT(shuffled_data_vec.size() == std::size(data));
      std::sort(data, data + std::size(data));
      for (uint32_t i = 0; i < std::size(data); i++) {
        DerefScope scope;
        TEST_ASSERT(data[i] == shuffled_data_vec.at(scope, i));
      }
    }

    cout << "Passed" << endl;
  }
};
} // namespace far_memory

void _main(void *arg) {
  auto manager = std::unique_ptr<FarMemManager>(FarMemManagerFactory::build(
      kCacheSize, kNumGCThreads, new FakeDevice(kFarMemSize)));
  FarMemTest test;
  test.do_work(manager.get());
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
