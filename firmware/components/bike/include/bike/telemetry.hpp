#pragma once
#include <cstdint>
#include <span>

namespace bike
{

struct Telemetry {
  float speed_mps = 0, voltage = 0, battery_percent = 0;
  float motor_current = 0, battery_current = 0, mosfet_c = 0, motor_c = 0;
  float pitch_deg = 0, roll_deg = 0;
  uint8_t fault = 0, state = 0;
  bool setup_valid = false, bike_valid = false;
  uint64_t setup_ms = 0, bike_ms = 0;

  bool fresh(uint64_t now) const {
    return setup_valid && now - setup_ms < 1500;
  }

  bool can_configure(uint64_t now) const;

};

// Transactional: invalid/truncated replies leave the last valid sample untouched.

bool decode_telemetry(std::span<const uint8_t> payload, uint64_t now, Telemetry &state);

} // namespace bike
