#pragma once
#include "protocol.hpp"
#include "telemetry.hpp"
#include <span>
#include <string_view>

namespace bike
{

enum class IntegrationId : uint8_t {
  Vesc = 0,
  Wfb = 1,
  WheelE = 2,
  Auto = 255
};

struct VehicleFeatures {
  bool ride_presets = false;
  bool speed_limit = false;
  bool lock = false;
};

class VehicleIntegration {
public:

  virtual ~VehicleIntegration() = default;

  virtual IntegrationId id() const = 0;

  virtual const char *name() const = 0;

  virtual VehicleFeatures features() const = 0;

  virtual std::span<const uint8_t> identity_request() const {
    return {};
  }

  virtual bool verify_identity(std::span<const uint8_t>) const {
    return false;
  }

  virtual std::span<const uint8_t> poll() const = 0;

  virtual bool decode(std::span<const uint8_t> payload, uint64_t now, Telemetry &out) const = 0;

  virtual bool ride(RideMode, std::array<uint8_t, 6> &) const {
    return false;
  }

  virtual bool speed_limit(int, std::array<uint8_t, 4> &) const {
    return false;
  }

  virtual bool lock(bool, std::array<uint8_t, 4> &) const {
    return false;
  }
};

const VehicleIntegration &integration(IntegrationId id);

IntegrationId integration_id(int value);

} // namespace bike
