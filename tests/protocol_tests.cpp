#include "bike/ble_name.hpp"
#include "bike/integration.hpp"
#include "bike/lock_policy.hpp"
#include "bike/orientation.hpp"
#include "bike/protocol.hpp"
#include "bike/telemetry.hpp"
#include "bike/units.hpp"
#include "bike/update_policy.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>
#include <vector>

#define CHECK(c)                                                                                                       \
  do {                                                                                                                 \
    if (!(c)) {                                                                                                        \
      std::cerr << __LINE__ << ": " #c "\n";                                                                           \
      std::exit(1);                                                                                                    \
    }                                                                                                                  \
  } while (0)
using Bytes = std::vector<uint8_t>;

Bytes framed(const Bytes &p, int can = -1) {
  Bytes out(bike::max_payload + 6);
  auto n = bike::frame(p, out, can);
  CHECK(n);
  out.resize(n);
  return out;
}

struct Receiver {
  std::vector<Bytes> packets;
  bike::Decoder decoder{[](void *ctx, std::span<const uint8_t> p) {
                          static_cast<Receiver *>(ctx)->packets.emplace_back(p.begin(), p.end());
                        },
                        this};
};

void put16(Bytes &b, int n) {
  b.push_back(uint8_t(n >> 8));
  b.push_back(uint8_t(n));
}

void put32(Bytes &b, int32_t n) {
  put16(b, int(uint32_t(n) >> 16));
  put16(b, n);
}

Bytes setup(int speed = 2500, int fault = 0) {
  Bytes b{47};
  put16(b, 350);
  put16(b, 400);
  put32(b, -1234);
  put32(b, 567);
  put16(b, 100);
  put32(b, 12345);
  put32(b, speed);
  put16(b, 756);
  put16(b, 825);
  for (int i = 0; i < 7; ++i)
    put32(b, 0);
  b.push_back(fault);
  return b;
}

void distance_reporting_tests() {
  auto packet = setup();
  auto write32 = [&](size_t offset, uint32_t value) {
    for (size_t i = 0; i < 4; ++i)
      packet[offset + i] = uint8_t(value >> (24 - i * 8));
  };
  write32(43, uint32_t(-250000)); // Signed displacement can be negative.
  write32(47, 12345678);          // Absolute trip distance, millimetres.
  packet.push_back(42);
  packet.push_back(1);
  put32(packet, 25000);
  put32(packet, std::bit_cast<int32_t>(uint32_t(4000000000u)));
  put32(packet, 90000);
  CHECK(packet.size() == 70);
  bike::Telemetry sample;
  CHECK(bike::decode_telemetry(packet, 100, sample));
  CHECK(std::abs(sample.distance_abs_m - 12345.678) < 1e-8);
  CHECK(sample.odometer_valid && sample.odometer_m == 4000000000u);
  CHECK(bike::distance_in_units(1000, true) == 1);
  CHECK(bike::distance_in_units(1609.344, false) == 1);
  CHECK(bike::distance_in_units(0, false) == 0);
  CHECK(bike::distance_in_units(sample.odometer_m, true) == 4000000);
  for (size_t size = 0; size < packet.size(); ++size) {
    auto copy = sample;
    const bool older_prefix = size == 56 || size == 57 || size == 58 || size == 62 || size == 66;
    CHECK(bike::decode_telemetry(std::span(packet).first(size), 200, copy) == older_prefix);
    if (older_prefix) {
      CHECK(copy.setup_ms == 200 && copy.distance_abs_m == sample.distance_abs_m);
      CHECK(copy.odometer_valid == (size >= 66));
      CHECK(copy.odometer_m == (size >= 66 ? 4000000000u : 0u));
    }
    else {
      CHECK(copy.setup_ms == sample.setup_ms);
      CHECK(copy.distance_abs_m == sample.distance_abs_m && copy.odometer_m == sample.odometer_m);
    }
  }
  packet.push_back(0); // Forward-compatible extension after the complete tail.
  CHECK(bike::decode_telemetry(packet, 300, sample));
  write32(47, uint32_t(-1));
  CHECK(!bike::decode_telemetry(packet, 400, sample));
  CHECK(sample.setup_ms == 300 && sample.odometer_m == 4000000000u);
  CHECK(bike::decode_telemetry(setup(), 500, sample));
  CHECK(!sample.odometer_valid && sample.odometer_m == 0 && sample.distance_abs_m == 0);
}

void write16(Bytes &packet, size_t offset, int value) {
  packet[offset] = uint8_t(value >> 8);
  packet[offset + 1] = uint8_t(value);
}

void write32(Bytes &packet, size_t offset, uint32_t value) {
  for (size_t i = 0; i < 4; ++i)
    packet[offset + i] = uint8_t(value >> (24 - i * 8));
}

