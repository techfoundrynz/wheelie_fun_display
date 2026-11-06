#include "bike/integration.hpp"
#include <bit>
#include <cmath>

namespace bike
{
namespace
{

constexpr uint32_t wheel_e_mask = (1u << 1) | (1u << 17) | (1u << 19);

class VescIntegration : public VehicleIntegration {
public:

  IntegrationId id() const override {
    return IntegrationId::Vesc;
  }

  const char *name() const override {
    return "VESC";
  }

  VehicleFeatures features() const override {
    return {};
  }

  std::span<const uint8_t> poll() const override {
    return {};
  }

  bool decode(std::span<const uint8_t> payload, uint64_t now, Telemetry &out) const override {
    return !payload.empty() && payload[0] == 47 && decode_telemetry(payload, now, out);
  }
};

class WfbIntegration final : public VescIntegration {
public:

  IntegrationId id() const override {
    return IntegrationId::Wfb;
  }

  const char *name() const override {
    return "Wheelie Fun Bike";
  }

  VehicleFeatures features() const override {
    return {true, true, true};
  }

  std::span<const uint8_t> poll() const override {
    static constexpr uint8_t command[] = {36, 102, 10, 1};
    return command;
  }

  bool decode(std::span<const uint8_t> payload, uint64_t now, Telemetry &out) const override {
    return decode_telemetry(payload, now, out);
  }

  bool ride(RideMode mode, std::array<uint8_t, 6> &out) const override {
    out = wfb_ride_command(mode);
    return true;
  }

  bool speed_limit(int mph, std::array<uint8_t, 4> &out) const override {
    return wfb_speed_limit_command(mph, out);
  }

  bool lock(bool locked, std::array<uint8_t, 4> &out) const override {
    out = wfb_lock_command(locked);
    return true;
  }
};

class WheelEIntegration final : public VescIntegration {
public:

  std::span<const uint8_t> identity_request() const override {
    static constexpr uint8_t command[] = {36, 101, 0, 2, 0};
    return command;
  }

  bool verify_identity(std::span<const uint8_t> payload) const override {
    if (payload.size() != 61 || payload[0] != 36 || payload[1] != 101 || payload[2] != 0 || payload[3] != 2 ||
        payload[4] != 0)
      return false;
    constexpr char expected[] = "wheel-e";
    for (size_t i = 0; i < 20; ++i) {
      auto character = payload[5 + i];
      if (character >= 'A' && character <= 'Z')
        character += 'a' - 'A';
      if (character != (i < sizeof(expected) - 1 ? uint8_t(expected[i]) : 0))
        return false;
    }
    // INFO v2 includes a nonzero system tick rate; never accept a truncated legacy response.
    return payload[52] || payload[53] || payload[54] || payload[55];
  }

  IntegrationId id() const override {
    return IntegrationId::WheelE;
  }

  const char *name() const override {
    return "wheel-e";
  }

  VehicleFeatures features() const override {
    return {false, false, true};
  }

  std::span<const uint8_t> poll() const override {
    // REALTIME_DATA: float32, raw state flags, pitch and roll; no mask2 fields.
    static constexpr uint8_t command[] = {36, 101, 33, 1, 0, 0x0a, 0, 2, 0, 0, 0, 0};
    return command;
  }

  bool lock(bool locked, std::array<uint8_t, 4> &out) const override {
    out = {36, 101, 12, uint8_t(locked)};
    return true;
  }

  bool decode(std::span<const uint8_t> p, uint64_t now, Telemetry &out) const override {
    if (!p.empty() && p[0] == 47)
      return VescIntegration::decode(p, now, out);
    if (p.size() != 28 || p[0] != 36 || p[1] != 101 || p[2] != 33 || p[3] != 1)
      return false;
    auto u32 = [&](size_t i) {
      return uint32_t(p[i]) << 24 | uint32_t(p[i + 1]) << 16 | uint32_t(p[i + 2]) << 8 | p[i + 3];
    };
    if (u32(4) != wheel_e_mask || u32(8) != 0)
      return false;
    const uint32_t flags = u32(16);
    const auto state = (flags >> 24) & 7;
    const float pitch = std::bit_cast<float>(u32(20)), roll = std::bit_cast<float>(u32(24));
    if (state > 5 || !std::isfinite(pitch) || !std::isfinite(roll) || std::abs(pitch) > 180 || std::abs(roll) > 180)
      return false;
    auto next = out;
    // Normalize to the existing display states; retain throttle/cruise separately.
    constexpr uint8_t states[] = {15, 11, 0, 1, 16, 17};
    next.state = flags & (1u << 21) ? 14 : states[state];
    if (flags & (1u << 20))
      next.state = 255;
    next.pitch_deg = pitch;
    next.roll_deg = roll;
    next.bike_valid = true;
    next.bike_ms = now;
    out = next;
    return true;
  }
};

} // namespace

IntegrationId integration_id(int value) {
  return value == -1 || value == 255 ? IntegrationId::Auto
         : value == 1                ? IntegrationId::Wfb
         : value == 2                ? IntegrationId::WheelE
                                     : IntegrationId::Vesc;
}

const VehicleIntegration &integration(IntegrationId id) {
  static const VescIntegration vesc;
  static const WfbIntegration wfb;
  static const WheelEIntegration wheel_e;
  switch (id) {
  case IntegrationId::Wfb:
    return wfb;
  case IntegrationId::WheelE:
    return wheel_e;
  default:
    return vesc;
  }
}

} // namespace bike
