#include "bike/telemetry.hpp"
#include <cmath>

namespace bike
{
namespace
{

struct Reader {
  std::span<const uint8_t> b;
  size_t p = 0;
  bool ok = true;

  uint8_t u8() {
    if (p >= b.size()) {
      ok = false;
      return 0;
    }
    return b[p++];
  }

  uint16_t u16() {
    auto hi = u8();
    return (uint16_t(hi) << 8) | u8();
  }

  int16_t i16() {
    return static_cast<int16_t>(u16());
  }

  uint32_t u32() {
    auto hi = u16();
    return (uint32_t(hi) << 16) | u16();
  }

  int32_t i32() {
    return static_cast<int32_t>(u32());
  }

  void skip(size_t n) {
    for (size_t i = 0; i < n; ++i)
      u8();
  }
};

} // namespace

bool Telemetry::can_configure(uint64_t now) const {
  return fresh(now) && bike_valid && now - bike_ms < 1500 && fault == 0 && std::abs(speed_mps) < .2f;
}

bool decode_telemetry(std::span<const uint8_t> bytes, uint64_t now, Telemetry &out) {
  Reader r{bytes};
  auto t = out;
  const auto cmd = r.u8();
  if (cmd == 47) {
    t.mosfet_c = r.i16() / 10.f;
    t.motor_c = r.i16() / 10.f;
    t.motor_current = r.i32() / 100.f;
    t.battery_current = r.i32() / 100.f;
    r.skip(2 + 4);                  // duty, electrical RPM
    t.speed_mps = r.i32() / 1000.f; // upstream buffer_append_float32(speed, 1e3)
    t.voltage = r.i16() / 10.f;
    t.battery_percent = r.i16() / 10.f;
    r.skip(4 * 4 + 4 * 2 + 4); // energy, distances, PID position
    t.fault = r.u8();
    if (!r.ok || t.battery_percent < 0 || t.battery_percent > 100 || t.voltage < 0 || t.voltage > 200 ||
        std::abs(t.speed_mps) > 150)
      return false;
    t.setup_valid = true;
    t.setup_ms = now;
  }
  else if (cmd == 36) {
    if (r.u8() != 102 || r.u8() != 10)
      return false;
    const auto mode = r.u8();
    if (mode != 1 && mode != 2)
      return false;
    r.skip(2);
    t.pitch_deg = r.i16() / 10.f;
    t.roll_deg = r.i16() / 10.f;
    t.state = r.u8() & 15;
    // Need the complete documented body, including mode-dependent tail.
    constexpr size_t base_body = 45; // mode + 31 bytes prefix + 13 bytes tail
    if (!r.ok || bytes.size() < 3 + base_body + (mode == 2 ? 7 : 0))
      return false;
    t.bike_valid = true;
    t.bike_ms = now;
  }
  else
    return false;
  out = t;
  return true;
}

} // namespace bike
