#include "local_skiplist.hpp"

#include <cassert>
#include <cstring>

namespace far_memory {

GenericLocalSkiplist::GenericLocalSkiplist(uint32_t item_size,
                                           uint64_t data_size)
    : item_size_(item_size),
      slab_(static_cast<uint8_t *>(helpers::allocate_hugepage(data_size)),
            data_size) {
  negative_infinite_ = slab_.allocate(item_size);
  positive_infinite_ = slab_.allocate(item_size);

  head_ = reinterpret_cast<Entry *>(slab_.allocate(sizeof(Entry)));
  tail_ = reinterpret_cast<Entry *>(slab_.allocate(sizeof(Entry)));
  head_->key = negative_infinite_;
  tail_->key = positive_infinite_;
  head_->left = head_->up = head_->down = nullptr;
  tail_->right = tail_->up = tail_->down = nullptr;
  head_->right = tail_;
  tail_->left = head_;
  levels_ = 0;
}

FORCE_INLINE bool GenericLocalSkiplist::should_bubble_up() {
  return distribution_(generator_) == 0;
}

GenericLocalSkiplist::Entry *
GenericLocalSkiplist::_find_closest(const void *key, Entry **level_traces) {
  Entry *prev = nullptr;
  uint32_t level = 0;

  auto record_level_trace_fn = [&](Entry *trace) {
    if (level_traces) {
      level_traces[level++] = trace;
    }
  };

  for (auto cur = head_; cur;) {
    prev = cur;
    if ((!cur->right) || is_smaller_(key, cur->right->key)) {
      record_level_trace_fn(cur);
      cur = cur->down;
    } else {
      cur = cur->right;
    }
  }
  return prev;
}

void GenericLocalSkiplist::bubble_up(Entry *down_ptr, Entry **level_traces) {
  uint32_t level = 0;
  while (should_bubble_up() && level < kMaxLevels) {
    level++;
    auto new_ptr = reinterpret_cast<Entry *>(slab_.allocate(sizeof(Entry)));
    new_ptr->key = down_ptr->key;
    new_ptr->up = nullptr;
    new_ptr->down = down_ptr;
    down_ptr->up = new_ptr;
    down_ptr = new_ptr;

    if (level <= levels_) {
      auto level_trace_ptr = level_traces[levels_ - level];
      level_trace_ptr->right->left = new_ptr;
      new_ptr->right = level_trace_ptr->right;
      level_trace_ptr->right = new_ptr;
      new_ptr->left = level_trace_ptr;
    } else {
      levels_++;
      auto new_head = reinterpret_cast<Entry *>(slab_.allocate(sizeof(Entry)));
      auto new_tail = reinterpret_cast<Entry *>(slab_.allocate(sizeof(Entry)));
      new_head->key = negative_infinite_;
      new_tail->key = positive_infinite_;
      new_head->left = new_head->up = nullptr;
      new_tail->right = new_tail->up = nullptr;
      new_head->right = new_ptr;
      new_ptr->left = new_head;
      new_tail->left = new_ptr;
      new_ptr->right = new_tail;
      new_head->down = head_;
      head_->up = new_head;
      head_ = new_head;
      new_tail->down = tail_;
      tail_->up = new_tail;
      tail_ = new_tail;
    }
  }
}

bool GenericLocalSkiplist::insert(const void *key) {
  assert(is_greater_(key, negative_infinite_) &&
         is_smaller_(key, positive_infinite_));

  auto insert_key_fn = [&](Entry *ptr) {
    auto new_key_slab = slab_.allocate(item_size_);
    memcpy(new_key_slab, key, item_size_);
    ptr->key = new_key_slab;
  };

  Entry *level_traces[kMaxLevels];
  auto ptr = _find_closest(key, level_traces);
  if (is_equal_(key, ptr->key)) {
    return false;
  }
  auto new_ptr = reinterpret_cast<Entry *>(slab_.allocate(sizeof(Entry)));
  insert_key_fn(new_ptr);
  new_ptr->up = new_ptr->down = nullptr;
  ptr->right->left = new_ptr;
  new_ptr->right = ptr->right;
  ptr->right = new_ptr;
  new_ptr->left = ptr;

  bubble_up(new_ptr, level_traces);

  return true;
}

bool GenericLocalSkiplist::exist(const void *key) {
  assert(is_greater_(key, negative_infinite_) &&
         is_smaller_(key, positive_infinite_));

  auto ptr = _find_closest(key);
  return is_equal_(key, ptr->key);
}

void GenericLocalSkiplist::prune_empty_level(Entry *left_boundary,
                                             Entry *right_boundary) {
  if (left_boundary->down) {
    left_boundary->down->up = left_boundary->up;
  }
  if (left_boundary->up) {
    left_boundary->up->down = left_boundary->down;
  }
  if (right_boundary->down) {
    right_boundary->down->up = right_boundary->up;
  }
  if (right_boundary->up) {
    right_boundary->up->down = right_boundary->down;
  }
  slab_.free(reinterpret_cast<uint8_t *>(left_boundary), sizeof(Entry));
  slab_.free(reinterpret_cast<uint8_t *>(right_boundary), sizeof(Entry));
}

bool GenericLocalSkiplist::remove(const void *key) {
  assert(is_greater_(key, negative_infinite_) &&
         is_smaller_(key, positive_infinite_));

  auto ptr = _find_closest(key);
  if (is_equal_(key, ptr->key)) {
    slab_.free(reinterpret_cast<uint8_t *>(ptr->key), item_size_);
    do {
      ptr->left->right = ptr->right;
      ptr->right->left = ptr->left;
      if (unlikely(ptr->left->key == negative_infinite_ &&
                   ptr->right->key == positive_infinite_)) {
        prune_empty_level(ptr->left, ptr->right);
      }
      auto up_ptr = ptr->up;
      slab_.free(reinterpret_cast<uint8_t *>(ptr), sizeof(Entry));
      ptr = up_ptr;
    } while (ptr);
    return true;
  }
  return false;
}

} // namespace far_memory
