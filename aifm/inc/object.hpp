#pragma once

#include <limits>

namespace far_memory {

// Forward declaration.
template <typename T> class UniquePtr;

class Object {
  //
  // Format:
  // |<------------------ header ------------------>|
  // |ptr_addr(6B)|data_len(2B)|ds_id(1B)|id_len(1B)|data|object_ID|
  //
  //      ptr_addr: points to the corresponding far-mem pointer. During GC,
  //                the collector uses the field to jump from Region to far-mem
  //                pointer, and marks the far-mem pointer as absent from local
  //                cache.
  //      data_len: the length of object data.
  //         ds_id: the data structure ID.
  //        id_len: the length of object ID.
  //          data: object data.
  //     object_ID: the unique object ID which is used by the remote side to
  //                locate the object during swapping in and swapping out.
private:
  constexpr static uint32_t kPtrAddrPos = 0;
  constexpr static uint32_t kPtrAddrSize = 6;
  constexpr static uint32_t kDataLenPos = 6;
  constexpr static uint32_t kDSIDPos = 8;
  constexpr static uint32_t kIDLenPos = 9;
  // It stores the address of the object (which is stored in the local region).
  uint64_t addr_;

public:
  constexpr static uint32_t kDSIDSize = 1;
  constexpr static uint32_t kIDLenSize = 1;
  constexpr static uint32_t kDataLenSize = 2;
  constexpr static uint32_t kHeaderSize =
      kPtrAddrSize + kDataLenSize + kDSIDSize + kIDLenSize;
  constexpr static uint16_t kMaxObjectSize =
      std::numeric_limits<uint16_t>::max();
  constexpr static uint16_t kMaxObjectIDSize = (1 << (8 * kIDLenSize)) - 1;
  constexpr static uint16_t kMaxObjectDataSize =
      kMaxObjectSize - kHeaderSize - kMaxObjectIDSize;

  Object();
  // Create a reference to the object at address addr.
  Object(uint64_t addr);
  // Initialize the object at address addr. Field ptr_addr is written by
  // far-mem pointer.
  Object(uint64_t addr, uint8_t ds_id, uint16_t data_len, uint8_t id_len,
         const uint8_t *id);
  void init(uint8_t ds_id, uint16_t data_len, uint8_t id_len,
            const uint8_t *id);
  uint64_t get_addr() const;
  uint16_t get_data_len() const;
  uint8_t get_obj_id_len() const;
  void set_ptr_addr(uint64_t address);
  const uint8_t *get_obj_id() const;
  uint64_t get_ptr_addr() const;
  uint64_t get_data_addr() const;
  void set_ds_id(uint8_t ds_id);
  uint8_t get_ds_id() const;
  void set_data_len(uint16_t data_len);
  void set_obj_id(const uint8_t *id, uint8_t id_len);
  void set_obj_id_len(uint8_t id_len);
  uint16_t size() const;
  bool is_freed() const;
  void free();
};

} // namespace far_memory

#include "internal/object.ipp"
