#pragma once

#include "helpers.hpp"
#include "reader_writer_lock.hpp"
#include "server.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

namespace far_memory {

template <typename T> class ServerDataFrameVector : public ServerDS {
private:
  ReaderWriterLock lock_;
  friend class ServerDataFrameVectorFactory;

  void compute_reserve(uint16_t input_len, const uint8_t *input_buf,
                       uint16_t *output_len, uint8_t *output_buf);
  void compute_unique(uint16_t input_len, const uint8_t *input_buf,
                      uint16_t *output_len, uint8_t *output_buf);
  void compute_copy_data_by_idx(uint16_t input_len, const uint8_t *input_buf,
                                uint16_t *output_len, uint8_t *output_buf);
  void compute_shuffle_data_by_idx(uint16_t input_len, const uint8_t *input_buf,
                                   uint16_t *output_len, uint8_t *output_buf);
  void compute_assign(uint16_t input_len, const uint8_t *input_buf,
                      uint16_t *output_len, uint8_t *output_buf);
  void compute_aggregate(uint8_t opcode, uint16_t input_len,
                         const uint8_t *input_buf, uint16_t *output_len,
                         uint8_t *output_buf);
  template <typename Key_t>
  std::pair<uint64_t, uint64_t>
  _compute_aggregate(uint8_t opcode, uint8_t result_ds, uint8_t key_ds,
                     uint64_t size);
  template <typename U>
  void _compute_unique(uint64_t vec_size, std::vector<U> &unique_vec);

public:
  std::vector<T> vec_;

  ServerDataFrameVector();
  ~ServerDataFrameVector();
  void read_object(uint8_t obj_id_len, const uint8_t *obj_id,
                   uint16_t *data_len, uint8_t *data_buf);
  void write_object(uint8_t obj_id_len, const uint8_t *obj_id,
                    uint16_t data_len, const uint8_t *data_buf);
  bool remove_object(uint8_t obj_id_len, const uint8_t *obj_id);
  void compute(uint8_t opcode, uint16_t input_len, const uint8_t *input_buf,
               uint16_t *output_len, uint8_t *output_buf);
};

class ServerDataFrameVectorFactory : public ServerDSFactory {
public:
  ServerDS *build(uint32_t param_len, uint8_t *params);
};

}; // namespace far_memory
