#pragma once

#include "local_concurrent_hopscotch.hpp"
#include "server.hpp"

#include <cstring>
#include <memory>

namespace far_memory {
class ServerHashTable : public ServerDS {
private:
  std::unique_ptr<LocalGenericConcurrentHopscotch> local_hopscotch_;
  friend class ServerHashTableFactory;

public:
  ServerHashTable(uint32_t param_len, uint8_t *params);
  ~ServerHashTable();
  void read_object(uint8_t obj_id_len, const uint8_t *obj_id,
                   uint16_t *data_len, uint8_t *data_buf);
  void write_object(uint8_t obj_id_len, const uint8_t *obj_id,
                    uint16_t data_len, const uint8_t *data_buf);
  bool remove_object(uint8_t obj_id_len, const uint8_t *obj_id);
  void compute(uint8_t opcode, uint16_t input_len, const uint8_t *input_buf,
               uint16_t *output_len, uint8_t *output_buf);
};

class ServerHashTableFactory : public ServerDSFactory {
public:
  ServerDS *build(uint32_t param_len, uint8_t *params);
};

}; // namespace far_memory
