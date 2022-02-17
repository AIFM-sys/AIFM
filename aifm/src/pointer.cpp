#include "pointer.hpp"
#include "deref_scope.hpp"
#include "helpers.hpp"
#include "manager.hpp"

#include <cstdint>

namespace far_memory {

void FarMemPtrMeta::set_present(uint64_t object_addr) {
  static_assert(kSize == sizeof(uint64_t));
  auto obj = Object(object_addr);
  obj.set_ptr_addr(reinterpret_cast<uint64_t>(this));
  wmb();
  uint64_t new_metadata =
      ((object_addr + Object::kHeaderSize) << kObjectDataAddrBitPos) |
      ((kDirtyClear | kHotClear) + ((kHotThresh - 1) << (8 * kHotPos)));
  new_metadata |= (static_cast<uint64_t>(is_shared()) << kSharedBitPos);
  from_uint64_t(new_metadata);
}

void FarMemPtrMeta::mutator_copy(uint64_t new_local_object_addr) {
  constexpr auto obj_data_addr_mask = (1ULL << kObjectDataAddrBitPos) - 1;
  // Clear the evacuation bit.
  constexpr auto evacuation_mask = (~(1ULL << (8 * kEvacuationPos)));
  constexpr auto mask = (obj_data_addr_mask & evacuation_mask);
  auto masked_old_meta = (to_uint64_t() & mask);
  uint64_t new_metadata =
      (masked_old_meta | ((new_local_object_addr + Object::kHeaderSize)
                          << kObjectDataAddrBitPos));
  new_metadata |= (static_cast<uint64_t>(is_shared()) << kSharedBitPos);
  from_uint64_t(new_metadata);
}

void FarMemPtrMeta::gc_copy(uint64_t new_local_object_addr) {
  uint64_t old_metadata = to_uint64_t();
  assert((old_metadata & kPresentClear) == 0);
  auto new_local_object_data_addr = new_local_object_addr + Object::kHeaderSize;
  auto new_metadata = (new_local_object_data_addr << kObjectDataAddrBitPos) |
                      (kHotClear + ((kHotThresh - 1) << (8 * kHotPos))) |
                      (old_metadata & (0xFF << (8 * kPresentPos)));
  new_metadata |= (static_cast<uint64_t>(is_shared()) << kSharedBitPos);
  from_uint64_t(new_metadata);
}

void FarMemPtrMeta::gc_wb(uint8_t ds_id, uint16_t object_size,
                          uint64_t obj_id) {
  assert(obj_id < (1ULL << kObjectIDBitSize));
  auto new_metadata =
      (obj_id << kObjectIDBitPos) |
      (static_cast<uint64_t>(object_size) << kObjectSizeBitPos) |
      kPresentClear | ds_id;
  new_metadata |= (static_cast<uint64_t>(is_shared()) << kSharedBitPos);
  from_uint64_t(new_metadata);
}

void GenericFarMemPtr::swap_in(bool nt) {
  FarMemManagerFactory::get()->swap_in(nt, this);
}

bool GenericFarMemPtr::mutator_migrate_object() {
  auto *manager = FarMemManagerFactory::get();

  auto object = meta().object();
  rmb();
  if (unlikely(!meta().is_present())) {
    return false;
  }

  auto obj_id_len = object.get_obj_id_len();
  auto *obj_id = object.get_obj_id();
  FarMemManager::lock_object(obj_id_len, obj_id);
  auto guard = helpers::finally(
      [&]() { FarMemManager::unlock_object(obj_id_len, obj_id); });

  if (unlikely(!meta().is_present() || !meta().is_evacuation())) {
    return false;
  }

  bool nt = meta().is_nt();
  auto object_size = object.size();

  auto optional_new_local_object_addr =
      manager->allocate_local_object_nb(nt, object_size);
  if (!optional_new_local_object_addr) {
    return false;
  }
  auto new_local_object_addr = *optional_new_local_object_addr;
  if (auto copy_notifier = manager->copy_notifiers_[object.get_ds_id()]) {
    memcpy(reinterpret_cast<void *>(new_local_object_addr),
           reinterpret_cast<void *>(object.get_addr()), Object::kHeaderSize);
    auto dest_object = Object(new_local_object_addr);
    dest_object.set_obj_id(object.get_obj_id(), object.get_obj_id_len());
    copy_notifier(Object(new_local_object_addr), object);
  } else {
    memcpy(reinterpret_cast<void *>(new_local_object_addr),
           reinterpret_cast<void *>(object.get_addr()), object_size);
  }
  Region::atomic_inc_ref_cnt(new_local_object_addr, -1);

  if (!meta().is_shared()) {
    meta().mutator_copy(new_local_object_addr);
  } else {
    reinterpret_cast<GenericSharedPtr *>(this)->traverse(
        [=](GenericSharedPtr *ptr) {
          ptr->meta().mutator_copy(new_local_object_addr);
        });
  }
  object.free();
  return true;
}

void GenericFarMemPtr::_flush(bool obj_locked) {
restart:
  FarMemPtrMeta meta_snapshot = meta();
  if (unlikely(meta_snapshot.is_present() && meta_snapshot.is_dirty())) {
    auto obj_id_len = sizeof(uint64_t);
    auto obj = meta_snapshot.object();
    auto obj_id_ptr = obj.get_obj_id();

    auto guard = helpers::finally([&]() {
      if (!obj_locked) {
        FarMemManager::unlock_object(obj_id_len, obj_id_ptr);
      }
    });

    if (!obj_locked) {
      FarMemManager::lock_object(obj_id_len, obj_id_ptr);
      if (unlikely(meta() != meta_snapshot)) {
        goto restart;
      }
    }

    FarMemManagerFactory::get()->get_device()->write_object(
        obj.get_ds_id(), obj_id_len, obj_id_ptr, obj.get_data_len(),
        reinterpret_cast<const uint8_t *>(obj.get_data_addr()));
    if (!meta_snapshot.is_shared()) {
      meta().clear_dirty();
    } else {
      reinterpret_cast<GenericSharedPtr *>(this)->traverse(
          [=](GenericFarMemPtr *ptr) { ptr->meta().clear_dirty(); });
    }
  }
}

void GenericFarMemPtr::move(GenericFarMemPtr &other, uint64_t reset_value) {
retry:
  uint8_t other_obj_id_len = sizeof(uint64_t);
  const uint8_t *other_obj_id_ptr;
  uint64_t other_obj_id;
  FarMemPtrMeta other_meta_snapshot = other.meta();
  bool other_present = other_meta_snapshot.is_present();
  Object other_object;
  if (other_present) {
    other_object = other_meta_snapshot.object();
    other_obj_id_ptr = other_object.get_obj_id();
  } else {
    other_obj_id = other_meta_snapshot.get_object_id();
    other_obj_id_ptr = reinterpret_cast<const uint8_t *>(&other_obj_id);
  }
  FarMemManager::lock_object(other_obj_id_len, other_obj_id_ptr);
  auto guard = helpers::finally([&]() {
    FarMemManager::unlock_object(other_obj_id_len, other_obj_id_ptr);
  });

  if (unlikely(other.meta() != other_meta_snapshot)) {
    goto retry;
  }

  meta() = other.meta();
  wmb();
  if (other_present) {
    if (meta().is_shared()) {
      auto *other_ptr = reinterpret_cast<GenericSharedPtr *>(&other);
      auto *prev = other_ptr;
      other_ptr->traverse([&](GenericSharedPtr *p) { prev = p; });
      auto *cur = reinterpret_cast<GenericSharedPtr *>(this);
      prev->set_next_ptr(cur);
      auto next = (prev != other_ptr) ? other_ptr->next_ptr_ : this;
      cur->set_next_ptr(reinterpret_cast<GenericSharedPtr *>(next));
    }
    other_object.set_ptr_addr(reinterpret_cast<uint64_t>(this));
  }
  __builtin_memcpy(reinterpret_cast<uint64_t *>(&other.meta()), &reset_value,
                   sizeof(reset_value));
}

void GenericUniquePtr::_free() {
  assert(!meta().is_null());
  assert(meta().is_present());

  auto obj = object();
  auto obj_id_len = obj.get_obj_id_len();
  auto *obj_id = obj.get_obj_id();
  FarMemManager::lock_object(obj_id_len, obj_id);
  auto guard = helpers::finally(
      [&]() { FarMemManager::unlock_object(obj_id_len, obj_id); });

  object().free();
  meta().nullify();
}

void GenericUniquePtr::free(bool race) {
  if (!meta().is_present() && !race) {
    return;
  }
  auto pin_guard = pin</* Shared */ false>();
  _free();
}

void GenericUniquePtr::evacuate() {
restart:
  FarMemPtrMeta meta_snapshot = meta();
  if (meta_snapshot.is_present()) {
    auto obj = meta_snapshot.object();
    auto obj_id_len = obj.get_obj_id_len();
    auto *obj_id = obj.get_obj_id();

    FarMemManager::lock_object(obj_id_len, obj_id);
    auto guard = helpers::finally(
        [&]() { FarMemManager::unlock_object(obj_id_len, obj_id); });

    if (unlikely(meta() != meta_snapshot)) {
      goto restart;
    }
    _flush(/* obj_locked = */ true);
    auto ds_id = obj.get_ds_id();
    auto obj_size = obj.size();
    meta().gc_wb(ds_id, obj_size, *reinterpret_cast<const uint64_t *>(obj_id));
    obj.free();
  }
}

void GenericSharedPtr::_free() {
  assert(!meta().is_null());
  assert(meta().is_present());

  auto obj = object();
  auto obj_id_len = obj.get_obj_id_len();
  auto *obj_id = obj.get_obj_id();
  FarMemManager::lock_object(obj_id_len, obj_id);
  auto guard = helpers::finally(
      [&]() { FarMemManager::unlock_object(obj_id_len, obj_id); });
  if (next_ptr_ == this) {
    object().free();
  } else {
    auto *ptr = next_ptr_;
    while (ptr->next_ptr_ != this) {
      ptr = ptr->next_ptr_;
    }
    ptr->next_ptr_ = next_ptr_;
    if (unlikely(object().get_ptr_addr() == reinterpret_cast<uint64_t>(this))) {
      object().set_ptr_addr(reinterpret_cast<uint64_t>(next_ptr_));
    }
  }
  meta().nullify();
}

GenericSharedPtr &GenericSharedPtr::operator=(const GenericSharedPtr &other) {
  auto pin_guard = pin</* Shared */ true>();
  meta() = other.meta();
  next_ptr_ = other.next_ptr_;
  const_cast<GenericSharedPtr *>(&other)->next_ptr_ = this;
  return *this;
}

} // namespace far_memory
