#include "server_hashtable.hpp"
#include "helpers.hpp"

#include <cstring>

namespace far_memory {

ServerHashTable::ServerHashTable(uint32_t param_len, uint8_t *params) {
  auto remote_num_entries_shift = *reinterpret_cast<uint32_t *>(&params[0]);
  auto remote_data_size =
      *reinterpret_cast<uint64_t *>(&params[sizeof(remote_num_entries_shift)]);
  local_hopscotch_.reset(new LocalGenericConcurrentHopscotch(
      remote_num_entries_shift, remote_data_size));
}

ServerHashTable::~ServerHashTable() {}

void ServerHashTable::read_object(uint8_t obj_id_len, const uint8_t *obj_id,
                                  uint16_t *data_len, uint8_t *data_buf) {
#ifdef HASHTABLE_EXCLUSIVE
  local_hopscotch_->get(obj_id_len, obj_id, data_len, data_buf,
                        /* remove= */ true);
#else
  local_hopscotch_->get(obj_id_len, obj_id, data_len, data_buf,
                        /* remove= */ false);
#endif
}

void ServerHashTable::write_object(uint8_t obj_id_len, const uint8_t *obj_id,
                                   uint16_t data_len, const uint8_t *data_buf) {
  local_hopscotch_->put(obj_id_len, obj_id, data_len, data_buf);
}

bool ServerHashTable::remove_object(uint8_t obj_id_len, const uint8_t *obj_id) {
  return local_hopscotch_->remove(obj_id_len, obj_id);
}

void ServerHashTable::compute(uint8_t opcode, uint16_t input_len,
                              const uint8_t *input_buf, uint16_t *output_len,
                              uint8_t *output_buf) {
  BUG();
}

ServerDS *ServerHashTableFactory::build(uint32_t param_len, uint8_t *params) {
  return new ServerHashTable(param_len, params);
}

} // namespace far_memory
