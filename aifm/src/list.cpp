extern "C" {
#include <base/assert.h>
}

#include "helpers.hpp"
#include "internal/ds_info.hpp"
#include "list.hpp"
#include "manager.hpp"

namespace far_memory {

GenericList::GenericList(const DerefScope &scope, const uint16_t kItemSize,
                         const uint16_t kNumNodesPerChunk, bool enable_merge,
                         bool customized_split)
    : kItemSize_(kItemSize), kNumNodesPerChunk_(kNumNodesPerChunk),
      kChunkListNodeSize_(sizeof(ChunkList::Node) + kItemSize),
      kChunkSize_(sizeof(ChunkListData) + sizeof(ChunkList::ListData) +
                  kNumNodesPerChunk * kChunkListNodeSize_),
      kInitMeta_(((~static_cast<decltype(kInitMeta_)>(0)) >>
                  (8 * sizeof(kInitMeta_) - kNumNodesPerChunk))),
      kMergeThresh_(
          static_cast<uint16_t>(kNumNodesPerChunk_ * kMergeThreshRatio)),
      kPrefetchNumNodes_(
          FarMemManagerFactory::get()->get_device()->get_prefetch_win_size() /
          kChunkSize_),
      enable_merge_(enable_merge), customized_split_(customized_split) {
  local_list_.push_back(LocalNode());
  local_list_.push_back(LocalNode());
  init_local_node(scope, &local_list_.front());
  init_local_node(scope, &local_list_.back());
  local_list_.front().cnt = kInvalidCnt;
  local_list_.back().cnt = kInvalidCnt;
}

void GenericList::init_local_node(const DerefScope &scope,
                                  LocalNode *local_node) {
  local_node->ptr.nullify();
  local_node->ptr =
      std::move(FarMemManagerFactory::get()->allocate_generic_unique_ptr(
          kVanillaPtrDSID, kChunkSize_));
  auto *chunk_list_data =
      static_cast<ChunkListData *>(local_node->ptr.deref_mut(scope));
  chunk_list_data->meta = kInitMeta_;
  auto list_data =
      reinterpret_cast<ChunkList::ListData *>(chunk_list_data->data);
  auto *chunk_list_ptr = &(local_node->chunk_list);
  struct ChunkListState state;
  state.list_data_ptr_addr =
      reinterpret_cast<uint64_t>(&(chunk_list_ptr->list_data_));
  state.kChunkListNodeSize = kChunkListNodeSize_;
  new (chunk_list_ptr) ChunkList(list_data, state);
  auto base_addr = reinterpret_cast<uint64_t>(chunk_list_data) +
                   static_cast<uint64_t>(sizeof(ChunkListData));
  auto head_addr = reinterpret_cast<uint64_t>(&(list_data->head));
  auto tail_addr = reinterpret_cast<uint64_t>(&(list_data->tail));
  chunk_list_ptr->init(ChunkNodePtr(0, head_addr - base_addr),
                       ChunkNodePtr(0, tail_addr - base_addr));
}

} // namespace far_memory
