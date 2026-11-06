#pragma once
#include "telemetry.hpp"
#include <cmath>
#include <cstdint>
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
