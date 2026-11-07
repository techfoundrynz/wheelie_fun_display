#pragma once
#include "telemetry.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>

namespace bike
{

constexpr bool valid_pin(std::string_view pin) {
  if (pin.size() != 6)
    return false;
  for (char digit : pin)
    if (digit < '0' || digit > '9')
      return false;
  return true;
}

// Numeric BLE passkeys are 0..999999; fewer entered digits imply leading zeroes.
constexpr bool valid_ble_pin(std::string_view pin) {
  if (pin.empty() || pin.size() > 6)
    return false;
  for (char digit : pin)
    if (digit < '0' || digit > '9')
      return false;
  return true;
}

inline bool append_pattern(std::string &path, int node) {
  if (node < 1 || node > 9 || path.size() >= 6 || path.find(char('0' + node)) != std::string::npos)
    return false;
  if (!path.empty()) {
    const int previous = path.back() - '1';
    const int next = node - 1;
    const int x = previous % 3 + next % 3, y = previous / 3 + next / 3;
    if (x % 2 == 0 && y % 2 == 0) {
      const char middle = char('1' + y / 2 * 3 + x / 2);
      if (path.find(middle) == std::string::npos) {
        if (path.size() == 5)
          return false;
        path.push_back(middle);
      }
    }
  }
  path.push_back(char('0' + node));
  return true;
}

inline bool valid_pattern(std::string_view path) {
  if (path.size() != 6)
    return false;
  std::string canonical;
  for (char node : path)
    if (!append_pattern(canonical, node - '0'))
      return false;
  return canonical == path;
}

struct LockRecord {
  uint8_t version = 2;
  std::array<uint8_t, 16> salt{};
  std::array<uint8_t, 32> digest{};
  uint8_t armed = 0;
  uint8_t timeout = 30;
  uint8_t failures = 0;
  // Initial entry mode for records written before the separate entry_mode preference.
  uint8_t pattern = 0;
};

static_assert(sizeof(LockRecord) == 53);

inline bool decode_lock_record(std::span<const uint8_t> bytes, LockRecord &record) {
  if (bytes.size() != 52 && bytes.size() != sizeof(LockRecord))
    return false;
  LockRecord decoded;
  std::memcpy(&decoded, bytes.data(), bytes.size());
  if ((bytes.size() == 52 && decoded.version != 1) || (decoded.version != 1 && decoded.version != 2) ||
      decoded.pattern > 1 || (decoded.version == 1 && decoded.pattern) || decoded.armed > 1 || decoded.failures > 5 ||
      (decoded.timeout != 15 && decoded.timeout != 30 && decoded.timeout != 60 && decoded.timeout != 120))
    return false;
  record = decoded;
  return true;
}

inline bool can_lock(const Telemetry &t, uint64_t now) {
  const bool stopped = t.state == 0 || (t.state >= 6 && t.state <= 9) || (t.state >= 11 && t.state <= 13) ||
                       t.state == 15 || t.state == 16;
  return t.can_configure(now) && stopped && std::abs(t.motor_current) < 2.f;
}

struct PinAttempts {
  unsigned failures = 0;
  uint64_t retry_at = 0;

  bool allowed(uint64_t now) const {
    return now >= retry_at;
  }

  void result(bool correct, uint64_t now) {
    if (correct) {
      failures = 0;
      retry_at = 0;
    }
    else if (++failures >= 5) {
      retry_at = now + 30000;
    }
  }
};

// Confirmation is tied to the same link and a sample received after the write.
struct LockRequest {
  bool pending = false, desired = false;
  uint32_t session = 0;
  uint64_t since = 0;

  bool confirmed(const Telemetry &t, uint32_t link, uint64_t now) const {
    return pending && session == link && can_lock(t, now) && t.bike_ms > since + 400 && now - t.bike_ms < 1500 &&
           (t.state == 15) == desired;
  }
};

} // namespace bike
