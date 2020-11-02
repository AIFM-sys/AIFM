#pragma once

#include "helpers.hpp"
#include "region.hpp"

#include <type_traits>

namespace far_memory {

FORCE_INLINE FarMemPtrMeta::FarMemPtrMeta() {
  // GC design assumes the pointer is always aligned so that R/W operations
  // will be atomic.
  assert(reinterpret_cast<uint64_t>(this) % sizeof(FarMemPtrMeta) == 0);
  nullify();
}

FORCE_INLINE FarMemPtrMeta::FarMemPtrMeta(const FarMemPtrMeta &other) {
  assert(reinterpret_cast<uint64_t>(this) % sizeof(FarMemPtrMeta) == 0);
  static_assert(kSize == sizeof(uint64_t));
  *reinterpret_cast<uint64_t *>(metadata_) =
      *reinterpret_cast<const uint64_t *>(other.metadata_);
}

FORCE_INLINE
FarMemPtrMeta::FarMemPtrMeta(bool shared, uint64_t object_addr) {
  assert(reinterpret_cast<uint64_t>(this) % sizeof(FarMemPtrMeta) == 0);
  memset(this, 0, sizeof(*this));
  init(shared, object_addr);
}

FORCE_INLINE
void FarMemPtrMeta::init(bool shared, uint64_t object_addr) {
  if (shared) {
    set_shared();
  }
  set_present(object_addr);
  set_dirty();
}

FORCE_INLINE bool FarMemPtrMeta::is_present() const {
  return !((ACCESS_ONCE(*reinterpret_cast<const uint16_t *>(metadata_))) &
           kPresentClear);
}

FORCE_INLINE bool FarMemPtrMeta::is_null() const {
  return (to_uint64_t() & kNullMask) == kNull;
}

FORCE_INLINE void FarMemPtrMeta::nullify() { from_uint64_t(kNull); }

FORCE_INLINE uint64_t FarMemPtrMeta::get_object_data_addr() const {
  return to_uint64_t() >> kObjectDataAddrBitPos;
}

FORCE_INLINE uint64_t FarMemPtrMeta::get_object_addr() const {
  return get_object_data_addr() - Object::kHeaderSize;
}

FORCE_INLINE uint16_t FarMemPtrMeta::get_object_size() const {
  assert(!is_present());
  return to_uint64_t() >> kObjectSizeBitPos;
}

FORCE_INLINE Object FarMemPtrMeta::object() {
  return Object(get_object_addr());
}

FORCE_INLINE FarMemPtrMeta *FarMemPtrMeta::from_object(const Object &object) {
  return reinterpret_cast<FarMemPtrMeta *>(object.get_ptr_addr());
}

FORCE_INLINE uint64_t FarMemPtrMeta::get_object_id() const {
  return to_uint64_t() >> kObjectIDBitPos;
}

FORCE_INLINE bool FarMemPtrMeta::is_dirty() const {
  return !((*reinterpret_cast<const uint16_t *>(metadata_)) & kDirtyClear);
}

FORCE_INLINE void FarMemPtrMeta::set_dirty() {
  *reinterpret_cast<uint16_t *>(metadata_) &= (~kDirtyClear);
}

FORCE_INLINE void FarMemPtrMeta::clear_dirty() {
  *reinterpret_cast<uint16_t *>(metadata_) |= kDirtyClear;
}

FORCE_INLINE bool FarMemPtrMeta::is_hot() const {
  return !((*reinterpret_cast<const uint16_t *>(metadata_)) & kHotClear);
}

FORCE_INLINE void FarMemPtrMeta::clear_hot() {
  metadata_[kHotPos] = (kHotClear >> (8 * kHotPos)) + (kHotThresh - 1);
}

FORCE_INLINE bool FarMemPtrMeta::is_nt() const {
  auto obj_data_addr = get_object_data_addr();
  if (unlikely(!obj_data_addr)) {
    // The pointer has being freed.
    return false;
  }
  return Region::is_nt(obj_data_addr & (~(Region::kSize - 1)));
}

FORCE_INLINE void FarMemPtrMeta::set_hot() {
  *reinterpret_cast<uint16_t *>(metadata_) &= (~kHotClear);
}

FORCE_INLINE uint64_t FarMemPtrMeta::to_uint64_t() const {
  return ACCESS_ONCE(*reinterpret_cast<const uint64_t *>(metadata_));
}

FORCE_INLINE void FarMemPtrMeta::from_uint64_t(uint64_t val) {
  ACCESS_ONCE(*reinterpret_cast<uint64_t *>(metadata_)) = val;
}

FORCE_INLINE void FarMemPtrMeta::set_evacuation() {
  assert(is_present());
  metadata_[kEvacuationPos] |= 1;
}

FORCE_INLINE bool FarMemPtrMeta::is_evacuation() const {
  return metadata_[kEvacuationPos] & 1;
}

FORCE_INLINE bool FarMemPtrMeta::is_shared() const {
  return to_uint64_t() & (1 << kSharedBitPos);
}

FORCE_INLINE void FarMemPtrMeta::set_shared() {
  from_uint64_t(to_uint64_t() | (1 << kSharedBitPos));
}

FORCE_INLINE GenericFarMemPtr::GenericFarMemPtr() {}

FORCE_INLINE GenericFarMemPtr::GenericFarMemPtr(bool shared,
                                                uint64_t object_addr)
    : meta_(shared, object_addr) {}

FORCE_INLINE void GenericFarMemPtr::init(bool shared, uint64_t object_addr) {
  meta_.init(shared, object_addr);
}

FORCE_INLINE Object GenericFarMemPtr::object() { return meta_.object(); }

FORCE_INLINE FarMemPtrMeta &GenericFarMemPtr::meta() { return meta_; }

FORCE_INLINE const FarMemPtrMeta &GenericFarMemPtr::meta() const {
  return meta_;
}

FORCE_INLINE void GenericFarMemPtr::nullify() { meta_.nullify(); }

FORCE_INLINE bool GenericFarMemPtr::is_null() const { return meta_.is_null(); }

FORCE_INLINE bool FarMemPtrMeta::operator==(const FarMemPtrMeta &other) const {
  return to_uint64_t() == other.to_uint64_t();
}

FORCE_INLINE bool FarMemPtrMeta::operator!=(const FarMemPtrMeta &other) const {
  return !(operator==(other));
}

FORCE_INLINE uint8_t FarMemPtrMeta::get_ds_id() const {
  assert(!is_present());
  return metadata_[kDSIDPos];
}

template <bool Mut, bool Nt, bool Shared>
FORCE_INLINE void *GenericFarMemPtr::_deref() {
retry:
  // 1) movq.
  auto metadata = meta().to_uint64_t();
  auto exceptions = (FarMemPtrMeta::kHotClear | FarMemPtrMeta::kPresentClear |
                     FarMemPtrMeta::kEvacuationSet);
  if constexpr (Mut) {
    exceptions |= FarMemPtrMeta::kDirtyClear;
  }
  // 2) test. 3) jne. They got macro-fused into a single uop.
  if (very_unlikely(metadata & exceptions)) {
    // Slow path.
    if (very_unlikely(metadata & (FarMemPtrMeta::kPresentClear |
                                  FarMemPtrMeta::kEvacuationSet))) {
      if (metadata & FarMemPtrMeta::kPresentClear) {
        if (meta().is_null()) {
          // In this case, _deref() returns nullptr.
          return nullptr;
        }
        swap_in(Nt);
        // Just swapped in, need to update metadata (for the obj data addr).
        metadata = meta().to_uint64_t();
      } else {
        if (!mutator_migrate_object()) {
          // GC or another thread wins the race. They may still need a while to
          // finish migrating the object. Yielding itself rather than busy
          // retrying now.
          thread_yield();
        }
      }
      goto retry;
    }
    if constexpr (Mut) {
      // set P and D.
      if constexpr (!Shared) {
        __asm__("movb $0, %0"
                : "=m"(meta().metadata_[FarMemPtrMeta::kPresentPos]));
      } else {
        __asm__("movb $2, %0"
                : "=m"(meta().metadata_[FarMemPtrMeta::kPresentPos]));
      }
    }
    meta().metadata_[FarMemPtrMeta::kHotPos]--;
  }

  // 4) shrq.
  return reinterpret_cast<void *>(metadata >>
                                  FarMemPtrMeta::kObjectDataAddrBitPos);
}

template <bool Shared>
FORCE_INLINE auto GenericFarMemPtr::pin(void **pinned_raw_ptr) {
  bool in_scope = DerefScope::is_in_deref_scope();
  if (!in_scope) {
    DerefScope::enter();
  }
  auto *derefed_ptr = _deref</*Mut = */ false, /* Nt = */ false, Shared>();
  if (pinned_raw_ptr) {
    *pinned_raw_ptr = derefed_ptr;
  }
  return helpers::finally([=]() {
    if (!in_scope) {
      DerefScope::exit();
    }
  });
}

FORCE_INLINE
void GenericFarMemPtr::flush() { _flush(/* obj_locked = */ false); }

FORCE_INLINE GenericUniquePtr::GenericUniquePtr() { meta().nullify(); }

FORCE_INLINE GenericUniquePtr::~GenericUniquePtr() {
  if (!meta().is_null()) {
    free();
  }
}

FORCE_INLINE GenericUniquePtr::GenericUniquePtr(uint64_t object_addr)
    : GenericFarMemPtr(/* shared = */ false, object_addr) {}

FORCE_INLINE void GenericUniquePtr::init(uint64_t object_addr) {
  GenericFarMemPtr::init(/* shared = */ false, object_addr);
}

FORCE_INLINE GenericUniquePtr::GenericUniquePtr(GenericUniquePtr &&other) {
  *this = std::move(other);
}

FORCE_INLINE GenericUniquePtr &GenericUniquePtr::
operator=(GenericUniquePtr &&other) {
  move(other, FarMemPtrMeta::kNull);
  return *this;
}

template <bool Mut, bool Nt> FORCE_INLINE void *GenericUniquePtr::_deref() {
  return GenericFarMemPtr::_deref<Mut, Nt, /* Shared = */ false>();
}

template <bool Nt>
FORCE_INLINE const void *GenericUniquePtr::deref(const DerefScope &scope) {
  return reinterpret_cast<const void *>(_deref</* Mut = */ false, Nt>());
}

template <bool Nt>
FORCE_INLINE void *GenericUniquePtr::deref_mut(const DerefScope &scope) {
  return _deref</* Mut = */ true, Nt>();
}

template <typename T>
FORCE_INLINE UniquePtr<T>::UniquePtr(uint64_t object_addr)
    : GenericUniquePtr(object_addr) {}

template <typename T>
FORCE_INLINE UniquePtr<T>::UniquePtr() : GenericUniquePtr() {
  meta().nullify();
}

template <typename T> FORCE_INLINE UniquePtr<T>::~UniquePtr() {
  if (!meta().is_null()) {
    free();
  }
}

template <typename T>
template <bool Nt>
FORCE_INLINE const T *UniquePtr<T>::deref(const DerefScope &scope) {
  return reinterpret_cast<const T *>(GenericUniquePtr::deref<Nt>(scope));
}

template <typename T>
template <bool Nt>
FORCE_INLINE T *UniquePtr<T>::deref_mut(const DerefScope &scope) {
  return reinterpret_cast<T *>(GenericUniquePtr::deref_mut<Nt>(scope));
}

template <typename T> FORCE_INLINE UniquePtr<T>::UniquePtr(UniquePtr &&other) {
  *this = std::move(other);
}

template <typename T>
FORCE_INLINE UniquePtr<T> &UniquePtr<T>::operator=(UniquePtr &&other) {
  GenericUniquePtr::operator=(std::move(other));
  return *this;
}

template <typename T> template <bool Nt> FORCE_INLINE T UniquePtr<T>::read() {
  DerefScope scope;
  return *(deref<Nt>(scope));
}

template <typename T>
template <bool Nt, typename U>
FORCE_INLINE void UniquePtr<T>::write(U &&u) {
  static_assert(std::is_same<std::decay_t<U>, std::decay_t<T>>::value,
                "U must be the same as T");
  DerefScope scope;
  *(deref_mut<Nt>(scope)) = u;
}

template <typename T> FORCE_INLINE void UniquePtr<T>::free() {
  if constexpr (std::is_trivially_destructible<T>::value) {
    if (!meta().is_present()) {
      return;
    }
  }
  T *raw_ptr;
  auto pin_guard = pin</* Shared */ false>(reinterpret_cast<void **>(&raw_ptr));
  raw_ptr->~T();
  _free();
}

FORCE_INLINE void GenericSharedPtr::traverse(auto f) {
  assert(meta().is_shared());
  auto *ptr = this;
  do {
    f(ptr);
    ptr = ptr->get_next_ptr();
  } while (ptr != this);
}

FORCE_INLINE void GenericSharedPtr::init(uint64_t object_addr) {
  GenericFarMemPtr::init(/* shared = */ true, object_addr);
  next_ptr_ = this;
}

FORCE_INLINE GenericSharedPtr *GenericSharedPtr::get_next_ptr() {
  return reinterpret_cast<GenericSharedPtr *>(next_ptr_);
}

FORCE_INLINE void GenericSharedPtr::set_next_ptr(GenericSharedPtr *ptr) {
  next_ptr_ = ptr;
}

FORCE_INLINE GenericSharedPtr::~GenericSharedPtr() {
  if (!meta().is_null()) {
    free();
  }
}

FORCE_INLINE GenericSharedPtr::GenericSharedPtr(uint64_t object_addr)
    : GenericFarMemPtr(/* shared = */ true, object_addr), next_ptr_(this) {}

FORCE_INLINE GenericSharedPtr::GenericSharedPtr(const GenericSharedPtr &other) {
  *this = other;
}

FORCE_INLINE GenericSharedPtr::GenericSharedPtr(GenericSharedPtr &&other) {
  *this = std::move(other);
}

FORCE_INLINE GenericSharedPtr &GenericSharedPtr::
operator=(GenericSharedPtr &&other) {
  move(other, FarMemPtrMeta::kNull);
  return *this;
}

FORCE_INLINE void GenericSharedPtr::free() {
  auto pin_guard = pin</* Shared */ true>();
  _free();
}

template <bool Mut, bool Nt> FORCE_INLINE void *GenericSharedPtr::_deref() {
  return GenericFarMemPtr::_deref<Mut, Nt, /* Shared = */ true>();
}

template <bool Nt>
FORCE_INLINE const void *GenericSharedPtr::deref(const DerefScope &scope) {
  return reinterpret_cast<const void *>(_deref</* Mut = */ false, Nt>());
}

template <bool Nt>
FORCE_INLINE void *GenericSharedPtr::deref_mut(const DerefScope &scope) {
  return _deref</* Mut = */ true, Nt>();
}

template <typename T>
FORCE_INLINE SharedPtr<T>::SharedPtr(uint64_t object_addr)
    : GenericSharedPtr(object_addr) {}

template <typename T>
FORCE_INLINE SharedPtr<T>::SharedPtr() : GenericSharedPtr() {}

template <typename T> FORCE_INLINE SharedPtr<T>::~SharedPtr() {
  if (!meta().is_null()) {
    free();
  }
}

template <typename T>
template <bool Nt>
FORCE_INLINE const T *SharedPtr<T>::deref(const DerefScope &scope) {
  return reinterpret_cast<const T *>(GenericSharedPtr::deref<Nt>(scope));
}

template <typename T>
template <bool Nt>
FORCE_INLINE T *SharedPtr<T>::deref_mut(const DerefScope &scope) {
  return reinterpret_cast<T *>(GenericSharedPtr::deref_mut<Nt>(scope));
}

template <typename T>
FORCE_INLINE SharedPtr<T>::SharedPtr(const SharedPtr &other)
    : GenericSharedPtr(other) {}

template <typename T>
FORCE_INLINE SharedPtr<T> &SharedPtr<T>::operator=(const SharedPtr &other) {
  GenericSharedPtr::operator=(other);
  return *this;
}

template <typename T>
FORCE_INLINE SharedPtr<T>::SharedPtr(SharedPtr &&other)
    : GenericSharedPtr(std::move(other)) {}

template <typename T>
FORCE_INLINE SharedPtr<T> &SharedPtr<T>::operator=(SharedPtr &&other) {
  GenericSharedPtr::operator=(std::move(other));
  return *this;
}

template <typename T> template <bool Nt> FORCE_INLINE T SharedPtr<T>::read() {
  DerefScope scope;
  return *(deref<Nt>(scope));
}

template <typename T>
template <bool Nt, typename U>
FORCE_INLINE void SharedPtr<T>::write(U &&u) {
  static_assert(std::is_same<std::decay_t<U>, std::decay_t<T>>::value,
                "U must be the same as T");
  DerefScope scope;
  *(deref_mut<Nt>(scope)) = u;
}

template <typename T> FORCE_INLINE void SharedPtr<T>::free() {
  T *raw_ptr;
  auto pin_guard = pin</* Shared */ true>(reinterpret_cast<void **>(&raw_ptr));
  if (next_ptr_ == this) {
    raw_ptr->~T();
  }
  _free();
}

} // namespace far_memory
