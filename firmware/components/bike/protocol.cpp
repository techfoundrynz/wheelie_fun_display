#include "bike/protocol.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace bike
{

uint16_t crc16(std::span<const uint8_t> bytes) {
  uint16_t crc = 0;
  for (auto b : bytes) {
    crc ^= uint16_t(b) << 8;
    for (int i = 0; i < 8; ++i)
      crc = (crc & 0x8000) ? uint16_t((crc << 1) ^ 0x1021) : uint16_t(crc << 1);
  }
  return crc;
}

size_t frame(std::span<const uint8_t> payload, std::span<uint8_t> out, int can_id) {
  if (payload.empty() || can_id < -1 || can_id > 255)
    return 0;
  const bool forward = can_id >= 0 && payload[0] != 34;
  const size_t n = payload.size() + (forward ? 2 : 0);
  const size_t header = n <= 255 ? 2 : 3;
  if (n > max_payload || out.size() < n + header + 3)
    return 0;
  out[0] = n <= 255 ? 2 : 3;
  size_t p = 1;
  if (header == 3)
    out[p++] = uint8_t(n >> 8);
  out[p++] = uint8_t(n);
  if (forward) {
    out[p++] = 34;
    out[p++] = uint8_t(can_id);
  }
  std::copy(payload.begin(), payload.end(), out.begin() + p);
  const auto crc = crc16(out.subspan(header, n));
  p = header + n;
  out[p++] = uint8_t(crc >> 8);
  out[p++] = uint8_t(crc);
  out[p++] = 3;
  return p;
}

void Decoder::discard(size_t n) {
  used_ -= n;
  std::memmove(buffer_.data(), buffer_.data() + n, used_);
}

void Decoder::feed(std::span<const uint8_t> bytes, uint64_t now) {
  if (used_ && now - last_ms_ > 500) {
    reset();
    ++rejected_;
  }
  last_ms_ = now;
  for (auto b : bytes) {
    if (used_ == buffer_.size()) {
      discard(1);
      ++rejected_;
    }
    buffer_[used_++] = b;
    parse();
  }
}

void Decoder::parse() {
  while (used_) {
    if (buffer_[0] != 2 && buffer_[0] != 3) {
      discard(1);
      continue;
    }
    const size_t h = buffer_[0] == 2 ? 2 : 3;
    if (used_ < h)
      return;
    const size_t n = h == 2 ? buffer_[1] : (size_t(buffer_[1]) << 8) | buffer_[2];
    if (!n || n > max_payload) {
      discard(1);
      ++rejected_;
      continue;
    }
    const size_t total = h + n + 3;
    if (used_ < total)
      return;
    const auto bytes = std::span<const uint8_t>(buffer_.data() + h, n);
    const uint16_t expected = (uint16_t(buffer_[h + n]) << 8) | buffer_[h + n + 1];
    if (buffer_[total - 1] != 3 || expected != crc16(bytes)) {
      // A complete rejected frame has a known extent. Consuming it also
      // prevents its 0x03 terminator becoming a false long-frame start.
      discard(total);
      ++rejected_;
      continue;
    }
    handler_(context_, bytes); // callback must consume/copy synchronously, never re-enter
    discard(total);
  }
}

uint8_t wfb_ramp_byte(double a) {
  if (!std::isfinite(a) || a <= .0001)
    return 0;
  double v;
  if (a <= .001)
    v = (a - .0001) / .0001;
  else if (a <= .01)
    v = 10 + (a - .001) / .0005;
  else if (a <= .1)
    v = 28 + (a - .01) / .001;
  else if (a <= .5)
    v = 118 + (a - .1) / .005;
  else if (a < 1)
    v = 198 + (a - .5) / .01;
  else
    v = 248;
  return uint8_t(std::clamp(std::lround(v), 0L, 248L));
}

std::array<uint8_t, 6> wfb_ride_command(RideMode mode) {
  const bool eco = mode == RideMode::Eco;
  return {36,
          102,
          100,
          uint8_t(0x90 | (eco ? 9 : 15)),
          wfb_ramp_byte(eco                       ? .002
                        : mode == RideMode::Sport ? .015
                                                  : .003),
          wfb_ramp_byte(eco ? .005 : .008)};
}

bool wfb_speed_limit_command(int mph, std::array<uint8_t, 4> &out) {
  if (mph != 0 && (mph < 5 || mph > 35))
    return false;
  out = {36, 102, 103, uint8_t(mph)};
  return true;
}

} // namespace bike
