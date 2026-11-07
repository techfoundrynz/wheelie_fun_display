#pragma once
#include <cstdint>
#include <span>

namespace vesc_update
{

struct Owner {
  bool native_can = false;
  uint32_t epoch = 0, session = 0;
  uint8_t peer = 0;
  bool operator==(const Owner &) const = default;
};

struct Snapshot {
  bool active = false;
  int progress = 0;
  const char *status = "Ready for VESC Tool";
  bool cancellable = false, native_can = false;
};

void start();

void set_allowed(bool begin, bool continue_update);

bool handles(std::span<const uint8_t> payload);

void submit(Owner owner, std::span<const uint8_t> payload);

void cancel();

Snapshot snapshot();

bool busy();

} // namespace vesc_update
