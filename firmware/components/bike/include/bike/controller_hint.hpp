#pragma once
#include "integration.hpp"
#include <algorithm>
#include <array>
#include <optional>
#include <span>

namespace bike
{

struct ControllerHint {
  int route = -1;
  std::array<uint8_t, 12> uuid{};
  bool operator==(const ControllerHint &) const = default;
};

struct HintContext {
  uint8_t transport = 0; // BLE, CAN, UART.
  IntegrationId selection = IntegrationId::Auto;
  std::array<uint8_t, 7> peer{}; // BLE address type followed by its six address bytes; zero for wired.
  bool operator==(const HintContext &) const = default;
};

struct HintRecord {
  HintContext context;
  ControllerHint controller;
};

constexpr size_t controller_hint_slots = 8;
using HintBytes = std::array<uint8_t, 23>;

inline bool valid_hint(const HintRecord &record) {
  const auto &context = record.context;
  const auto &hint = record.controller;
  if (context.transport > 2 ||
      (context.selection != IntegrationId::Auto && context.selection != IntegrationId::Vesc &&
       context.selection != IntegrationId::Wfb && context.selection != IntegrationId::WheelE) ||
      hint.route < -1 || hint.route > 254 || (context.transport == 1 && hint.route < 0))
    return false;
  if (context.transport == 0) {
    if (context.peer[0] > 3 ||
        std::all_of(context.peer.begin() + 1, context.peer.end(), [](auto byte) { return byte == 0; }) ||
        std::all_of(context.peer.begin() + 1, context.peer.end(), [](auto byte) { return byte == 255; }))
      return false;
  }
  else if (std::any_of(context.peer.begin(), context.peer.end(), [](auto byte) { return byte != 0; }))
    return false;
  return !std::all_of(hint.uuid.begin(), hint.uuid.end(), [](auto byte) { return byte == 0; }) &&
         !std::all_of(hint.uuid.begin(), hint.uuid.end(), [](auto byte) { return byte == 255; });
}

inline HintBytes encode_hint(const HintRecord &record) {
  HintBytes bytes{};
  if (!valid_hint(record))
    return bytes;
  bytes[0] = 1;
  bytes[1] = record.context.transport;
  bytes[2] = uint8_t(record.context.selection);
  std::copy(record.context.peer.begin(), record.context.peer.end(), bytes.begin() + 3);
  bytes[10] = uint8_t(record.controller.route + 1);
  std::copy(record.controller.uuid.begin(), record.controller.uuid.end(), bytes.begin() + 11);
  return bytes;
}

inline std::optional<HintRecord> decode_hint(std::span<const uint8_t> bytes) {
  if (bytes.size() != HintBytes{}.size() || bytes[0] != 1)
    return std::nullopt;
  HintRecord record;
  record.context.transport = bytes[1];
  record.context.selection = static_cast<IntegrationId>(bytes[2]);
  std::copy_n(bytes.begin() + 3, 7, record.context.peer.begin());
  record.controller.route = int(bytes[10]) - 1;
  std::copy_n(bytes.begin() + 11, 12, record.controller.uuid.begin());
  return valid_hint(record) ? std::optional(record) : std::nullopt;
}

} // namespace bike