void expanded_setup_tests() {
  auto packet = setup();
  write16(packet, 13, -250);
  write32(packet, 15, uint32_t(-12345));
  write32(packet, 27, 12345);
  write32(packet, 31, 23456);
  write32(packet, 35, 34567);
  write32(packet, 39, 45678);
  write32(packet, 43, uint32_t(-250000));
  write32(packet, 47, 12345678);
  write32(packet, 51, uint32_t(-12345678));
  packet.push_back(42);
  packet.push_back(3);
  put32(packet, 250000);
  put32(packet, std::bit_cast<int32_t>(uint32_t(4000000000u)));
  put32(packet, std::bit_cast<int32_t>(uint32_t(4000000123u)));
  bike::Telemetry sample;
  CHECK(bike::decode_telemetry(packet, 100, sample));
  CHECK(sample.duty_cycle == -.25f && sample.erpm == -12345);
  CHECK(std::abs(sample.amp_hours - 1.2345f) < .00001f);
  CHECK(std::abs(sample.amp_hours_charged - 2.3456f) < .00001f);
  CHECK(std::abs(sample.watt_hours - 3.4567f) < .00001f);
  CHECK(std::abs(sample.watt_hours_charged - 4.5678f) < .00001f);
  CHECK(sample.distance_m == -250 && std::abs(sample.distance_abs_m - 12345.678) < 1e-8);
  CHECK(std::abs(sample.pid_position_deg + 12.345678f) < .00001f);
  CHECK(sample.controller_id_valid && sample.controller_id == 42);
  CHECK(sample.controller_count_valid && sample.controller_count == 3);
  CHECK(sample.remaining_wh_valid && sample.remaining_wh == 250);
  CHECK(sample.uptime_valid && sample.uptime_ms == 4000000123u);
  CHECK(sample.odometer_valid && sample.odometer_m == 4000000000u);
  for (size_t size = 0; size < packet.size(); ++size) {
    auto copy = sample;
    const bool prefix = size == 56 || size == 57 || size == 58 || size == 62 || size == 66;
    CHECK(bike::decode_telemetry(std::span(packet).first(size), 200, copy) == prefix);
    if (prefix) {
      CHECK(copy.controller_id_valid == (size >= 57));
      CHECK(copy.controller_count_valid == (size >= 58));
      CHECK(copy.remaining_wh_valid == (size >= 62));
      CHECK(copy.odometer_valid == (size >= 66));
      CHECK(!copy.uptime_valid && copy.uptime_ms == 0);
    }
    else {
      CHECK(copy.setup_ms == 100 && copy.uptime_ms == sample.uptime_ms);
      CHECK(copy.amp_hours == sample.amp_hours && copy.duty_cycle == sample.duty_cycle);
    }
  }
  auto invalid = packet;
  write16(invalid, 13, 1001);
  CHECK(!bike::decode_telemetry(invalid, 300, sample));
  CHECK(sample.setup_ms == 100 && sample.duty_cycle == -.25f);
  invalid = packet;
  write32(invalid, 58, uint32_t(-1));
  CHECK(bike::decode_telemetry(invalid, 300, sample));
  CHECK(sample.setup_ms == 300 && !sample.remaining_wh_valid && sample.remaining_wh < 0);
  invalid = packet;
  invalid[56] = 255;
  CHECK(bike::decode_telemetry(invalid, 300, sample));
  CHECK(!sample.controller_id_valid && sample.controller_count_valid);
  CHECK(bike::decode_telemetry(setup(), 400, sample));
  CHECK(!sample.controller_id_valid && !sample.controller_count_valid && !sample.remaining_wh_valid &&
        !sample.uptime_valid);
}

void battery_estimate_validity_tests() {
  bike::Telemetry sample;
  CHECK(!sample.battery_valid);
  // Captured from a real wheel-e controller: 81.7 V, but an unclamped 411.2%
  // battery estimate. The complete setup frame still contains valid motor data.
  Bytes captured{
      0x2f, 0x01, 0x46, 0xff, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x31, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x12, 0xc6, 0x84,
      0xc0, 0x00, 0x57, 0x01, 0x00, 0x03, 0x8d, 0x83, 0x00, 0x00, 0xb4, 0xdf, 0x00, 0x4d, 0xc2, 0x8a,
  };
  CHECK(bike::decode_telemetry(captured, 100, sample));
  CHECK(sample.fresh(100) && !sample.battery_valid);
  CHECK(std::abs(sample.battery_percent - 411.2f) < .001f);
  CHECK(std::abs(sample.voltage - 81.7f) < .001f);
  CHECK(sample.speed_mps == 0 && sample.motor_current == 0 && sample.battery_current == 0);
  CHECK(sample.fault == 0 && sample.controller_id_valid && sample.controller_id == 87);
  CHECK(sample.remaining_wh_valid && sample.uptime_valid && sample.odometer_valid);
  sample.bike_valid = true;
  sample.bike_ms = 100;
  CHECK(sample.can_configure(100)); // Battery estimates do not determine whether the motor is stationary.
  write32(captured, 19, 2500);
  write32(captured, 5, uint32_t(-1234));
  CHECK(bike::decode_telemetry(captured, 200, sample));
  CHECK(!sample.battery_valid && sample.speed_mps == 2.5f && sample.motor_current == -12.34f);
  CHECK(!sample.can_configure(200));
  write32(captured, 19, 0);
  captured[55] = 1;
  CHECK(bike::decode_telemetry(captured, 300, sample));
  CHECK(!sample.can_configure(300)); // A fault still blocks changes, even at zero speed.
  for (const auto estimate : {-1, 0, 1000, 1001, 32767}) {
    auto packet = setup();
    write16(packet, 25, estimate);
    CHECK(bike::decode_telemetry(packet, 400, sample));
    CHECK(sample.battery_valid == (estimate >= 0 && estimate <= 1000));
    CHECK(sample.fresh(400) && sample.speed_mps == 2.5f);
  }
  CHECK(bike::decode_telemetry(setup(), 500, sample));
  CHECK(sample.battery_valid && std::abs(sample.battery_percent - 82.5f) < .001f);
}

