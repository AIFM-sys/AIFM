#pragma once

#include "server_ds.hpp"

#include <memory>

namespace far_memory {
class ServerPtr : public ServerDS {
private:
  std::unique_ptr<uint8_t> buf_;
  friend class ServerPtrFactory;

public:
  ServerPtr(uint32_t param_len, uint8_t *params);
  ~ServerPtr();
  void read_object(uint8_t obj_id_len, const uint8_t *obj_id,
                   uint16_t *data_len, uint8_t *data_buf);
  void write_object(uint8_t obj_id_len, const uint8_t *obj_id,
                    uint16_t data_len, const uint8_t *data_buf);
  bool remove_object(uint8_t obj_id_len, const uint8_t *obj_id);
  void compute(uint8_t opcode, uint16_t input_len, const uint8_t *input_buf,
               uint16_t *output_len, uint8_t *output_buf);
};

class ServerPtrFactory : public ServerDSFactory {
public:
  ServerDS *build(uint32_t param_len, uint8_t *params);
};
} // namespace far_memory
