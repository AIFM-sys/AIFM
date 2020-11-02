#pragma once

#include "deref_scope.hpp"
#include "object.hpp"

namespace far_memory {

// Format:
//  I) |XXXXXXX !H(1b)|  0   S(1b)!D(1b)00000|E(1b)|  Object Data Addr(47b)  |
// II) |   DS_ID(8b)  |!P(1b)S(1b)| Object Size(16b) |      ObjectID(38b)    |
//
//                  D: dirty bit.
//                  P: present.
//                  H: hot bits.
//                  S: shared bits, meaning the pointer is a UniquePtr or a
//                     SharedPtr.
//                  E: The pointed data is being evacuated.
//   Object Data Addr: the address of the referenced object's data (which are
//                     stored in regions).
//              DS_ID: data structure ID.
//        Object Size: the size of the pointed object.
//          Object ID: universal object ID (used when swapping in).

class FarMemPtrMeta {
private:
  constexpr static uint32_t kSize = 8;
  constexpr static uint32_t kEvacuationPos = 2;
  constexpr static uint32_t kObjectIDBitPos = 26;
  constexpr static uint32_t kObjectIDBitSize = 38;
  constexpr static uint32_t kObjectDataAddrPos = 2;
  constexpr static uint32_t kObjectDataAddrSize = 6;
  constexpr static uint32_t kDirtyClear = 0x400U;
  constexpr static uint32_t kPresentClear = 0x100U;
  constexpr static uint32_t kHotClear = 0x80U;
  constexpr static uint32_t kEvacuationSet = 0x10000U;
  constexpr static uint32_t kObjIDLenPosShift = 9;
  constexpr static uint32_t kObjectDataAddrBitPos = 17;
  constexpr static uint32_t kObjectSizeBitPos = 10;
  constexpr static uint32_t kHotPos = 0;
  constexpr static uint32_t kPresentPos = 1;
  constexpr static uint32_t kHotThresh = 2;
  constexpr static uint32_t kDSIDPos = 0;
  constexpr static uint32_t kSharedBitPos = 9;

  uint8_t metadata_[kSize];
  friend class FarMemManager;
  friend class GenericFarMemPtr;

  FarMemPtrMeta();
  void init(bool shared, uint64_t object_addr);

public:
  constexpr static uint64_t kNull = kPresentClear;
  constexpr static uint64_t kNullMask =
      ((~static_cast<uint64_t>(0)) << (8 * kPresentPos));

  FarMemPtrMeta(const FarMemPtrMeta &other);
  FarMemPtrMeta(bool shared, uint64_t object_addr);
  bool operator==(const FarMemPtrMeta &other) const;
  bool operator!=(const FarMemPtrMeta &other) const;
  bool is_dirty() const;
  void set_dirty();
  void clear_dirty();
  bool is_hot() const;
  bool is_nt() const;
  void clear_hot();
  void set_hot();
  bool is_present() const;
  void set_present(uint64_t object_addr);
  void set_evacuation();
  bool is_evacuation() const;
  bool is_shared() const;
  void set_shared();
  uint64_t get_object_id() const;
  uint64_t get_object_data_addr() const;
  void set_object_data_addr(uint64_t new_local_object_addr);
  uint64_t get_object_addr() const;
  uint16_t get_object_size() const;
  Object object();
  uint8_t get_ds_id() const;
  bool is_null() const;
  void nullify();
  uint64_t to_uint64_t() const;
  void from_uint64_t(uint64_t val);
  void mutator_copy(uint64_t new_local_object_addr);
  void gc_copy(uint64_t new_local_object_addr);
  void gc_wb(uint8_t ds_id, uint16_t object_size, uint64_t obj_id);
  static FarMemPtrMeta *from_object(const Object &object);
};

class GenericFarMemPtr {
private:
  FarMemPtrMeta meta_;

protected:
  friend class GenericDataFrameVector;
  friend class GenericConcurrentHopscotch;
  friend class FarMemManager;
  friend class GCParallelMarker;