Bytes bms_packet(size_t cells = 3, size_t temps = 2, bool extended = true) {
  Bytes packet{96};
  put32(packet, 75600000);
  put32(packet, 84000000);
  put32(packet, -1234567);
  put32(packet, -1200000);
  put32(packet, 12345);
  put32(packet, 67890);
  packet.push_back(uint8_t(cells));
  for (size_t i = 0; i < cells; ++i)
    put16(packet, 4200 - int(i));
  for (size_t i = 0; i < cells; ++i)
    packet.push_back(uint8_t(i % 2));
  packet.push_back(uint8_t(temps));
  for (size_t i = 0; i < temps; ++i)
    put16(packet, 2500 + int(i) * 10);
  put16(packet, 3000);
  put16(packet, 2850);
  put16(packet, 4567);
  put16(packet, 3500);
  if (extended) {
    put16(packet, 825);
    put16(packet, 970);
    packet.push_back(7);
    for (float value : {123.5f, 4567.25f, 100.25f, 4000.f})
      put32(packet, std::bit_cast<int32_t>(value));
    put16(packet, 10132);
    packet.push_back(3);
    for (char c : std::string_view("Charging"))
      packet.push_back(uint8_t(c));
    packet.push_back(0);
  }
  return packet;
}

void bms_telemetry_tests() {
  const auto packet = bms_packet();
  CHECK(packet.size() == 81);
  bike::Telemetry sample;
  sample.setup_ms = 50;
  CHECK(bike::decode_telemetry(packet, 1000, sample));
  const auto &bms = sample.bms;
  CHECK(bms.available && bms.fresh(5999) && !bms.fresh(6000) && !bms.fresh(999));
  CHECK(sample.setup_ms == 50 && !sample.setup_valid && !sample.can_configure(1000));
  CHECK(std::abs(bms.pack_voltage - 75.6f) < .001f && bms.charge_voltage == 84);
  CHECK(std::abs(bms.input_current + 1.234567f) < .000001f && bms.input_current_ic == -1.2f);
  CHECK(std::abs(bms.amp_hours - 12.345f) < .001f && std::abs(bms.watt_hours - 67.89f) < .001f);
  CHECK(bms.cell_count == 3 && bms.temp_count == 2);
  CHECK(std::abs(bms.cell_volts[2] - 4.198f) < .0001f);
  CHECK(!bms.balancing[0] && bms.balancing[1] && !bms.balancing[2]);
  CHECK(bms.temps_c[0] == 25 && bms.temps_c[1] == 25.1f);
  CHECK(bms.ic_c == 30 && bms.humidity_sensor_c == 28.5f && bms.max_cell_c == 35);
  CHECK(std::abs(bms.humidity_percent - 45.67f) < .001f);
  CHECK(bms.soc_valid && bms.soh_valid && bms.soc == .825f && bms.soh == .97f);
  CHECK(bms.can_id_valid && bms.can_id == 7 && bms.lifetime_valid);
  CHECK(bms.lifetime_ah_charged_total == 123.5f && bms.lifetime_wh_charged_total == 4567.25f);
  CHECK(bms.lifetime_ah_discharged_total == 100.25f && bms.lifetime_wh_discharged_total == 4000);
  CHECK(bms.pressure_valid && bms.pressure_pa == 101320 && bms.data_version_valid && bms.data_version == 3);
  CHECK(bms.status_valid && std::string_view(bms.status.data()) == "Charging");
  for (size_t size = 0; size < packet.size(); ++size) {
    auto copy = sample;
    const bool prefix = size == 48 || size == 50 || size == 52 || size == 53 || size == 69 || size == 71 || size == 72;
    CHECK(bike::decode_telemetry(std::span(packet).first(size), 2000, copy) == prefix);
    if (prefix) {
      CHECK(copy.bms.received_ms == 2000 && copy.bms.available);
      CHECK(copy.bms.soc_valid == (size >= 50) && copy.bms.soh_valid == (size >= 52));
      CHECK(copy.bms.can_id_valid == (size >= 53) && copy.bms.lifetime_valid == (size >= 69));
      CHECK(copy.bms.pressure_valid == (size >= 71) && copy.bms.data_version_valid == (size >= 72));
      CHECK(!copy.bms.status_valid);
    }
    else {
      CHECK(copy.bms.received_ms == 1000 && copy.bms.cell_volts == bms.cell_volts);
      CHECK(copy.bms.status == bms.status && copy.bms.lifetime_ah_charged_total == bms.lifetime_ah_charged_total);
    }
  }
  auto reject = [&](Bytes invalid) {
    auto copy = sample;
    CHECK(!bike::decode_telemetry(invalid, 2000, copy));
    CHECK(copy.bms.received_ms == 1000 && copy.bms.cell_volts == sample.bms.cell_volts);
    CHECK(copy.bms.temps_c == sample.bms.temps_c && copy.bms.status == sample.bms.status);
  };
  auto invalid = packet;
  invalid[25] = 51;
  reject(invalid);
  invalid = packet;
  invalid[35] = 51;
  reject(invalid);
  invalid = packet;
  invalid[32] = 2;
  reject(invalid);
  invalid = packet;
  write16(invalid, 26, -1);
  reject(invalid);
  invalid = packet;
  write32(invalid, 1, uint32_t(-1));
  reject(invalid);
  invalid = packet;
  write16(invalid, 44, 10001);
  reject(invalid);
  invalid = packet;
  write16(invalid, 48, 1001);
  reject(invalid);
  invalid = packet;
  write16(invalid, 50, -1);
  reject(invalid);
  for (size_t index = 0; index < 4; ++index)
    for (uint32_t bits : {0x7f800000u, 0x7fc00000u, 0xbf800000u}) {
      invalid = packet;
      write32(invalid, 53 + index * 4, bits);
      reject(invalid);
    }
  invalid = packet;
  write16(invalid, 69, -1);
  reject(invalid);
  invalid = packet;
  invalid.pop_back();
  reject(invalid);
  invalid = packet;
  invalid.push_back(0);
  reject(invalid);
  invalid.resize(72);
  invalid.insert(invalid.end(), 41, 'X');
  invalid.push_back(0);
  reject(invalid);
  invalid = packet;
  invalid[72] = 255;
  CHECK(bike::decode_telemetry(invalid, 2000, sample));
  CHECK(sample.bms.status[0] == '?');
  CHECK(bike::decode_telemetry(bms_packet(50, 50), 3000, sample));
  CHECK(sample.bms.cell_count == 50 && sample.bms.temp_count == 50 && sample.bms.balancing[49]);
  auto empty = bms_packet(0, 0, false);
  std::fill(empty.begin() + 1, empty.end(), 0);
  CHECK(bike::decode_telemetry(empty, 4000, sample));
  CHECK(!sample.bms.available && !sample.bms.fresh(4000) && sample.bms.cell_count == 0);
  CHECK(!sample.bms.soc_valid && !sample.bms.lifetime_valid && !sample.bms.status_valid);
  // A modern controller with no cached BMS sends zeros plus can_id = -1 (wire255).
  put16(empty, 0);
  put16(empty, 0);
  empty.push_back(255);
  for (int i = 0; i < 4; ++i)
    put32(empty, 0);
  put16(empty, 0);
  empty.push_back(0);
  empty.push_back(0);
  CHECK(bike::decode_telemetry(empty, 4500, sample));
  CHECK(!sample.bms.available && !sample.bms.can_id_valid && !sample.bms.fresh(4500));
  CHECK(sample.bms.status_valid && sample.bms.status[0] == 0);
  for (auto id : {bike::IntegrationId::Vesc, bike::IntegrationId::WheelE, bike::IntegrationId::Wfb}) {
    bike::Telemetry integrated;
    CHECK(bike::integration(id).decode(packet, 5000, integrated));
    CHECK(integrated.bms.available && !integrated.setup_valid && !integrated.bike_valid);
  }
}

