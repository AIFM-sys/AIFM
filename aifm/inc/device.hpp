#pragma once

extern "C" {
#include <runtime/tcp.h>
}

#include "helpers.hpp"
#include "server.hpp"
#include "shared_pool.hpp"

namespace far_memory {

class FarMemDevice {
public:
  uint64_t far_mem_size_;
  uint32_t prefetch_win_size_;

  FarMemDevice(uint64_t far_mem_size, uint32_t prefetch_win_size);
  virtual ~FarMemDevice() {}
  uint64_t get_far_mem_size() const { return far_mem_size_; }
  uint32_t get_prefetch_win_size() const { return prefetch_win_size_; }
  virtual void read_object(uint8_t ds_id, uint8_t obj_id_len,
                           const uint8_t *obj_id, uint16_t *data_len,
                           uint8_t *data_buf) = 0;
  virtual void write_object(uint8_t ds_id, uint8_t obj_id_len,
                            const uint8_t *obj_id, uint16_t data_len,
                            const uint8_t *data_buf) = 0;
  virtual bool remove_object(uint64_t ds_id, uint8_t obj_id_len,
                             const uint8_t *obj_id) = 0;
  virtual void construct(uint8_t ds_type, uint8_t ds_id, uint8_t param_len,
                         uint8_t *params) = 0;
  virtual void destruct(uint8_t ds_id) = 0;
  virtual void compute(uint8_t ds_id, uint8_t opcode, uint16_t input_len,
                       const uint8_t *input_buf, uint16_t *output_len,
                       uint8_t *output_buf) = 0;
};

class FakeDevice : public FarMemDevice {
private:
  constexpr static uint32_t kPrefetchWinSize = 1 << 20;
  Server server_;

public:
  FakeDevice(uint64_t far_mem_size);
  ~FakeDevice();
  void read_object(uint8_t ds_id, uint8_t obj_id_len, const uint8_t *obj_id,
                   uint16_t *data_len, uint8_t *data_buf);
  void write_object(uint8_t ds_id, uint8_t obj_id_len, const uint8_t *obj_id,
                    uint16_t data_len, const uint8_t *data_buf);
  bool remove_object(uint64_t ds_id, uint8_t obj_id_len, const uint8_t *obj_id);
  void construct(uint8_t ds_type, uint8_t ds_id, uint8_t param_len,
                 uint8_t *params);
  void destruct(uint8_t ds_id);
  void compute(uint8_t ds_id, uint8_t opcode, uint16_t input_len,
               const uint8_t *input_buf, uint16_t *output_len,
               uint8_t *output_buf);
};

class TCPDevice : public FarMemDevice {
private:
  constexpr static uint32_t kPrefetchWinSize = 1 << 20;

  tcpconn_t *remote_master_;
  SharedPool<tcpconn_t *> shared_pool_;

  void _read_object(tcpconn_t *remote_slave, uint8_t ds_id, uint8_t obj_id_len,
                    const uint8_t *obj_id, uint16_t *data_len,
                    uint8_t *data_buf);
  void _write_object(tcpconn_t *remote_slave, uint8_t ds_id, uint8_t obj_id_len,
                     const uint8_t *obj_id, uint16_t data_len,
                     const uint8_t *data_buf);
  bool _remove_object(tcpconn_t *remote_slave, uint64_t ds_id,
                      uint8_t obj_id_len, const uint8_t *obj_id);
  void _construct(tcpconn_t *remote_slave, uint8_t ds_type, uint8_t ds_id,
                  uint8_t param_len, uint8_t *params);
  void _destruct(tcpconn_t *remote_slave, uint8_t ds_id);
  void _compute(tcpconn_t *remote_slave, uint8_t ds_id, uint8_t opcode,
                uint16_t input_len, const uint8_t *input_buf,
                uint16_t *output_len, uint8_t *output_buf);

public:
  // TCPDevice talks to remote agent via TCP.
  // Request format:
  //     |OpCode (1B)|Data (optional)|
  // All possible OpCode:
  //     0. init
  //     1. shutdown
  //     2. read_object
  //     3. write_object
  //     4. remove_object
  //     5. construct
  //     6. destruct
  //     7. compute
  constexpr static uint32_t kOpcodeSize = 1;
  constexpr static uint32_t kPortSize = 2;
  constexpr static uint32_t kLargeDataSize = 512;
  constexpr static uint32_t kMaxComputeDataLen = 65535;

  constexpr static uint8_t kOpInit = 0;
  constexpr static uint8_t kOpShutdown = 1;
  constexpr static uint8_t kOpReadObject = 2;
  constexpr static uint8_t kOpWriteObject = 3;
  constexpr static uint8_t kOpRemoveObject = 4;
  constexpr static uint8_t kOpConstruct = 5;
  constexpr static uint8_t kOpDeconstruct = 6;
  constexpr static uint8_t kOpCompute = 7;

  TCPDevice(netaddr raddr, uint32_t num_connections, uint64_t far_mem_size);
  ~TCPDevice();
  void read_object(uint8_t ds_id, uint8_t obj_id_len, const uint8_t *obj_id,
                   uint16_t *data_len, uint8_t *data_buf);
  void write_object(uint8_t ds_id, uint8_t obj_id_len, const uint8_t *obj_id,
                    uint16_t data_len, const uint8_t *data_buf);
  bool remove_object(uint64_t ds_id, uint8_t obj_id_len, const uint8_t *obj_id);
  void construct(uint8_t ds_type, uint8_t ds_id, uint8_t param_len,
                 uint8_t *params);
  void destruct(uint8_t ds_id);
  void compute(uint8_t ds_id, uint8_t opcode, uint16_t input_len,
               const uint8_t *input_buf, uint16_t *output_len,
               uint8_t *output_buf);
};

} // namespace far_memory
