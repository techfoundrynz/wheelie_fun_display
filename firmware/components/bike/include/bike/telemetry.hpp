#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace bike
{

struct BmsTelemetry {
  static constexpr size_t max_cells = 50, max_temps = 50;
  bool available = false;
  uint64_t received_ms = 0;
  float pack_voltage = 0, charge_voltage = 0, input_current = 0, input_current_ic = 0;
  float amp_hours = 0, watt_hours = 0;
  uint8_t cell_count = 0, temp_count = 0;
  std::array<float, max_cells> cell_volts{};
  std::array<bool, max_cells> balancing{};
  std::array<float, max_temps> temps_c{};
  float ic_c = 0, humidity_sensor_c = 0, humidity_percent = 0, max_cell_c = 0;
  float soc = 0, soh = 0;
  uint8_t can_id = 255;
  bool soc_valid = false, soh_valid = false, can_id_valid = false;
  float lifetime_ah_charged_total = 0, lifetime_wh_charged_total = 0;
  float lifetime_ah_discharged_total = 0, lifetime_wh_discharged_total = 0;
  bool lifetime_valid = false;
  float pressure_pa = 0;
  uint8_t data_version = 0;
  std::array<char, 41> status{};
  bool pressure_valid = false, data_version_valid = false, status_valid = false;

  bool fresh(uint64_t now) const {
    return available && now - received_ms < 5000;
  }
};

struct Telemetry {
  float speed_mps = 0, voltage = 0, battery_percent = 0;
  bool battery_valid = false;
  float motor_current = 0, battery_current = 0, mosfet_c = 0, motor_c = 0;
  float pitch_deg = 0, roll_deg = 0;
  float duty_cycle = 0;
  int32_t erpm = 0;
  float amp_hours = 0, amp_hours_charged = 0, watt_hours = 0, watt_hours_charged = 0;
  double distance_m = 0;
  float pid_position_deg = 0;
  uint8_t controller_id = 255, controller_count = 0;
  float remaining_wh = 0;
  uint32_t uptime_ms = 0;
  bool controller_id_valid = false, controller_count_valid = false, remaining_wh_valid = false, uptime_valid = false;
  BmsTelemetry bms;
  double distance_abs_m = 0;
  uint32_t odometer_m = 0;
  bool odometer_valid = false;
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