int main() {
  distance_reporting_tests();
  expanded_setup_tests();
  battery_estimate_validity_tests();
  bms_telemetry_tests();
  const auto &wheel_e = bike::integration(bike::IntegrationId::WheelE);
  const auto &wfb_vehicle = bike::integration(bike::IntegrationId::Wfb);
  const auto &generic_vehicle = bike::integration(bike::IntegrationId::Vesc);
  CHECK(wheel_e.features().lock && !wheel_e.features().ride_presets && !wheel_e.features().speed_limit);
  CHECK(wfb_vehicle.features().ride_presets && wfb_vehicle.features().speed_limit && wfb_vehicle.features().lock);
  CHECK(!generic_vehicle.features().lock && generic_vehicle.poll().empty());
  CHECK(bike::integration_id(-1) == bike::IntegrationId::Auto);
  CHECK(bike::integration_id(255) == bike::IntegrationId::Auto);
  std::array<uint8_t, 6> preset{};
  std::array<uint8_t, 4> vehicle_command{};
  CHECK(!wheel_e.ride(bike::RideMode::Sport, preset));
  CHECK(!wheel_e.speed_limit(20, vehicle_command));
  CHECK(!generic_vehicle.lock(true, vehicle_command));
  CHECK(wheel_e.lock(true, vehicle_command));
  CHECK((vehicle_command == std::array<uint8_t, 4>{36, 101, 12, 1}));
  CHECK(wheel_e.lock(false, vehicle_command));
  CHECK((vehicle_command == std::array<uint8_t, 4>{36, 101, 12, 0}));
  CHECK(wfb_vehicle.lock(false, vehicle_command));
  CHECK((vehicle_command == std::array<uint8_t, 4>{36, 102, 12, 0}));
  CHECK(Bytes(wheel_e.poll().begin(), wheel_e.poll().end()) == Bytes({36, 101, 33, 1, 0, 10, 0, 2, 0, 0, 0, 0}));
  Bytes wheel_rt{36, 101, 33, 1};
  put32(wheel_rt, (1u << 1) | (1u << 17) | (1u << 19));
  put32(wheel_rt, 0);
  put32(wheel_rt, 12345);
  put32(wheel_rt, 0); // wheel-e raw DISABLED
  put32(wheel_rt, std::bit_cast<uint32_t>(-12.5f));
  put32(wheel_rt, std::bit_cast<uint32_t>(3.25f));
  bike::Telemetry wheel_sample;
  CHECK(wheel_e.decode(wheel_rt, 1000, wheel_sample));
  CHECK(wheel_sample.bike_valid && wheel_sample.bike_ms == 1000 && wheel_sample.state == 15);
  CHECK(wheel_sample.pitch_deg == -12.5f && wheel_sample.roll_deg == 3.25f);
  CHECK(!wfb_vehicle.decode(wheel_rt, 1000, wheel_sample));
  CHECK(!generic_vehicle.decode(wheel_rt, 1000, wheel_sample));
  for (size_t size = 0; size < wheel_rt.size(); ++size) {
    auto previous = wheel_sample;
    CHECK(!wheel_e.decode(std::span(wheel_rt).first(size), 2000, wheel_sample));
    CHECK(wheel_sample.bike_ms == previous.bike_ms && wheel_sample.pitch_deg == previous.pitch_deg);
  }
  wheel_rt[16] = 3; // raw RUNNING -> normalized running, never lockable
  CHECK(wheel_e.decode(wheel_rt, 2000, wheel_sample) && wheel_sample.state == 1);
  wheel_rt[16] = 4; // ordinary two-wheel throttle state, checked for speed/current separately
  CHECK(wheel_e.decode(wheel_rt, 2000, wheel_sample) && wheel_sample.state == 16);
  wheel_sample.setup_valid = true;
  wheel_sample.setup_ms = 2000;
  CHECK(bike::can_lock(wheel_sample, 2000));
  wheel_sample.speed_mps = 1;
  CHECK(!bike::can_lock(wheel_sample, 2000));
  wheel_sample.speed_mps = 0;
  wheel_rt[16] = 5;
  CHECK(wheel_e.decode(wheel_rt, 2000, wheel_sample) && wheel_sample.state == 17);
  CHECK(!bike::can_lock(wheel_sample, 2000));
  wheel_rt[16] = 7;
  CHECK(!wheel_e.decode(wheel_rt, 3000, wheel_sample));
  wheel_rt[16] = 0;
  wheel_rt[17] = 0x20; // charging flag
  CHECK(wheel_e.decode(wheel_rt, 2000, wheel_sample) && wheel_sample.state == 14);
  CHECK(!bike::can_lock(wheel_sample, 2000));
  wheel_rt[17] = 0x10; // fatal-error flag
  CHECK(wheel_e.decode(wheel_rt, 2000, wheel_sample) && wheel_sample.state == 255);
  CHECK(!bike::can_lock(wheel_sample, 2000));
  wheel_rt[17] = 0;
  wheel_rt[20] = 0x7f;
  wheel_rt[21] = 0x80; // non-finite pitch must leave telemetry untouched
  CHECK(!wheel_e.decode(wheel_rt, 3000, wheel_sample));
  CHECK(wheel_sample.bike_ms == 2000 && wheel_sample.state == 255);
  wheel_rt[4] = 1;
  CHECK(!wheel_e.decode(wheel_rt, 3000, wheel_sample));
  CHECK(bike::wfb_name_matches("WFB Bike"));
  CHECK(bike::wfb_name_matches("My wFb-123"));
  CHECK(!bike::wfb_name_matches("VESC"));
  CHECK(!bike::wfb_name_matches("Nordic UART"));
  CHECK(!bike::wfb_name_matches(""));
  CHECK(bike::valid_ble_pin("0"));
  CHECK(bike::valid_ble_pin("1234"));
  CHECK(bike::valid_ble_pin("000123"));
  CHECK(!bike::valid_ble_pin(""));
  CHECK(!bike::valid_ble_pin("1234567"));
  CHECK(!bike::valid_ble_pin("123x"));
  CHECK(!bike::valid_pin("1234"));
  std::string pattern;
  CHECK(bike::append_pattern(pattern, 1));
  CHECK(bike::append_pattern(pattern, 3) && pattern == "123");
  CHECK(bike::append_pattern(pattern, 9) && pattern == "12369");
  CHECK(bike::append_pattern(pattern, 8) && pattern == "123698");
  CHECK(!bike::append_pattern(pattern, 7));
  CHECK(bike::valid_pattern(pattern));
  CHECK(!bike::valid_pattern("12345"));
  CHECK(!bike::valid_pattern("123451"));
  CHECK(!bike::valid_pattern("139876"));
  CHECK(!bike::valid_pattern("023456"));
  pattern = "12367";
  CHECK(!bike::append_pattern(pattern, 9) && pattern == "12367");
  bike::LockRecord stored, decoded;
  stored.version = 1;
  stored.armed = 1;
  stored.digest[0] = 42;
  const auto serialized = std::span(reinterpret_cast<const uint8_t *>(&stored), sizeof(stored));
  CHECK(bike::decode_lock_record(serialized.first(52), decoded));
  CHECK(decoded.version == 1 && !decoded.pattern && decoded.armed && decoded.digest[0] == 42);
  CHECK(bike::decode_lock_record(serialized, decoded));
  stored.version = 2;
  stored.pattern = 1;
  CHECK(bike::decode_lock_record(serialized, decoded) && decoded.pattern);
  CHECK(!bike::decode_lock_record(serialized.first(52), decoded));
  stored.pattern = 2;
  CHECK(!bike::decode_lock_record(serialized, decoded));
  stored.pattern = 1;
  stored.version = 1;
  CHECK(!bike::decode_lock_record(serialized, decoded));
  CHECK(bike::valid_pin("012345"));
  CHECK(!bike::valid_pin("12345"));
  CHECK(!bike::valid_pin("1234567"));
  CHECK(!bike::valid_pin("12345x"));
  CHECK((bike::wfb_lock_command(true) == std::array<uint8_t, 4>{36, 102, 12, 1}));
  CHECK((bike::wfb_lock_command(false) == std::array<uint8_t, 4>{36, 102, 12, 0}));
  bike::PinAttempts attempts;
  for (int i = 0; i < 5; ++i)
    attempts.result(false, 1000);
  CHECK(!attempts.allowed(30999));
  CHECK(attempts.allowed(31000));
  attempts.result(true, 31000);
  CHECK(attempts.failures == 0 && attempts.allowed(31000));
  bike::Telemetry stopped;
  stopped.setup_valid = stopped.bike_valid = true;
  stopped.setup_ms = stopped.bike_ms = 2000;
  stopped.state = 15;
  CHECK(bike::can_lock(stopped, 2100));
  bike::LockRequest lock_request{true, true, 7, 1500};
  CHECK(lock_request.confirmed(stopped, 7, 2100));
  CHECK(!lock_request.confirmed(stopped, 8, 2100));
  CHECK(!lock_request.confirmed(stopped, 7, 3600));
  stopped.bike_ms = 1800;
  CHECK(!lock_request.confirmed(stopped, 7, 2100));
  stopped.bike_ms = 2000;
  stopped.state = 1;
  CHECK(!bike::can_lock(stopped, 2100));
  lock_request.desired = false;
  CHECK(!lock_request.confirmed(stopped, 7, 2100));
  stopped.state = 0;
  CHECK(lock_request.confirmed(stopped, 7, 2100));
  stopped.speed_mps = 1;
  CHECK(!bike::can_lock(stopped, 2100));
  stopped.speed_mps = 0;
  stopped.motor_current = 3;
  CHECK(!bike::can_lock(stopped, 2100));
  CHECK(bike::trusted_update_url("https://github.com/techfoundrynz/wheelie_fun_display/releases/download/v1/test.bin"));
  CHECK(bike::trusted_update_url("https://release-assets.githubusercontent.com/path?signature=abc"));
  for (const char *url :
       {"http://github.com/file", "https://github.com.evil.test/file", "https://github.com@evil.test/file",
        "https://evil.test/github.com", "file:///firmware.bin", "https://github.com:80/file"})
    CHECK(!bike::trusted_update_url(url));
  auto digest = bike::release_digest("sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
  CHECK(digest && (*digest)[0] == 1 && (*digest)[31] == 239);
  CHECK(!bike::release_digest("sha256:123"));
  CHECK(!bike::release_digest("sha256:Z123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
  for (const auto dimensions : {std::array<int, 2>{410, 502}, std::array<int, 2>{480, 800}}) {
    for (const auto orientation : {bike::Orientation::Portrait, bike::Orientation::Landscape,
                                   bike::Orientation::PortraitInverted, bike::Orientation::LandscapeInverted}) {
      for (int y = 0; y < dimensions[1]; ++y)
        for (int x = 0; x < dimensions[0]; ++x) {
          const auto logical = bike::to_logical({x, y}, orientation, dimensions[0], dimensions[1]);
          CHECK(logical.x >= 0 && logical.x < bike::display_width(orientation, dimensions[0], dimensions[1]));
          CHECK(logical.y >= 0 && logical.y < bike::display_height(orientation, dimensions[0], dimensions[1]));
          const auto raw = bike::to_panel(logical, orientation, dimensions[0], dimensions[1]);
          CHECK(raw.x == x && raw.y == y);
        }
    }
  }

  // Test the actual firmware touch transform at every native panel pixel,
  // including corners: no off-by-one, mirrored axis or out-of-bounds result.
  for (const auto orientation : {bike::Orientation::Portrait, bike::Orientation::Landscape,
                                 bike::Orientation::PortraitInverted, bike::Orientation::LandscapeInverted}) {
    for (int y = 0; y < bike::panel_height; ++y)
      for (int x = 0; x < bike::panel_width; ++x) {
        const auto p = bike::to_logical({x, y}, orientation);
        CHECK(p.x >= 0 && p.x < bike::display_width(orientation));
        CHECK(p.y >= 0 && p.y < bike::display_height(orientation));
        const auto raw = bike::to_panel(p, orientation);
        CHECK(raw.x == x && raw.y == y);
      }
  }
  for (int degrees : {0, 90, 180, 270}) {
    CHECK(bike::valid_rotation(degrees));
    CHECK(static_cast<int>(bike::orientation_from_degrees(degrees)) == degrees);
  }
  for (int degrees : {-90, 1, 45, 360, 999})
    CHECK(!bike::valid_rotation(degrees));
  CHECK(bike::to_logical({0, 0}, bike::Orientation::PortraitInverted).x == 409);
  CHECK(bike::to_logical({0, 0}, bike::Orientation::PortraitInverted).y == 501);
  CHECK(bike::to_logical({0, 0}, bike::Orientation::LandscapeInverted).x == 501);
  CHECK(bike::to_logical({0, 0}, bike::Orientation::LandscapeInverted).y == 0);
  CHECK(bike::to_logical({0, 0}, bike::Orientation::Landscape).x == 0);
  CHECK(bike::to_logical({0, 0}, bike::Orientation::Landscape).y == 409);
  CHECK(bike::to_logical({409, 501}, bike::Orientation::Landscape).x == 501);
  CHECK(bike::to_logical({409, 501}, bike::Orientation::Landscape).y == 0);
  CHECK(bike::to_logical({-1, 9999}, bike::Orientation::Landscape).x == 501);
  const Bytes check{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  CHECK(bike::crc16(check) == 0x31c3);
  // Boundary regression: exactly 256 bytes must use the two-byte length form.
  for (size_t n : {1u, 20u, 255u, 256u, 1024u}) {
    Bytes payload(n);
    for (size_t i = 0; i < n; ++i)
      payload[i] = uint8_t(i * 17);
    auto f = framed(payload);
    CHECK(f[0] == (n <= 255 ? 2 : 3));
    for (size_t split = 0; split <= f.size(); ++split) {
      Receiver r;
      r.decoder.feed(std::span(f).first(split), 1);
      r.decoder.feed(std::span(f).subspan(split), 2);
      CHECK(r.packets.size() == 1 && r.packets[0] == payload);
    }
  }
  std::array<uint8_t, 8> tiny{};
  CHECK(bike::frame({}, tiny) == 0);
  CHECK(bike::frame(Bytes(1025), tiny) == 0);
  CHECK(bike::frame(Bytes{1}, tiny, 256) == 0);
  CHECK(bike::frame(Bytes(6), tiny) == 0);
  Receiver forwarded;
  forwarded.decoder.feed(framed({47}, 42), 1);
  CHECK(forwarded.packets[0] == Bytes({34, 42, 47}));
  Receiver no_double;
  no_double.decoder.feed(framed({34, 42, 47}, 42), 1);
  CHECK(no_double.packets[0] == Bytes({34, 42, 47}));
  // Corrupt CRC, bad terminator, garbage and coalesced frames recover.
  auto valid = framed({47});
  auto corrupt = valid;
  corrupt[3] ^= 0x80;
  Bytes stream{0, 255, 1};
  stream.insert(stream.end(), corrupt.begin(), corrupt.end());
  stream.insert(stream.end(), valid.begin(), valid.end());
  stream.insert(stream.end(), valid.begin(), valid.end());
  Receiver recovery;
  recovery.decoder.feed(stream, 1);
  CHECK(recovery.packets.size() == 2 && recovery.decoder.rejected() > 0);
  Receiver timeout;
  timeout.decoder.feed(Bytes{3, 1, 0, 55}, 1);
  timeout.decoder.feed(valid, 502);
  CHECK(timeout.packets.size() == 1);
  // Every single-bit alteration must fail the CRC/terminator check.
  auto original = framed({36, 102, 103, 20});
  for (size_t i = 2; i < original.size(); ++i)
    for (int bit = 0; bit < 8; ++bit) {
      auto bad = original;
      bad[i] ^= 1 << bit;
      Receiver r;
      r.decoder.feed(bad, 0);
      CHECK(r.packets.empty());
    }
  std::mt19937 random(123);
  Receiver fuzz;
  for (int i = 0; i < 20000; ++i) {
    Bytes noise(random() % 120);
    for (auto &v : noise)
      v = uint8_t(random());
    fuzz.decoder.feed(noise, i);
  }
  fuzz.decoder.feed(valid, 21000);
  CHECK(!fuzz.packets.empty() && fuzz.packets.back() == Bytes{47});
  CHECK(bike::wfb_ride_command(bike::RideMode::Eco) == (std::array<uint8_t, 6>{36, 102, 100, 0x99, 12, 18}));
  CHECK(bike::wfb_ride_command(bike::RideMode::Cruise) == (std::array<uint8_t, 6>{36, 102, 100, 0x9f, 14, 24}));
  CHECK(bike::wfb_ride_command(bike::RideMode::Sport) == (std::array<uint8_t, 6>{36, 102, 100, 0x9f, 33, 24}));
  CHECK(bike::wfb_ramp_byte(.0001) == 0 && bike::wfb_ramp_byte(.01) == 28 && bike::wfb_ramp_byte(.1) == 118 &&
        bike::wfb_ramp_byte(1) == 248);
  std::array<uint8_t, 4> limit{};
  CHECK(bike::wfb_speed_limit_command(20, limit) && limit[3] == 20);
  CHECK(bike::wfb_speed_limit_command(0, limit) && limit[3] == 0);
  for (int invalid : {-1, 1, 4, 36, 255})
    CHECK(!bike::wfb_speed_limit_command(invalid, limit));
  bike::Telemetry t;
  const auto full = setup();
  CHECK(bike::decode_telemetry(full, 100, t));
  CHECK(t.speed_mps == 2.5f && std::abs(t.battery_percent - 82.5f) < .01f && std::abs(t.voltage - 75.6f) < .01f);
  CHECK(t.mosfet_c == 35 && t.motor_c == 40 && t.motor_current < 0);
  for (size_t n = 0; n < full.size(); ++n) {
    auto copy = t;
    CHECK(!bike::decode_telemetry(std::span(full).first(n), 200, copy));
    CHECK(copy.setup_ms == 100);
  }
  CHECK(!t.can_configure(100)); // no WFB identity/telemetry
  CHECK(bike::decode_telemetry(setup(0), 100, t));
  Bytes bike(48);
  bike[0] = 36;
  bike[1] = 102;
  bike[2] = 10;
  bike[3] = 1;
  bike[10] = 1;
  CHECK(bike::decode_telemetry(bike, 100, t));
  CHECK(t.can_configure(100));
  CHECK(!t.can_configure(1600));
  CHECK(bike::decode_telemetry(setup(500), 200, t));
  CHECK(!t.can_configure(200));
  CHECK(bike::decode_telemetry(setup(-500), 200, t));
  CHECK(!t.can_configure(200));
  CHECK(bike::decode_telemetry(setup(0, 1), 200, t));
  CHECK(!t.can_configure(200));
  CHECK(bike::decode_telemetry(setup(0), 1700, t));
  CHECK(!t.can_configure(1700)); // WFB stale independently
  bike[1] = 101;
  CHECK(!bike::decode_telemetry(bike, 1700, t));
  bike[1] = 102;
  for (size_t n = 0; n < bike.size(); ++n)
    CHECK(!bike::decode_telemetry(std::span(bike).first(n), 200, t));
  bike[3] = 2;
  CHECK(!bike::decode_telemetry(bike, 200, t));
  bike.resize(55);
  CHECK(bike::decode_telemetry(bike, 200, t));
  CHECK(!bike::decode_telemetry(setup(151000), 200, t));
  std::cout << "Protocol, framing, fuzz, telemetry and configuration gate checks passed\n";
}
