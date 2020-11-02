#pragma once

#include <cstdint>

class ServerDS {
public:
  virtual ~ServerDS() {}
  virtual void read_object(uint8_t obj_id_len, const uint8_t *obj_id,
                           uint16_t *data_len, uint8_t *data_buf) = 0;
  virtual void write_object(uint8_t obj_id_len, const uint8_t *obj_id,
                            uint16_t data_len, const uint8_t *data_buf) = 0;
  virtual bool remove_object(uint8_t obj_id_len, const uint8_t *obj_id) = 0;
  virtual void compute(uint8_t opcode, uint16_t input_len,
                       const uint8_t *input_buf, uint16_t *output_len,
                       uint8_t *output_buf) = 0;
};

class ServerDSFactory {
public:
  virtual ServerDS *build(uint32_t param_len, uint8_t *params) = 0;
};
