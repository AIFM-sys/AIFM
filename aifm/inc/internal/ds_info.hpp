#pragma once

#include <cstdint>

namespace far_memory {

// Global.
constexpr static uint8_t kMaxNumDSIDs = 255;
constexpr static uint8_t kMaxNumDSTypes = 255;

// Vanilla ptr.
constexpr static uint8_t kVanillaPtrDSType = 0;
constexpr static uint8_t kVanillaPtrDSID = 0; // Reserve 0 as its fixed DS ID.
constexpr static uint32_t kVanillaPtrObjectIDSize =
    sizeof(uint64_t); // Its object ID is always the remote object addr.

// Hashtable.
constexpr static uint8_t kHashTableDSType = 1;

// DataFrameVector.
constexpr static uint8_t kDataFrameVectorDSType = 2;

} // namespace far_memory
