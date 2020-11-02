extern "C" {
#include <runtime/preempt.h>
#include <runtime/thread.h>
}

#include "concurrent_hopscotch.hpp"
#include "deref_scope.hpp"
#include "hash.hpp"
#include "helpers.hpp"
#include "manager.hpp"

#include <cstring>

namespace far_memory {

GenericConcurrentHopscotch::GenericConcurrentHopscotch(
    uint8_t ds_id, uint32_t local_num_entries_shift,
    uint32_t remote_num_entries_shift, uint64_t remote_data_size)
    : kHashMask_((1 << local_num_entries_shift) - 1),
      kNumEntries_((1 << local_num_entries_shift) + kNeighborhood),
      ds_id_(ds_id) {
  // Check overflow.
  BUG_ON(((kHashMask_ + 1) >> local_num_entries_shift) != 1);

  // Allocate memory for buckets.
  auto size = kNumEntries_ * sizeof(BucketEntry);
  preempt_disable();
  buckets_mem_.reset(static_cast<uint8_t *>(helpers::allocate_hugepage(size)));
  buckets_ = new (buckets_mem_.get()) BucketEntry[kNumEntries_];
  preempt_enable();

  // Allocate memory for far memory pointers.
  size = kNumEntries_ * sizeof(GenericUniquePtr);

  // Initialize the remote-side hashtable.
  uint8_t params[sizeof(remote_num_entries_shift) + sizeof(remote_data_size)];
  __builtin_memcpy(params, &remote_num_entries_shift,
                   sizeof(remote_num_entries_shift));
  __builtin_memcpy(&params[sizeof(remote_num_entries_shift)], &remote_data_size,
                   sizeof(remote_data_size));
  FarMemManagerFactory::get()->construct(kHashTableDSType, ds_id,
                                         sizeof(params), params);

  // Register evac notifier.
  FarMemManager::EvacNotifier evac_notifier_fn =
      [&](Object obj, FarMemManager::WriteObjectFn write_obj_fn) -> bool {
    write_obj_fn(obj.get_data_len() - sizeof(EvacNotifierMeta));
    this->evac_notifier(obj);
    return true;
  };
  FarMemManagerFactory::get()->register_eval_notifier(ds_id, evac_notifier_fn);
}

GenericConcurrentHopscotch::~GenericConcurrentHopscotch() {
  // Free local data.
  for (uint32_t i = 0; i < kNumEntries_; i++) {
    auto &ptr = buckets_[i].ptr;
    DerefScope scope;
    if (ptr.deref(scope)) {
      ptr.free();
    }
  }
  // Free remote data.
  FarMemManagerFactory::get()->destruct(ds_id_);
}

void GenericConcurrentHopscotch::do_evac_notifier(EvacNotifierMeta meta) {
  auto *bucket =
      reinterpret_cast<BucketEntry *>(static_cast<uint64_t>(meta.anchor_addr));
  auto *entry = bucket + meta.offset;
  entry->ptr.nullify();

  if (likely(bucket->spin.TryLock())) {
    auto guard = helpers::finally([&]() { bucket->spin.Unlock(); });
    assert(bucket->bitmap & (1 << meta.offset));
    bucket->bitmap ^= (1 << meta.offset);
  } else {
    preempt_disable();
    BUG_ON(!evac_notifier_stash_.push_back(meta));
    preempt_enable();
  }
}

void GenericConcurrentHopscotch::forward_get(uint8_t key_len,
                                             const uint8_t *key,
                                             uint16_t *val_len, uint8_t *val) {
  // Cannot find the key locally, so forward the request to the remote agent.
  FarMemManagerFactory::get()->read_object(ds_id_, key_len, key, val_len, val);
  if (*val_len) {
    _put(key_len, key, *val_len, val, /* swap_in = */ true);
  }
}

FORCE_INLINE void *deref(GenericUniquePtr &ptr, bool mut) {
  if (mut) {
    return ptr._deref<true, false>();
  } else {
    return ptr._deref<false, false>();
  }
}

bool GenericConcurrentHopscotch::_put(uint8_t key_len, const uint8_t *key,
                                      uint16_t val_len, const uint8_t *val,
                                      bool swap_in) {
  uint32_t hash = hash_32(static_cast<const void *>(key), key_len);
  uint32_t bucket_idx = hash & kHashMask_;
retry:
  auto *bucket = &(buckets_[bucket_idx]);
  auto orig_bucket_idx = bucket_idx;

  while (unlikely(!bucket->spin.TryLockWp())) {
    thread_yield();
  }
  auto bucket_lock_guard = helpers::finally([&]() { bucket->spin.UnlockWp(); });

  uint32_t bitmap = load_acquire(&(bucket->bitmap));
  while (bitmap) {
    auto offset = helpers::bsf_32(bitmap);
    auto *bucket = &buckets_[bucket_idx];
    auto *entry = bucket + offset;
    auto &ptr = entry->ptr;
#ifdef HASHTABLE_EXCLUSIVE
    auto *obj_val_ptr = ptr._deref<true, false>();
#else
    auto *obj_val_ptr = deref(ptr, !swap_in);
#endif
    if (unlikely(!obj_val_ptr)) {
      bucket_lock_guard.reset();
      process_evac_notifier_stash();
      thread_yield();
      goto retry;
    }

    auto obj =
        Object(reinterpret_cast<uint64_t>(obj_val_ptr) - Object::kHeaderSize);
    if (obj.get_obj_id_len() == key_len) {
      auto obj_data_len = obj.get_data_len();
      if (strncmp(reinterpret_cast<const char *>(obj_val_ptr) + obj_data_len,
                  reinterpret_cast<const char *>(key), key_len) == 0) {
        if (unlikely(obj_data_len != val_len + sizeof(EvacNotifierMeta))) {
          auto new_data_size = val_len + sizeof(EvacNotifierMeta);
          if (!FarMemManagerFactory::get()->reallocate_generic_unique_ptr_nb(
                  *static_cast<DerefScope *>(nullptr), &ptr, new_data_size,
                  val)) {
            bucket_lock_guard.reset();
            FarMemManagerFactory::get()->mutator_wait_for_gc_cache();
            goto retry;
          }
          auto new_obj_val_ptr = ptr._deref<true, false>();
#ifndef HASHTABLE_EXCLUSIVE
          if (swap_in) {
            ptr.meta().clear_dirty();
          }
#endif
          assert(new_obj_val_ptr);
          auto new_meta = reinterpret_cast<EvacNotifierMeta *>(
              reinterpret_cast<uint64_t>(new_obj_val_ptr) + val_len);
          *new_meta = {.anchor_addr = reinterpret_cast<uint64_t>(bucket),
                       .offset = static_cast<uint8_t>(offset)};
        } else {
          memcpy(obj_val_ptr, val, val_len);
        }
        return true;
      }
    }
    bitmap ^= (1 << offset);
  }

  // The key does not exist. Use linear probing to find the first empty slot.
  while (bucket_idx < kNumEntries_) {
    auto *entry = &buckets_[bucket_idx];
    if (__sync_bool_compare_and_swap(reinterpret_cast<uint64_t *>(&entry->ptr),
                                     FarMemPtrMeta::kNull,
                                     BucketEntry::kBusyPtr)) {
      break;
    }
    bucket_idx++;
  }

  if (very_unlikely(bucket_idx == kNumEntries_)) {
    // The bucket is full and we need to resize the hash table.
    // For now, we just throw out an error.
    BUG();
  }

  uint32_t distance_to_orig_bucket;
  // Now keep moving the empty slot until it becomes neighbors.
  while ((distance_to_orig_bucket = bucket_idx - orig_bucket_idx) >=
         kNeighborhood) {
    // Try to see if we can move things backward.
    uint32_t distance;
    for (distance = kNeighborhood - 1; distance > 0; distance--) {
      auto idx = bucket_idx - distance;
      auto *anchor_entry = &(buckets_[idx]);
      if (!anchor_entry->bitmap) {
        continue;
      }

      // Lock and recheck bitmap.
      while (unlikely(!anchor_entry->spin.TryLockWp())) {
        thread_yield();
      }
      auto lock_guard =
          helpers::finally([&]() { anchor_entry->spin.UnlockWp(); });
      auto bitmap = load_acquire(&(anchor_entry->bitmap));
      if (unlikely(!bitmap)) {
        continue;
      }

      // Get the offset of the first entry within the bucket.
      auto offset = helpers::bsf_32(bitmap);
      if (idx + offset >= bucket_idx) {
        continue;
      }

      // Swap entry [closest_bucket + offset] and [bucket_idx]
      auto *from_entry = &buckets_[idx + offset];
      auto &from_entry_ptr = from_entry->ptr;
      auto *from_obj_val_ptr = from_entry_ptr._deref<false, false>();
      auto *to_entry = &buckets_[bucket_idx];
      if (unlikely(!from_obj_val_ptr)) {
        bucket_lock_guard.reset();
        to_entry->ptr.nullify();
        process_evac_notifier_stash();
        thread_yield();
        goto retry;
      }

      auto from_obj = Object(reinterpret_cast<uint64_t>(from_obj_val_ptr) -
                             Object::kHeaderSize);
      auto *from_meta = reinterpret_cast<EvacNotifierMeta *>(
          const_cast<uint8_t *>(from_obj.get_obj_id()) -
          sizeof(EvacNotifierMeta));

      from_meta->offset = to_entry - anchor_entry;
      assert((anchor_entry->bitmap & (1 << distance)) == 0);
      anchor_entry->bitmap |= (1 << distance);
      anchor_entry->timestamp++;
      to_entry->ptr.move(from_entry->ptr, BucketEntry::kBusyPtr);
      assert(anchor_entry->bitmap & (1 << offset));
      anchor_entry->bitmap ^= (1 << offset);

      // Jump backward.
      bucket_idx = idx + offset;
      break;
    }

    // The bucket is full and we need to resize the hash table.
    // For now, we just throw out an error.
    if (very_unlikely(!distance)) {
      BUG();
    }
  }

  // Allocate memory.
  auto *final_entry = &buckets_[bucket_idx];
  auto *ptr = &(final_entry->ptr);
  if (!FarMemManagerFactory::get()->allocate_generic_unique_ptr_nb(
          ptr, ds_id_, sizeof(EvacNotifierMeta) + val_len, key_len, key)) {
    bucket_lock_guard.reset();
    FarMemManagerFactory::get()->mutator_wait_for_gc_cache();
    goto retry;
  }
  auto *val_ptr = ptr->_deref<true, false>();
#ifndef HASHTABLE_EXCLUSIVE
  if (swap_in) {
    ptr->meta().clear_dirty();
  }
#endif
  assert(val_ptr);
  auto *meta = reinterpret_cast<EvacNotifierMeta *>(
      reinterpret_cast<uint64_t>(val_ptr) + val_len);

  // Write object.
  *meta = {.anchor_addr = reinterpret_cast<uint64_t>(bucket),
           .offset = static_cast<uint8_t>(final_entry - bucket)};
  memcpy(val_ptr, val, val_len);
  wmb();

  // Update the bitmap of the final bucket.
  assert((bucket->bitmap & (1 << distance_to_orig_bucket)) == 0);
  bucket->bitmap |= (1 << distance_to_orig_bucket);

  bucket_lock_guard.reset();

  // Ensure there's no copy at remote. Ideally we can make this happen
  // asynchronously and check completion before returning to client.
  if (!swap_in) {
    FarMemManagerFactory::get()->remove_object(ds_id_, key_len, key);
  }
  return false;
}

bool GenericConcurrentHopscotch::_remove(uint8_t key_len, const uint8_t *key) {
  uint32_t hash = hash_32(reinterpret_cast<const void *>(key), key_len);
  uint32_t bucket_idx = hash & kHashMask_;
  auto *bucket = &(buckets_[bucket_idx]);
  bool removed = false;

retry:
  while (unlikely(!bucket->spin.TryLockWp())) {
    thread_yield();
  }
  auto spin_guard = helpers::finally([&]() { bucket->spin.UnlockWp(); });

  uint32_t bitmap = load_acquire(&(bucket->bitmap));
  while (bitmap) {
    auto offset = helpers::bsf_32(bitmap);
    auto *entry = &buckets_[bucket_idx + offset];
    auto &ptr = entry->ptr;
    auto *obj_val_ptr = ptr._deref<false, false>();
    if (unlikely(!obj_val_ptr)) {
      spin_guard.reset();
      process_evac_notifier_stash();
      thread_yield();
      goto retry;
    }

    auto obj =
        Object(reinterpret_cast<uint64_t>(obj_val_ptr) - Object::kHeaderSize);
    if (obj.get_obj_id_len() == key_len) {
      auto obj_data_len = obj.get_data_len();
      if (strncmp(reinterpret_cast<const char *>(obj_val_ptr) + obj_data_len,
                  reinterpret_cast<const char *>(key), key_len) == 0) {
        assert(bucket->bitmap & (1 << offset));
        bucket->bitmap ^= (1 << offset);
        ptr.free(/* race = */ true);
#ifdef HASHTABLE_EXCLUSIVE
        return true;
#else
        removed = true;
        break;
#endif
      }
    }
    bitmap ^= (1 << offset);
  }
  spin_guard.reset();

  // Forward the request to the remote agent.
  return FarMemManagerFactory::get()->remove_object(ds_id_, key_len, key) ||
         removed;
}

} // namespace far_memory
