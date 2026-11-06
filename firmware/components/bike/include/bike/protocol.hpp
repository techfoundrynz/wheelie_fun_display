#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace bike
{

constexpr size_t max_payload = 1024;
constexpr uint8_t custom_app = 36, wfb_app_id = 102;

uint16_t crc16(std::span<const uint8_t> bytes);

// Returns zero on invalid input or insufficient output space. CAN forwarding is explicit.

size_t frame(std::span<const uint8_t> payload, std::span<uint8_t> output, int can_id = -1);

class Decoder {
public:

  using Handler = void (*)(void *, std::span<const uint8_t>);

  Decoder(Handler handler, void *context) : handler_(handler), context_(context) {
  }

  void feed(std::span<const uint8_t> bytes, uint64_t now_ms);

  void reset() {
    used_ = 0;
  }

  uint32_t rejected() const {
    return rejected_;
  }

private:

  void parse();

  void discard(size_t count);

  std::array<uint8_t, max_payload + 6> buffer_{};
  size_t used_ = 0;
  uint64_t last_ms_ = 0;
  uint32_t rejected_ = 0;
  Handler handler_;
  void *context_;
};
enum class RideMode : uint8_t {
  Eco,
  Cruise,
  Sport
};

std::array<uint8_t, 6> wfb_ride_command(RideMode mode);

// UI limits are stricter than the buggy app encoder. Zero explicitly disables the limit.

bool wfb_speed_limit_command(int mph, std::array<uint8_t, 4> &out);

uint8_t wfb_ramp_byte(double alpha);

// Float-family LOCK: explicit state, never a toggle. WFB uses package ID 102.
constexpr std::array<uint8_t, 4> wfb_lock_command(bool locked) {
  return {custom_app, wfb_app_id, 12, uint8_t(locked)};
}

} // namespace bike
