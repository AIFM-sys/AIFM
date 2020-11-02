extern "C" {
#include <net/ip.h>
#include <runtime/storage.h>
}

#include "device.hpp"
#include "object.hpp"
#include "stats.hpp"

#include <cstring>

namespace far_memory {

FarMemDevice::FarMemDevice(uint64_t far_mem_size, uint32_t prefetch_win_size)
    : far_mem_size_(far_mem_size), prefetch_win_size_(prefetch_win_size) {}

FakeDevice::FakeDevice(uint64_t far_mem_size)
    : FarMemDevice(far_mem_size, kPrefetchWinSize), server_() {
  server_.construct(kVanillaPtrDSType, kVanillaPtrDSID, sizeof(far_mem_size),
                    reinterpret_cast<uint8_t *>(&far_mem_size));
}

FakeDevice::~FakeDevice() { destruct(kVanillaPtrDSID); }

void FakeDevice::read_object(uint8_t ds_id, uint8_t obj_id_len,
                             const uint8_t *obj_id, uint16_t *data_len,
                             uint8_t *data_buf) {
  server_.read_object(ds_id, obj_id_len, obj_id, data_len, data_buf);
}

void FakeDevice::write_object(uint8_t ds_id, uint8_t obj_id_len,
                              const uint8_t *obj_id, uint16_t data_len,
                              const uint8_t *data_buf) {
  server_.write_object(ds_id, obj_id_len, obj_id, data_len, data_buf);
}

bool FakeDevice::remove_object(uint64_t ds_id, uint8_t obj_id_len,
                               const uint8_t *obj_id) {
  return server_.remove_object(ds_id, obj_id_len, obj_id);
}

void FakeDevice::construct(uint8_t ds_type, uint8_t ds_id, uint8_t param_len,
                           uint8_t *params) {
  server_.construct(ds_type, ds_id, param_len, params);
}

void FakeDevice::destruct(uint8_t ds_id) { server_.destruct(ds_id); }

void FakeDevice::compute(uint8_t ds_id, uint8_t opcode, uint16_t input_len,
                         const uint8_t *input_buf, uint16_t *output_len,
                         uint8_t *output_buf) {
  server_.compute(ds_id, opcode, input_len, input_buf, output_len, output_buf);
}

// Request:
//     |OpCode = Init (1B)|Far Mem Size (8B)|
// Response:
//     |Ack (1B)|
TCPDevice::TCPDevice(netaddr raddr, uint32_t num_connections,
                     uint64_t far_mem_size)
    : FarMemDevice(far_mem_size, kPrefetchWinSize),
      shared_pool_(num_connections) {
  // Initialize the master connection.
  netaddr laddr = {.ip = MAKE_IP_ADDR(0, 0, 0, 0), .port = 0};
  BUG_ON(tcp_dial(laddr, raddr, &remote_master_) != 0);
  char req[kOpcodeSize + sizeof(far_mem_size)];
  __builtin_memcpy(req, &kOpInit, kOpcodeSize);
  __builtin_memcpy(req + kOpcodeSize, &far_mem_size, sizeof(far_mem_size));
  helpers::tcp_write_until(remote_master_, req, sizeof(req));
  uint8_t ack;
  helpers::tcp_read_until(remote_master_, &ack, sizeof(ack));

  // Initialize slave connections.
  tcpconn_t *remote_slave;
  for (uint32_t i = 0; i < num_connections; i++) {
    BUG_ON(tcp_dial(laddr, raddr, &remote_slave) != 0);
    shared_pool_.push(remote_slave);
  }

  construct(kVanillaPtrDSType, kVanillaPtrDSID, sizeof(far_mem_size),
            reinterpret_cast<uint8_t *>(&far_mem_size));
}

// Request:
//     |Opcode = Shutdown (1B)|
// Response:
//     |Ack (1B)|
TCPDevice::~TCPDevice() {
  destruct(kVanillaPtrDSID);

  helpers::tcp_write_until(remote_master_, &kOpShutdown, kOpcodeSize);
  uint8_t ack;
  helpers::tcp_read_until(remote_master_, &ack, sizeof(ack));
  tcp_close(remote_master_);
  shared_pool_.for_each([&](auto remote_slave) { tcp_close(remote_slave); });
}

void TCPDevice::read_object(uint8_t ds_id, uint8_t obj_id_len,
                            const uint8_t *obj_id, uint16_t *data_len,
                            uint8_t *data_buf) {
  auto remote_slave = shared_pool_.pop();
  _read_object(remote_slave, ds_id, obj_id_len, obj_id, data_len, data_buf);
  shared_pool_.push(remote_slave);
}

void TCPDevice::write_object(uint8_t ds_id, uint8_t obj_id_len,
                             const uint8_t *obj_id, uint16_t data_len,
                             const uint8_t *data_buf) {
  auto remote_slave = shared_pool_.pop();
  _write_object(remote_slave, ds_id, obj_id_len, obj_id, data_len, data_buf);
  shared_pool_.push(remote_slave);
}

bool TCPDevice::remove_object(uint64_t ds_id, uint8_t obj_id_len,
                              const uint8_t *obj_id) {
  auto remote_slave = shared_pool_.pop();
  auto ret = _remove_object(remote_slave, ds_id, obj_id_len, obj_id);
  shared_pool_.push(remote_slave);

  return ret;
}

void TCPDevice::construct(uint8_t ds_type, uint8_t ds_id, uint8_t param_len,
                          uint8_t *params) {
  auto remote_slave = shared_pool_.pop();
  _construct(remote_slave, ds_type, ds_id, param_len, params);
  shared_pool_.push(remote_slave);
}

void TCPDevice::destruct(uint8_t ds_id) {
  auto remote_slave = shared_pool_.pop();
  _destruct(remote_slave, ds_id);
  shared_pool_.push(remote_slave);
}

void TCPDevice::compute(uint8_t ds_id, uint8_t opcode, uint16_t input_len,
                        const uint8_t *input_buf, uint16_t *output_len,
                        uint8_t *output_buf) {
  auto remote_slave = shared_pool_.pop();
  _compute(remote_slave, ds_id, opcode, input_len, input_buf, output_len,
           output_buf);
  shared_pool_.push(remote_slave);
}

// Request:
// |Opcode = KOpReadObject(1B) | ds_id(1B) | obj_id_len(1B) | obj_id |
// Response:
// |data_len(2B)|data_buf(data_len B)|
void TCPDevice::_read_object(tcpconn_t *remote_slave, uint8_t ds_id,
                             uint8_t obj_id_len, const uint8_t *obj_id,
                             uint16_t *data_len, uint8_t *data_buf) {
  Stats::start_measure_read_object_cycles();
  
  uint8_t req[kOpcodeSize + Object::kDSIDSize + Object::kIDLenSize +
              Object::kMaxObjectIDSize];

  __builtin_memcpy(&req[0], &kOpReadObject, sizeof(kOpReadObject));
  __builtin_memcpy(&req[kOpcodeSize], &ds_id, Object::kDSIDSize);
  __builtin_memcpy(&req[kOpcodeSize + Object::kDSIDSize], &obj_id_len,
                   Object::kIDLenSize);
  memcpy(&req[kOpcodeSize + Object::kDSIDSize + Object::kIDLenSize], obj_id,
         obj_id_len);

  helpers::tcp_write_until(remote_slave, req,
                           kOpcodeSize + Object::kDSIDSize +
                               Object::kIDLenSize + obj_id_len);

  helpers::tcp_read_until(remote_slave, data_len, sizeof(*data_len));
  if (*data_len) {
    helpers::tcp_read_until(remote_slave, data_buf, *data_len);
  }

  Stats::finish_measure_read_object_cycles();
}

// Request:
// |Opcode = KOpWriteObject (1B)|ds_id(1B)|obj_id_len(1B)|data_len(2B)|
// |obj_id(obj_id_len B)|data_buf(data_len)|
// Response:
// |Ack (1B)|
void TCPDevice::_write_object(tcpconn_t *remote_slave, uint8_t ds_id,
                              uint8_t obj_id_len, const uint8_t *obj_id,
                              uint16_t data_len, const uint8_t *data_buf) {
  Stats::start_measure_write_object_cycles();

  uint8_t req[kOpcodeSize + Object::kDSIDSize + Object::kIDLenSize +
              Object::kDataLenSize + Object::kMaxObjectIDSize + kLargeDataSize];

  __builtin_memcpy(&req[0], &kOpWriteObject, sizeof(kOpWriteObject));
  __builtin_memcpy(&req[kOpcodeSize], &ds_id, Object::kDSIDSize);
  __builtin_memcpy(&req[kOpcodeSize + Object::kDSIDSize], &obj_id_len,
                   Object::kIDLenSize);
  __builtin_memcpy(&req[kOpcodeSize + Object::kDSIDSize + Object::kIDLenSize],
                   &data_len, Object::kDataLenSize);
  memcpy(&req[kOpcodeSize + Object::kDSIDSize + Object::kIDLenSize +
              Object::kDataLenSize],
         obj_id, obj_id_len);

  if (likely(data_len <= kLargeDataSize)) {
    memcpy(&req[kOpcodeSize + Object::kDSIDSize + Object::kIDLenSize +
                Object::kDataLenSize + obj_id_len],
           data_buf, data_len);
    helpers::tcp_write_until(remote_slave, req,
                             kOpcodeSize + Object::kDSIDSize +
                                 Object::kIDLenSize + Object::kDataLenSize +
                                 obj_id_len + data_len);
  } else {
    helpers::tcp_write2_until(remote_slave, req,
                              kOpcodeSize + Object::kDSIDSize +
                                  Object::kIDLenSize + Object::kDataLenSize +
                                  obj_id_len,
                              data_buf, data_len);
  }

  uint8_t ack;
  helpers::tcp_read_until(remote_slave, &ack, sizeof(ack));

  Stats::finish_measure_write_object_cycles();
}

// Request:
// |Opcode = kOpRemoveObject (1B)|ds_id(1B)|obj_id_len(1B)|obj_id(obj_id_len B)|
// Response:
// |exists (1B)|
bool TCPDevice::_remove_object(tcpconn_t *remote_slave, uint64_t ds_id,
                               uint8_t obj_id_len, const uint8_t *obj_id) {

  uint8_t req[kOpcodeSize + Object::kDSIDSize + Object::kIDLenSize +
              Object::kMaxObjectIDSize];

  __builtin_memcpy(&req[0], &kOpRemoveObject, sizeof(kOpRemoveObject));
  __builtin_memcpy(&req[kOpcodeSize], &ds_id, Object::kDSIDSize);
  __builtin_memcpy(&req[kOpcodeSize + Object::kDSIDSize], &obj_id_len,
                   Object::kIDLenSize);
  memcpy(&req[kOpcodeSize + Object::kDSIDSize + Object::kIDLenSize], obj_id,
         obj_id_len);

  helpers::tcp_write_until(remote_slave, req,
                           kOpcodeSize + Object::kDSIDSize +
                               Object::kIDLenSize + obj_id_len);

  bool exists;
  helpers::tcp_read_until(remote_slave, &exists, sizeof(exists));

  return exists;
}

// Request:
// |Opcode = kOpConstruct (1B)|ds_type(1B)|ds_id(1B)|
// |param_len(1B)|params(param_len B)|
// Response:
// |Ack (1B)|
void TCPDevice::_construct(tcpconn_t *remote_slave, uint8_t ds_type,
                           uint8_t ds_id, uint8_t param_len, uint8_t *params) {
  uint8_t req[kOpcodeSize + sizeof(ds_type) + Object::kDSIDSize +
              sizeof(param_len) +
              std::numeric_limits<decltype(param_len)>::max()];

  __builtin_memcpy(&req[0], &kOpConstruct, sizeof(kOpConstruct));
  __builtin_memcpy(&req[kOpcodeSize], &ds_type, sizeof(ds_type));
  __builtin_memcpy(&req[kOpcodeSize + sizeof(ds_type)], &ds_id,
                   Object::kDSIDSize);
  __builtin_memcpy(&req[kOpcodeSize + sizeof(ds_type) + Object::kDSIDSize],
                   &param_len, sizeof(param_len));

  memcpy(&req[kOpcodeSize + sizeof(ds_type) + Object::kDSIDSize +
              sizeof(param_len)],
         params, param_len);
  helpers::tcp_write_until(remote_slave, req,
                           kOpcodeSize + sizeof(ds_type) + Object::kDSIDSize +
                               sizeof(param_len) + param_len);

  uint8_t ack;
  helpers::tcp_read_until(remote_slave, &ack, sizeof(ack));
}

// Request:
// |Opcode = kOpDeconstruct (1B)|ds_id(1B)|
// Response:
// |Ack (1B)|
void TCPDevice::_destruct(tcpconn_t *remote_slave, uint8_t ds_id) {
  uint8_t req[kOpcodeSize + Object::kDSIDSize];

  __builtin_memcpy(&req[0], &kOpDeconstruct, sizeof(kOpDeconstruct));
  __builtin_memcpy(&req[kOpcodeSize], &ds_id, Object::kDSIDSize);

  helpers::tcp_write_until(remote_slave, req, kOpcodeSize + Object::kDSIDSize);

  uint8_t ack;
  helpers::tcp_read_until(remote_slave, &ack, sizeof(ack));
}

// Request:
// |Opcode = kOpCompute(1B)|ds_id(1B)|opcode(1B)|input_len(2B)|
// |input_buf(input_len)|
// Response:
// |output_len(2B)|output_buf(output_len B)|
void TCPDevice::_compute(tcpconn_t *remote_slave, uint8_t ds_id, uint8_t opcode,
                         uint16_t input_len, const uint8_t *input_buf,
                         uint16_t *output_len, uint8_t *output_buf) {
  assert(input_len <= kMaxComputeDataLen);
  uint8_t req[kOpcodeSize + Object::kDSIDSize + sizeof(opcode) +
              +sizeof(input_len) + kLargeDataSize];

  __builtin_memcpy(&req[0], &kOpCompute, sizeof(kOpWriteObject));
  __builtin_memcpy(&req[kOpcodeSize], &ds_id, Object::kDSIDSize);
  __builtin_memcpy(&req[kOpcodeSize + Object::kDSIDSize], &opcode,
                   sizeof(opcode));
  __builtin_memcpy(&req[kOpcodeSize + Object::kDSIDSize + sizeof(opcode)],
                   &input_len, sizeof(input_len));

  if (likely(input_len <= kLargeDataSize)) {
    memcpy(&req[kOpcodeSize + Object::kDSIDSize + sizeof(opcode) +
                sizeof(input_len)],
           input_buf, input_len);
    helpers::tcp_write_until(remote_slave, req,
                             kOpcodeSize + Object::kDSIDSize + sizeof(opcode) +
                                 sizeof(input_len) + input_len);
  } else {
    helpers::tcp_write2_until(remote_slave, req,
                              kOpcodeSize + Object::kDSIDSize + sizeof(opcode) +
                                  sizeof(input_len),
                              input_buf, input_len);
  }

  helpers::tcp_read_until(remote_slave, output_len, sizeof(*output_len));
  if (*output_len) {
    assert(*output_len <= kMaxComputeDataLen);
    helpers::tcp_read_until(remote_slave, output_buf, *output_len);
  }
}

} // namespace far_memory
