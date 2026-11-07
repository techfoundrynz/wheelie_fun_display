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

  size_t remaining() const {
    return p < b.size() ? b.size() - p : 0;
  }

  float auto_float() {
    const auto bits = u32();
    int exponent = (bits >> 23) & 255;
    const auto mantissa = bits & 0x7fffff;
    float significand = 0;
    if (exponent != 0 || mantissa != 0) {
      significand = float(mantissa) / 16777216.f + .5f;
      exponent -= 126;
    }
    if (bits & (1u << 31))
      significand = -significand;
    return std::ldexp(significand, exponent);
  }

  void skip(size_t n) {
    for (size_t i = 0; i < n; ++i)
      u8();
  }
};

bool decode_bms(Reader &r, uint64_t now, BmsTelemetry &out) {
  BmsTelemetry bms;
  bms.pack_voltage = r.i32() / 1e6f;
  bms.charge_voltage = r.i32() / 1e6f;
  bms.input_current = r.i32() / 1e6f;
  bms.input_current_ic = r.i32() / 1e6f;
  bms.amp_hours = r.i32() / 1e3f;
  bms.watt_hours = r.i32() / 1e3f;
  bms.cell_count = r.u8();
  if (bms.cell_count > BmsTelemetry::max_cells)
    return false;
  for (size_t i = 0; i < bms.cell_count; ++i) {
    bms.cell_volts[i] = r.i16() / 1e3f;
    if (bms.cell_volts[i] < 0)
      return false;
  }
  for (size_t i = 0; i < bms.cell_count; ++i) {
    const auto balance = r.u8();
    if (balance > 1)
      return false;
    bms.balancing[i] = balance != 0;
  }
  bms.temp_count = r.u8();
  if (bms.temp_count > BmsTelemetry::max_temps)
    return false;
  for (size_t i = 0; i < bms.temp_count; ++i)
    bms.temps_c[i] = r.i16() / 1e2f;
  bms.ic_c = r.i16() / 1e2f;
  bms.humidity_sensor_c = r.i16() / 1e2f;
  bms.humidity_percent = r.i16() / 1e2f;
  bms.max_cell_c = r.i16() / 1e2f;
  if (!r.ok || bms.pack_voltage < 0 || bms.charge_voltage < 0 || bms.humidity_percent < 0 || bms.humidity_percent > 100)
    return false;
  // Accept complete historical prefixes, never partial scalar/group fields.
  if (r.remaining()) {
    if (r.remaining() < 2)
      return false;
    bms.soc = r.i16() / 1e3f;
    bms.soc_valid = true;
    if (bms.soc < 0 || bms.soc > 1)
      return false;
  }
  if (r.remaining()) {
    if (r.remaining() < 2)
      return false;
    bms.soh = r.i16() / 1e3f;
    bms.soh_valid = true;
    if (bms.soh < 0 || bms.soh > 1)
      return false;
  }
  if (r.remaining()) {
    bms.can_id = r.u8();
    bms.can_id_valid = bms.can_id <= 254;
  }
  if (r.remaining()) {
    if (r.remaining() < 16)
      return false;
    bms.lifetime_ah_charged_total = r.auto_float();
    bms.lifetime_wh_charged_total = r.auto_float();
    bms.lifetime_ah_discharged_total = r.auto_float();
    bms.lifetime_wh_discharged_total = r.auto_float();
    for (float value : {bms.lifetime_ah_charged_total, bms.lifetime_wh_charged_total, bms.lifetime_ah_discharged_total,
                        bms.lifetime_wh_discharged_total})
      if (!std::isfinite(value) || value < 0)
        return false;
    bms.lifetime_valid = true;
  }
  if (r.remaining()) {
    if (r.remaining() < 2)
      return false;
    bms.pressure_pa = r.i16() / .1f;
    if (bms.pressure_pa < 0)
      return false;
    bms.pressure_valid = true;
  }
  if (r.remaining()) {
    bms.data_version = r.u8();
    bms.data_version_valid = true;
  }
  if (r.remaining()) {
    if (r.remaining() > bms.status.size())
      return false;
    bool terminated = false;
    for (size_t i = 0; i < bms.status.size() && r.remaining(); ++i) {
      const auto character = r.u8();
      if (!character) {
        terminated = true;
        break;
      }
      // Status must remain safe UTF-8 text even when a device sends control/binary bytes.
      bms.status[i] = character >= 32 && character <= 126 ? char(character) : '?';
    }
    if (!terminated || r.remaining())
      return false;
    bms.status_valid = true;
  }
  if (!r.ok)
    return false;
  bms.available = bms.pack_voltage > 0 || bms.cell_count > 0 || bms.temp_count > 0 || bms.can_id_valid;
  bms.received_ms = now;
  out = bms;
  return true;
}

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
    t.duty_cycle = r.i16() / 1e3f;
    t.erpm = r.i32();
    t.speed_mps = r.i32() / 1000.f; // upstream buffer_append_float32(speed, 1e3)
    t.voltage = r.i16() / 10.f;
    t.battery_percent = r.i16() / 10.f;
    t.amp_hours = r.i32() / 1e4f;
    t.amp_hours_charged = r.i32() / 1e4f;
    t.watt_hours = r.i32() / 1e4f;
    t.watt_hours_charged = r.i32() / 1e4f;
    t.distance_m = r.i32() / 1e3;
    const auto distance_abs_mm = r.i32();
    t.distance_abs_m = distance_abs_mm / 1000.0;
    t.pid_position_deg = r.i32() / 1e6f;
    t.fault = r.u8();
    if (!r.ok || distance_abs_mm < 0 || t.voltage < 0 || t.voltage > 200 || std::abs(t.speed_mps) > 150 ||
        std::abs(t.duty_cycle) > 1)
      return false;
    // VESC battery estimates depend on battery configuration and are not clamped
    // upstream. An unusable estimate must not discard valid motor/fault telemetry.
    t.battery_valid = t.battery_percent >= 0 && t.battery_percent <= 100;
    // Older replies end before the optional controller/energy/odometer/uptime
    // fields. Accept complete older prefixes, but never partial scalar fields.
    const auto tail = bytes.size() - r.p;
    if ((tail > 2 && tail < 6) || (tail > 6 && tail < 10) || (tail > 10 && tail < 14))
      return false;
    t.controller_id_valid = tail >= 1;
    t.controller_count_valid = tail >= 2;
    t.remaining_wh_valid = tail >= 6;
    t.odometer_valid = tail >= 10;
    t.uptime_valid = tail >= 14;
    t.controller_id = t.controller_id_valid ? r.u8() : 255;
    t.controller_id_valid &= t.controller_id <= 254;
    t.controller_count = t.controller_count_valid ? r.u8() : 0;
    t.remaining_wh = t.remaining_wh_valid ? r.i32() / 1e3f : 0;
    t.remaining_wh_valid &= t.remaining_wh >= 0;
    t.odometer_m = t.odometer_valid ? r.u32() : 0;
    t.uptime_ms = t.uptime_valid ? r.u32() : 0;
    t.setup_valid = true;
    t.setup_ms = now;
  }
  else if (cmd == 96) {
    if (!decode_bms(r, now, t.bms))
      return false;
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
