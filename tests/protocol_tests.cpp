#include "bike/ble_name.hpp"
#include "bike/integration.hpp"
#include "bike/lock_policy.hpp"
#include "bike/orientation.hpp"
#include "bike/protocol.hpp"
#include "bike/telemetry.hpp"
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

int main() {
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
