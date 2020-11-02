#pragma once

#include "deref_scope.hpp"
#include "local_list.hpp"
#include "pointer.hpp"

#include <cassert>
#include <cstdint>

namespace far_memory {

class GenericList {
private:
  struct ChunkListData {
    uint64_t meta;
    uint8_t data[0];
  };

#pragma pack(push, 1)
  struct ChunkNodePtr {
    uint8_t idx;
    uint16_t addr_offset;

    ChunkNodePtr();
    ChunkNodePtr(const ChunkNodePtr &o);
    constexpr ChunkNodePtr(uint8_t _idx, uint16_t _addr_offset);
    bool operator==(const ChunkNodePtr &o) const;
    bool operator!=(const ChunkNodePtr &o) const;
  };
#pragma pack(pop)

  struct ChunkListState {
    uint64_t list_data_ptr_addr : 48;
    uint16_t kChunkListNodeSize : 16;
  };

  static GenericLocalListNode<ChunkNodePtr> &
  deref_node_ptr(ChunkNodePtr ptr, ChunkListState state);
  static ChunkNodePtr allocate_node(ChunkListState state);
  static void free_node(ChunkNodePtr ptr, ChunkListState state);

  constexpr static auto kDerefNodePtrFn =
      [](ChunkNodePtr ptr,
         ChunkListState state) -> GenericLocalListNode<ChunkNodePtr> & {
    return deref_node_ptr(ptr, state);
  };
  constexpr static auto kAllocateNodeFn =
      [](ChunkListState state) -> ChunkNodePtr { return allocate_node(state); };
  constexpr static auto kFreeNodeFn =
      [](ChunkNodePtr ptr, ChunkListState state) { free_node(ptr, state); };
  using ChunkList =
      GenericLocalList<ChunkNodePtr, decltype(kDerefNodePtrFn),
                       decltype(kAllocateNodeFn), decltype(kFreeNodeFn)>;

#pragma pack(push, 1)
  struct LocalNode {
    GenericUniquePtr ptr;
    ChunkList chunk_list;
    uint8_t cnt = 0;
    bool swapping_in = false;
    uint8_t paddings[6];

    bool is_invalid() const;
  };
#pragma pack(pop)
  // Far-mem pointer should never cross the cacheline boundary.
  static_assert(sizeof(LocalNode) == 32);

  template <bool Reverse> class GenericIteratorImpl {
  private:
    uint8_t *insert(const DerefScope &scope);
    GenericIteratorImpl erase(const DerefScope &scope, uint8_t **data_ptr);

    LocalList<LocalNode>::IteratorImpl<Reverse> local_iter_;
    ChunkList::template IteratorImpl<Reverse> chunk_iter_;
    GenericList *list_;
    friend class GenericList;

  public:
    GenericIteratorImpl(
        const DerefScope &scope,
        const LocalList<LocalNode>::IteratorImpl<Reverse> &local_iter,
        GenericList *list);
    GenericIteratorImpl(const GenericIteratorImpl<Reverse> &o);
    GenericIteratorImpl &operator=(const GenericIteratorImpl<Reverse> &o);
    void inc(const DerefScope &scope);
    void dec(const DerefScope &scope);
    bool operator==(const GenericIteratorImpl &o);
    bool operator!=(const GenericIteratorImpl &o);
    const uint8_t *deref(const DerefScope &scope);
    uint8_t *deref_mut(const DerefScope &scope);
  };

  using GenericIterator = GenericIteratorImpl</* Reverse = */ false>;
  using ReverseGenericIterator = GenericIteratorImpl</* Reverse = */ true>;

  constexpr static uint16_t kMinNumNodesPerChunk = 8;
  constexpr static uint16_t kMaxNumNodesPerChunk = 64;
  constexpr static uint16_t kInvalidCnt = kMaxNumNodesPerChunk + 1;
  constexpr static uint16_t kDefaultChunkSize = 4096;
  constexpr static double kMergeThreshRatio = 0.75;

  const uint16_t kItemSize_;
  const uint16_t kNumNodesPerChunk_;
  const uint16_t kChunkListNodeSize_;
  const uint16_t kChunkSize_;
  const uint64_t kInitMeta_;
  const uint16_t kMergeThresh_;
  const uint32_t kPrefetchNumNodes_;
  LocalList<LocalNode> local_list_;
  uint64_t size_ = 0;
  bool enable_merge_;
  bool customized_split_; // Customized for Queue and Stack.
  bool enable_prefetch_ = false;
  bool prefetch_reversed_ = true;
  LocalList<LocalNode>::Iterator prefetch_iter_;

  template <typename T> friend class List;
  friend class FarMemTest;

