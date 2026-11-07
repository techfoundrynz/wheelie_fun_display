#pragma once
#include <cstdint>

namespace bike
{

// Target each telemetry family at 10 Hz (up to 20 VESC requests/s for setup
// plus integration data). The measured BLE link sustained about 29 replies/s.
// Outstanding-request credits and phone priority take precedence over cadence.
inline constexpr uint32_t telemetry_poll_hz = 10;
inline constexpr uint32_t telemetry_poll_interval_ms = 1000 / telemetry_poll_hz;
inline constexpr uint32_t bms_poll_interval_ms = 1000;

} // namespace bike
