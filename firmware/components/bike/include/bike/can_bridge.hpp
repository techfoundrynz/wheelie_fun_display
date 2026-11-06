#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace bike
{

enum class BridgeOperation {
  Firmware,
  Scan,
  Forward,
  Unsupported
};

struct BridgeRequest {
  BridgeOperation operation = BridgeOperation::Unsupported;
  int destination = -1;
  std::span<const uint8_t> payload;
};

// Adapter semantics, independent of every vehicle integration/controller route.
inline BridgeRequest can_bridge_request(std::span<const uint8_t> packet, uint8_t local) {
  if (packet.size() == 1 && packet[0] == 0)
    return {BridgeOperation::Firmware, -1, {}};
  if (packet.size() == 1 && packet[0] == 62)
    return {BridgeOperation::Scan, -1, {}};
  if (packet.size() >= 3 && packet[0] == 34 && packet[1] != 255 && packet[1] != local)
    return {BridgeOperation::Forward, packet[1], packet.subspan(2)};
  return {};
}

inline size_t can_bridge_firmware(std::span<uint8_t> output, const std::array<uint8_t, 12> &uuid) {
  constexpr std::string_view hardware = "Bike_CAN_Bridge", firmware = "Bike display";
  const size_t required = 3 + hardware.size() + 1 + uuid.size() + 8 + firmware.size() + 1 + 4;
  if (output.size() < required)
    return 0;
  size_t i = 0;
  output[i++] = 0;
  output[i++] = 6;
  output[i++] = 6;
  for (auto c : hardware)
    output[i++] = c;
  output[i++] = 0;
  for (auto b : uuid)
    output[i++] = b;
  output[i++] = 0; // Pairing done.
  output[i++] = 0; // Test firmware.
  output[i++] = 2; // HW_TYPE_CUSTOM_MODULE, never a motor controller.
  for (int field = 0; field < 5; ++field)
    output[i++] = 0; // Config/UI/filter/NRF capabilities.
  for (auto c : firmware)
    output[i++] = c;
  output[i++] = 0;
  for (int field = 0; field < 4; ++field)
    output[i++] = 0; // Hardware configuration CRC.
  return i;
}

} // namespace bike
