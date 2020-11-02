#pragma once

#include "helpers.hpp"

#include <cstring>

namespace far_memory {

FORCE_INLINE Object::Object() {}

FORCE_INLINE Object::Object(uint64_t addr) : addr_(addr) {}

FORCE_INLINE Object::Object(uint64_t addr, uint8_t ds_id, uint16_t data_len,
                            uint8_t id_len, const uint8_t *id)
    : Object(addr) {
  init(ds_id, data_len, id_len, id);
}

FORCE_INLINE void Object::init(uint8_t ds_id, uint16_t data_len, uint8_t id_len,
                               const uint8_t *id) {
  set_ds_id(ds_id);
  set_data_len(data_len);
  set_obj_id_len(id_len);
  set_obj_id(id, id_len);
}

FORCE_INLINE void Object::set_ds_id(uint8_t ds_id) {
  auto *ptr = reinterpret_cast<uint8_t *>(addr_ + kDSIDPos);
  *ptr = ds_id;
}

FORCE_INLINE uint8_t Object::get_ds_id() const {
  auto *ptr = reinterpret_cast<uint8_t *>(addr_ + kDSIDPos);
  return *ptr;
}

FORCE_INLINE void Object::set_obj_id_len(uint8_t id_len) {
  auto *ptr = reinterpret_cast<uint8_t *>(addr_ + kIDLenPos);
  *ptr = id_len;
}

FORCE_INLINE uint8_t Object::get_obj_id_len() const {
  auto *ptr = reinterpret_cast<uint8_t *>(addr_ + kIDLenPos);
  return *ptr;
}

FORCE_INLINE uint64_t Object::get_addr() const { return addr_; }

FORCE_INLINE bool Object::is_freed() const {
  return (*reinterpret_cast<uint8_t *>(addr_ + kPtrAddrPos + kPtrAddrSize -
                                       1)) == 0xFF;
}

FORCE_INLINE void Object::free() {
  *reinterpret_cast<uint8_t *>(addr_ + kPtrAddrPos + kPtrAddrSize - 1) = 0xFF;
}

FORCE_INLINE void Object::set_data_len(uint16_t data_len) {
  auto *ptr = reinterpret_cast<uint16_t *>(addr_ + kDataLenPos);
  *ptr = data_len;
}

FORCE_INLINE uint16_t Object::get_data_len() const {
  auto *ptr = reinterpret_cast<uint16_t *>(addr_ + kDataLenPos);
  return *ptr;
}

FORCE_INLINE void Object::set_obj_id(const uint8_t *id, uint8_t id_len) {
  auto offset = kHeaderSize + get_data_len();
  auto *ptr = reinterpret_cast<void *>(addr_ + offset);
  memcpy(ptr, id, id_len);
}

FORCE_INLINE const uint8_t *Object::get_obj_id() const {
  auto offset = kHeaderSize + get_data_len();
  auto *ptr = reinterpret_cast<const uint8_t *>(addr_ + offset);
  return ptr;
}

FORCE_INLINE void Object::set_ptr_addr(uint64_t address) {
  helpers::small_memcpy<kPtrAddrSize>(
      reinterpret_cast<void *>(addr_ + kPtrAddrPos), &address);
}

FORCE_INLINE uint64_t Object::get_ptr_addr() const {
  uint64_t address = 0;
  helpers::small_memcpy<kPtrAddrSize>(
      &address, reinterpret_cast<void *>(addr_ + kPtrAddrPos));
  return address;
}

FORCE_INLINE uint64_t Object::get_data_addr() const {
  return addr_ + kHeaderSize;
}

FORCE_INLINE uint16_t Object::size() const {
  return kHeaderSize + get_data_len() + get_obj_id_len();
}
} // namespace far_memory