  GenericList(const DerefScope &scope, const uint16_t kItemSize,
              const uint16_t kNumNodesPerChunk, bool enable_merge,
              bool customized_split = false);
  void init_local_node(const DerefScope &scope, LocalNode *local_node);
  template <bool Reverse>
  LocalList<LocalNode>::IteratorImpl<Reverse>
  add_local_list_node(const DerefScope &scope,
                      const LocalList<LocalNode>::IteratorImpl<Reverse> &iter);
  template <bool Reverse>
  GenericList::GenericIteratorImpl<Reverse>
  remove_local_list_node(const DerefScope &scope,
                         const GenericIteratorImpl<Reverse> &iter);
  template <bool Reverse>
  GenericIteratorImpl<Reverse>
  split_local_list_node(const DerefScope &scope,
                        const GenericIteratorImpl<Reverse> &iter);
  template <bool Reverse>
  void merge_local_list_node(
      const DerefScope &scope,
      const LocalList<LocalNode>::IteratorImpl<Reverse> &local_iter,
      const LocalList<LocalNode>::IteratorImpl<Reverse> &next_local_iter);

  template <bool Mut>
  static void update_chunk_list_addr(const DerefScope &scope,
                                     LocalNode *local_node);

  template <bool Reverse>
  void
  prefetch_fsm(const LocalList<LocalNode>::IteratorImpl<Reverse> &local_iter);
  void prefetch_once();
  void do_prefetch(LocalNode *local_node);

public:
  GenericIterator begin(const DerefScope &scope) const;
  GenericIterator end(const DerefScope &scope) const;
  ReverseGenericIterator rbegin(const DerefScope &scope) const;
  ReverseGenericIterator rend(const DerefScope &scope) const;
  const uint8_t *cfront(const DerefScope &scope) const;
  const uint8_t *cback(const DerefScope &scope) const;
  uint8_t *front(const DerefScope &scope) const;
  uint8_t *back(const DerefScope &scope) const;
  uint64_t size() const;
  bool empty() const;
  uint8_t *new_front(const DerefScope &scope);
  uint8_t *pop_front(const DerefScope &scope);
  uint8_t *new_back(const DerefScope &scope);
  uint8_t *pop_back(const DerefScope &scope);
  template <bool Reverse>
  uint8_t *insert(const DerefScope &scope, GenericIteratorImpl<Reverse> *iter);
  template <bool Reverse>
  GenericIteratorImpl<Reverse> erase(const DerefScope &scope,
                                     const GenericIteratorImpl<Reverse> &iter,
                                     uint8_t **data_ptr);
};

template <typename T> class List : public GenericList {
private:
  constexpr static uint16_t kNumNodesPerChunk =
      std::max(static_cast<uint16_t>(kMinNumNodesPerChunk),
               std::min(static_cast<uint16_t>(kMaxNumNodesPerChunk),
                        static_cast<uint16_t>(
                            (kDefaultChunkSize - sizeof(ChunkListData) -
                             sizeof(ChunkList::ListData)) /
                            (sizeof(T) + sizeof(ChunkList::Node)))));

  static_assert(kNumNodesPerChunk <= 8 * sizeof(ChunkListData::meta));

  template <bool Reverse>
  class IteratorImpl : public GenericIteratorImpl<Reverse> {
  private:
    IteratorImpl(const GenericIteratorImpl<Reverse> &generic_iter);
    friend class List;

  public:
    IteratorImpl(const IteratorImpl &o);
    IteratorImpl &operator=(const IteratorImpl &o);
    const T &deref(const DerefScope &scope);
    T &deref_mut(const DerefScope &scope);
  };

  using Iterator = IteratorImpl</* Reverse = */ false>;
  using ReverseIterator = IteratorImpl</* Reverse = */ true>;

  List(const DerefScope &scope, bool enable_merge,
       bool customized_split = false);

  template <typename D> friend class Queue;
  template <typename D> friend class Stack;
  friend class FarMemManager;
  friend class FarMemTest;

public:
  ~List();
  Iterator begin(const DerefScope &scope) const;
  Iterator end(const DerefScope &scope) const;
  ReverseIterator rbegin(const DerefScope &scope) const;
  ReverseIterator rend(const DerefScope &scope) const;
  const T &cfront(const DerefScope &scope) const;
  const T &cback(const DerefScope &scope) const;
  T &front(const DerefScope &scope) const;
  T &back(const DerefScope &scope) const;
  void push_front(const DerefScope &scope, const T &data);
  void push_back(const DerefScope &scope, const T &data);
  void pop_front(const DerefScope &scope);
  void pop_back(const DerefScope &scope);
  template <bool Reverse>
  void insert(const DerefScope &scope, IteratorImpl<Reverse> *iter,
              const T &data);
  template <bool Reverse>
  IteratorImpl<Reverse> erase(const DerefScope &scope,
                              const IteratorImpl<Reverse> &iter);
};

} // namespace far_memory

#include "internal/list.ipp"