  GenericFarMemPtr();
  GenericFarMemPtr(bool shared, uint64_t object_addr);
  void init(bool shared, uint64_t object_addr);
  Object object();
  FarMemPtrMeta &meta();
  const FarMemPtrMeta &meta() const;
  bool mutator_migrate_object();
  template <bool Shared> auto pin(void **pinned_raw_ptr = nullptr);
  template <bool Mut, bool Nt, bool Shared> void *_deref();
  void _flush(bool obj_locked);

public:
  void nullify();
  bool is_null() const;
  void swap_in(bool nt);
  void flush();
  void move(GenericFarMemPtr &other, uint64_t reset_value);
};

class GenericUniquePtr : public GenericFarMemPtr {
protected:
  friend class FarMemTest;
  friend class FarMemManager;
  template <typename InduceFn, typename InferFn, typename MappingFn>
  friend class Prefetcher;

  void init(uint64_t object_addr);
  void _free();
  void evacuate();

public:
  GenericUniquePtr();
  ~GenericUniquePtr();
  GenericUniquePtr(uint64_t object_addr);
  GenericUniquePtr(GenericUniquePtr &&other);
  GenericUniquePtr &operator=(GenericUniquePtr &&other);
  NOT_COPYABLE(GenericUniquePtr);
  template <bool Mut, bool Nt> void *_deref();
  template <bool Nt = false> const void *deref(const DerefScope &scope);
  template <bool Nt = false> void *deref_mut(const DerefScope &scope);
  void free(bool race = false);
};

template <typename T> class UniquePtr : public GenericUniquePtr {
private:
  friend class FarMemManager;
  template <typename R, uint64_t... Dims> friend class Array;

  UniquePtr(uint64_t local_object_addr);

public:
  UniquePtr();
  ~UniquePtr();
  UniquePtr(UniquePtr &&other);
  UniquePtr &operator=(UniquePtr &&other);
  NOT_COPYABLE(UniquePtr);
  template <bool Nt = false> const T *deref(const DerefScope &scope);
  template <bool Nt = false> T *deref_mut(const DerefScope &scope);
  template <bool Nt = false> T read();
  template <bool Nt = false, typename U> void write(U &&u);
  void free();
};

class GenericSharedPtr : public GenericFarMemPtr {
protected:
  constexpr static uint32_t kTailPos = 63;
  GenericSharedPtr *next_ptr_;
  friend class FarMemTest;
  friend class FarMemManager;
  friend class GenericFarMemPtr;
  friend class GCParallelMarker;

  void init(uint64_t object_addr);
  GenericSharedPtr *get_next_ptr();
  void set_next_ptr(GenericSharedPtr *);
  void traverse(auto f);
  void _free();

public:
  ~GenericSharedPtr();
  GenericSharedPtr(uint64_t object_addr);
  GenericSharedPtr(const GenericSharedPtr &other);
  GenericSharedPtr &operator=(const GenericSharedPtr &other);
  GenericSharedPtr(GenericSharedPtr &&other);
  GenericSharedPtr &operator=(GenericSharedPtr &&other);
  template <bool Mut, bool Nt> void *_deref();
  template <bool Nt = false> const void *deref(const DerefScope &scope);
  template <bool Nt = false> void *deref_mut(const DerefScope &scope);
  void free();
};

template <typename T> class SharedPtr : public GenericSharedPtr {
private:
  friend class FarMemManager;

  SharedPtr(uint64_t local_object_addr);

public:
  SharedPtr();
  ~SharedPtr();
  SharedPtr(const SharedPtr &other);
  SharedPtr &operator=(const SharedPtr &other);
  SharedPtr(SharedPtr &&other);
  SharedPtr &operator=(SharedPtr &&pther);
  template <bool Nt = false> const T *deref(const DerefScope &scope);
  template <bool Nt = false> T *deref_mut(const DerefScope &scope);
  template <bool Nt = false> T read();
  template <bool Nt = false, typename U> void write(U &&u);
  void free();
};

} // namespace far_memory

#include "internal/pointer.ipp"
