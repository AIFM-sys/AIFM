extern "C" {
#include <base/assert.h>
#include <base/compiler.h>
#include <base/limits.h>
#include <base/stddef.h>
}

#include "server.hpp"
#include "server_dataframe_vector.hpp"
#include "server_hashtable.hpp"
#include "server_ptr.hpp"

namespace far_memory {

std::unique_ptr<ServerDS> Server::server_ds_ptrs_[kMaxNumDSIDs];

Server::Server() {
  register_ds(kVanillaPtrDSType, new ServerPtrFactory());
  register_ds(kHashTableDSType, new ServerHashTableFactory());
  register_ds(kDataFrameVectorDSType, new ServerDataFrameVectorFactory());
}

void Server::register_ds(uint8_t ds_type, ServerDSFactory *factory) {
  registered_server_ds_factorys_[ds_type] = factory;
}

void Server::construct(uint8_t ds_type, uint8_t ds_id, uint8_t param_len,
                       uint8_t *params) {
  auto factory = registered_server_ds_factorys_[ds_type];
  BUG_ON(server_ds_ptrs_[ds_id]);
  server_ds_ptrs_[ds_id].reset(factory->build(param_len, params));
}

void Server::destruct(uint8_t ds_id) {
  BUG_ON(!server_ds_ptrs_[ds_id]);
  server_ds_ptrs_[ds_id].reset();
}

void Server::read_object(uint8_t ds_id, uint8_t obj_id_len,
                         const uint8_t *obj_id, uint16_t *data_len,
                         uint8_t *data_buf) {
  auto ds_ptr = server_ds_ptrs_[ds_id].get();
  if (!ds_ptr) {
    ds_ptr = server_ds_ptrs_[kVanillaPtrDSID].get();
  }
  ds_ptr->read_object(obj_id_len, obj_id, data_len, data_buf);
}

void Server::write_object(uint8_t ds_id, uint8_t obj_id_len,
                          const uint8_t *obj_id, uint16_t data_len,
                          const uint8_t *data_buf) {
  auto ds_ptr = server_ds_ptrs_[ds_id].get();
  if (!ds_ptr) {
    ds_ptr = server_ds_ptrs_[kVanillaPtrDSID].get();
  }
  ds_ptr->write_object(obj_id_len, obj_id, data_len, data_buf);
}

bool Server::remove_object(uint64_t ds_id, uint8_t obj_id_len,
                           const uint8_t *obj_id) {
  auto ds_ptr = server_ds_ptrs_[ds_id].get();
  if (!ds_ptr) {
    ds_ptr = server_ds_ptrs_[kVanillaPtrDSID].get();
  }
  return ds_ptr->remove_object(obj_id_len, obj_id);
}

void Server::compute(uint8_t ds_id, uint8_t opcode, uint16_t input_len,
                     const uint8_t *input_buf, uint16_t *output_len,
                     uint8_t *output_buf) {
  auto ds_ptr = server_ds_ptrs_[ds_id].get();
  return ds_ptr->compute(opcode, input_len, input_buf, output_len, output_buf);
}

ServerDS *Server::get_server_ds(uint8_t ds_id) {
  return server_ds_ptrs_[ds_id].get();
}

} // namespace far_memory
