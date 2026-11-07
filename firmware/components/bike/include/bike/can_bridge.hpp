#pragma once
#include "can_packet.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace bike
{

enum class BridgeOperation {
  Firmware,
  FirmwareInfo,
  Scan,
  Forward,
  RawFrame,
  Ignore,
  Invalid,
  Unsupported
};

struct BridgeRequest {
  BridgeOperation operation = BridgeOperation::Unsupported;
  int destination = -1;
  std::span<const uint8_t> payload;
  CanFrame frame{};
};

// Adapter semantics, independent of every vehicle integration/controller route.
inline BridgeRequest can_bridge_request(std::span<const uint8_t> packet, uint8_t local) {
  if (packet.size() == 1 && packet[0] == 0)
    return {BridgeOperation::Firmware, -1, {}, {}};
  if (packet.size() == 1 && packet[0] == 157)
    return {BridgeOperation::FirmwareInfo, -1, {}, {}};
  if (packet.size() == 1 && packet[0] == 30)
    return {BridgeOperation::Ignore, -1, {}, {}};
  if (packet.size() == 1 && packet[0] == 62)
    return {BridgeOperation::Scan, -1, {}, {}};
  if (packet.size() >= 3 && packet[0] == 34 && packet[1] != 255 && packet[1] != local)
    return {BridgeOperation::Forward, packet[1], packet.subspan(2), {}};
  if (!packet.empty() && packet[0] == 85) {
    BridgeRequest result;
    result.operation = BridgeOperation::Invalid;
    if (packet.size() < 6 || packet.size() > 14 || packet[5] > 1)
      return result;
    result.frame.id = uint32_t(packet[1]) << 24 | uint32_t(packet[2]) << 16 | uint32_t(packet[3]) << 8 | packet[4];
    result.frame.extended = packet[5] != 0;
    result.frame.size = packet.size() - 6;
    std::copy(packet.begin() + 6, packet.end(), result.frame.data.begin());
    if (valid_can_data_frame(result.frame))
      result.operation = BridgeOperation::RawFrame;
    return result;
  }
  return {};
}

// These bytes describe VESC protocol compatibility, not the display's release.
inline constexpr uint8_t bridge_protocol_major = 7, bridge_protocol_minor = 0;

inline size_t can_bridge_firmware(std::span<uint8_t> output, const std::array<uint8_t, 12> &uuid,
                                  std::string_view hardware = "Wheelie_Fun_Display", std::string_view version = {}) {
  constexpr std::string_view firmware = "Wheelie Fun Display";
  hardware = hardware.substr(0, 64);
  version = version.substr(0, 31);
  const size_t required =
      3 + hardware.size() + 1 + uuid.size() + 8 + firmware.size() + (version.empty() ? 0 : 2 + version.size()) + 1 + 4;
  if (output.size() < required)
    return 0;
  size_t i = 0;
  output[i++] = 0;
  output[i++] = bridge_protocol_major;
  output[i++] = bridge_protocol_minor;
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
  if (!version.empty()) {
    output[i++] = ' ';
    output[i++] = 'v';
    for (auto c : version)
      output[i++] = c;
  }
  output[i++] = 0;
  for (int field = 0; field < 4; ++field)
    output[i++] = 0; // Hardware configuration CRC.
  return i;
}

inline size_t can_bridge_firmware_info(std::span<uint8_t> output, std::string_view source_revision,
                                       std::string_view user_revision = {}) {
  source_revision = source_revision.substr(0, 46);
  user_revision = user_revision.substr(0, 46);
  const auto required = 6 + source_revision.size() + user_revision.size();
  if (output.size() < required)
    return 0;
  size_t i = 0;
  output[i++] = 157;
  output[i++] = bridge_protocol_major;
  output[i++] = bridge_protocol_minor;
  output[i++] = 0; // Test firmware number, matching COMM_FW_VERSION.
  for (const auto text : {source_revision, user_revision}) {
    for (const char c : text)
      output[i++] = c;
    output[i++] = 0;
  }
  return i;
}

} // namespace bike
