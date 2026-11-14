#pragma once
#include "bike/telemetry.hpp"
#include "config.h"
#include <cmath>
#include <cstdint>
#include <limits>

#if WFD_TELEMETRY_TEST
namespace telemetry_test
{

// Absolute 60 Hz slots avoid drift and never replay missed samples in a burst.
struct Clock {
  uint64_t slot = std::numeric_limits<uint64_t>::max();
  uint64_t updates = 0, skipped = 0;

  bool poll(uint64_t now_us) {
    const auto current = now_us * 60 / 1000000;
    if (current == slot)
      return false;
    if (updates)
      skipped += current - slot - 1;
    slot = current;
    ++updates;
    return true;
  }

  uint64_t next_us() const {
    return ((slot + 1) * 1000000 + 59) / 60;
  }
};

inline bike::Telemetry sample(uint64_t now_us) {
  const double seconds = now_us / 1000000.;
  const float wave = std::sin(seconds * 1.7);
  const float slow = std::sin(seconds * .43);
  bike::Telemetry data;
  data.setup_valid = data.bike_valid = true;
  data.setup_ms = data.bike_ms = now_us / 1000;
  data.speed_mps = 8 + 7 * wave;
  data.voltage = 74 + 6 * slow;
  data.battery_valid = true;
  data.battery_percent = 65 + 30 * slow;
  data.battery_current = 30 + 50 * wave;
  data.motor_current = 70 + 100 * wave;
  data.mosfet_c = 50 + 15 * wave;
  data.motor_c = 55 + 15 * slow;
  data.pitch_deg = 20 * wave;
  data.roll_deg = 15 * slow;
  data.duty_cycle = .5f + .4f * wave;
  data.erpm = 15000 + 12000 * wave;
  data.amp_hours = 2 + seconds / 120;
  data.amp_hours_charged = .1 + seconds / 1200;
  data.watt_hours = 150 + seconds * .6;
  data.watt_hours_charged = 8 + seconds * .06;
  data.distance_m = data.distance_abs_m = 12340 + seconds * 8;
  data.odometer_m = 1234560 + seconds * 8;
  data.odometer_valid = true;
  data.pid_position_deg = 180 * wave;
  data.controller_id = 1;
  data.controller_count = 1;
  data.controller_id_valid = data.controller_count_valid = true;
  data.remaining_wh = 1200 * data.battery_percent / 100;
  data.remaining_wh_valid = data.uptime_valid = true;
  data.uptime_ms = now_us / 1000;
  auto &bms = data.bms;
  bms.available = bms.soc_valid = bms.soh_valid = true;
  bms.received_ms = now_us / 1000;
  bms.pack_voltage = data.voltage;
  bms.charge_voltage = 84;
  bms.input_current = bms.input_current_ic = data.battery_current;
  bms.amp_hours = data.amp_hours;
  bms.watt_hours = data.watt_hours;
  bms.soc = data.battery_percent / 100;
  bms.soh = .98f;
  bms.cell_count = 20;
  bms.temp_count = 4;
  for (unsigned i = 0; i < bms.cell_count; ++i)
    bms.cell_volts[i] = data.voltage / 20 + .015f * std::sin(seconds * 2 + i);
  for (unsigned i = 0; i < bms.temp_count; ++i)
    bms.temps_c[i] = 30 + i + 5 * wave;
  bms.ic_c = bms.max_cell_c = 35 + 5 * wave;
  bms.humidity_sensor_c = 30;
  bms.humidity_percent = 45 + 5 * slow;
  return data;
}

} // namespace telemetry_test
#endif
