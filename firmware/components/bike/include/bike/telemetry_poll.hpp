#pragma once
#include "polling.hpp"
#include "proxy_poll.hpp"
#include <algorithm>
#include <optional>

namespace bike
{

constexpr bool relay_advertising_hold(uint64_t now, uint64_t discovery_started, bool ready, uint64_t ready_at) {
  return ready ? now < ready_at + 300 : now < discovery_started + 10000;
}

constexpr bool proxy_setup_poll_needed(uint64_t now, uint64_t passive_setup_ms) {
  // Two missed target samples leave room for arbitration and radio latency.
  return !passive_setup_ms || now - passive_setup_ms >= 2 * telemetry_poll_interval_ms;
}

inline std::optional<uint8_t> setup_controller_identity(std::span<const uint8_t> payload) {
  // Controller ID is the first optional field after the complete setup body.
  if (payload.size() < 57 || payload[0] != 47 || payload[56] == 255)
    return std::nullopt;
  return payload[56];
}

// A delayed phone reply must not replace the selected controller after handoff.
// Historical replies without an ID still use the transport/session boundary.
inline bool setup_reply_matches_controller(std::span<const uint8_t> payload, int expected_id) {
  const auto id = setup_controller_identity(payload);
  return !id || expected_id < 0 || expected_id > 254 || *id == expected_id;
}

inline bool proxy_setup_matches_controller(std::span<const uint8_t> payload, int expected_id, bool native_can,
                                           int sender, int destination) {
  const auto id = setup_controller_identity(payload);
  return id && expected_id >= 0 && *id == expected_id && (!native_can || sender == destination);
}

inline std::optional<ProxyReplyKey> telemetry_poll_key(std::span<const uint8_t> payload) {
  constexpr std::array<uint8_t, 4> wfb{36, 102, 10, 1};
  constexpr std::array<uint8_t, 12> wheel_e{36, 101, 33, 1, 0, 0x0a, 0, 2, 0, 0, 0, 0};
  const auto equal = [&](auto expected) {
    return payload.size() == expected.size() && std::equal(payload.begin(), payload.end(), expected.begin());
  };
  if (!((payload.size() == 1 && (payload[0] == 47 || payload[0] == 96)) || equal(wfb) || equal(wheel_e)))
    return std::nullopt;
  ProxyReplyKey key;
  key.size = payload[0] == 36 ? 4 : 1;
  std::copy_n(payload.begin(), key.size, key.bytes.begin());
  return key;
}

class TelemetryPollCredits {
  struct Entry {
    ProxyReplyKey key;
    uint64_t deadline = 0;
  };

  std::array<Entry, 8> entries_{};

public:

  void reset() {
    entries_ = {};
  }

  bool available(ProxyReplyKey key, uint64_t now) const {
    for (const auto &entry : entries_)
      if (entry.key == key)
        return now >= entry.deadline;
    for (const auto &entry : entries_)
      if (!entry.key.size)
        return true;
    return false;
  }

  void sent(ProxyReplyKey key, uint64_t now) {
    for (auto &entry : entries_)
      if (entry.key == key) {
        entry.deadline = now + 1000;
        return;
      }
    for (auto &entry : entries_)
      if (!entry.key.size) {
        entry = {key, now + 1000};
        return;
      }
  }

  void reply(std::span<const uint8_t> payload) {
    for (auto &entry : entries_)
      if (entry.key.matches(payload))
        entry.deadline = 0;
  }
};

} // namespace bike
