#pragma once
#include <cstdint>

namespace bike
{

struct ConfigurationWriteContext {
  bool ready = false, integration_verified = false, unlocked = false, phone_present = true, telemetry_permits = false;
  bool has_candidate = false;
  uint32_t session = 0, confirmed_session = 0;
  uint64_t now_ms = 0, confirmed_ms = 0;
};

constexpr bool configuration_write_allowed(const ConfigurationWriteContext &context) {
  return context.ready && context.integration_verified && context.unlocked && !context.phone_present &&
         context.telemetry_permits && context.has_candidate && context.session == context.confirmed_session &&
         context.now_ms >= context.confirmed_ms && context.now_ms - context.confirmed_ms < 30000;
}

} // namespace bike
