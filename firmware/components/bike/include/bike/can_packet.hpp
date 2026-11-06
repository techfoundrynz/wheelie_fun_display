#pragma once
#include "protocol.hpp"
#include <algorithm>
#include <array>
#include <span>

namespace bike
{

struct CanFrame {
  uint32_t id = 0;
  uint8_t size = 0;
  std::array<uint8_t, 8> data{};
  bool extended = true, remote = false;
};

// Native VESC CAN packet transport. UART framing is deliberately absent.
template <class Send>
bool send_can_packet(std::span<const uint8_t> payload, uint8_t source, uint8_t destination, bool reply, Send send) {
  if (payload.empty() || payload.size() > max_payload || source == 255 || destination == 255 || source == destination)
    return false;
  CanFrame f;
  if (payload.size() <= 6) {
    f.id = (8u << 8) | destination;
    f.data[0] = source;
    f.data[1] = reply;
    f.size = payload.size() + 2;
    std::copy(payload.begin(), payload.end(), f.data.begin() + 2);
    return send(f);
  }
  for (size_t offset = 0; offset < payload.size();) {
    const bool long_offset = offset > 255;
    const size_t header = long_offset ? 2 : 1;
    const auto count = std::min(8 - header, payload.size() - offset);
    f.id = ((long_offset ? 6u : 5u) << 8) | destination;
    f.size = header + count;
    if (long_offset) {
      f.data[0] = offset >> 8;
      f.data[1] = offset;
    }
    else
      f.data[0] = offset;
    std::copy_n(payload.begin() + offset, count, f.data.begin() + header);
    if (!send(f))
      return false;
    offset += count;
  }
  const auto crc = crc16(payload);
  f.id = (7u << 8) | destination;
  f.size = 6;
  f.data[0] = source;
  f.data[1] = reply;
  f.data[2] = payload.size() >> 8;
  f.data[3] = payload.size();
  f.data[4] = crc >> 8;
  f.data[5] = crc;
  return send(f);
}

class CanReassembler {
public:

  void reset() {
    used_ = 0;
    sender_ = -1;
  }

  int sender() const {
    return sender_;
  }

  // A returned span remains valid until the next feed. Only replies from the
  // selected controller addressed to this display are accepted.
  std::span<const uint8_t> feed(const CanFrame &f, uint8_t local, int peer, uint64_t now) {
    if (used_ && now - started_ >= 500)
      reset();
    if (!f.extended || f.remote || f.size > 8 || (f.id & 255) != local)
      return {};
    const auto command = f.id >> 8;
    if (command == 8) {
      if (f.size < 3 || (peer >= 0 && f.data[0] != peer) || f.data[1] != 1)
        return {};
      reset();
      sender_ = f.data[0];
      std::copy_n(f.data.begin() + 2, f.size - 2, buffer_.begin());
      return {buffer_.data(), size_t(f.size - 2)};
    }
    if (command == 5 || command == 6) {
      const size_t header = command == 5 ? 1 : 2;
      if (f.size <= header) {
        reset();
        return {};
      }
      const size_t offset = command == 5 ? f.data[0] : (size_t(f.data[0]) << 8) | f.data[1];
      if (offset == 0) {
        reset();
        started_ = now;
      }
      const size_t count = f.size - header;
      if (offset != used_ || offset + count > buffer_.size() || (offset != 0 && !used_)) {
        reset();
        return {};
      }
      std::copy_n(f.data.begin() + header, count, buffer_.begin() + offset);
      used_ += count;
      return {};
    }
    if (command == 7) {
      const size_t size = (size_t(f.data[2]) << 8) | f.data[3];
      const bool valid = f.size == 6 && (peer < 0 || f.data[0] == peer) && f.data[1] == 1 && size > 0 &&
                         size == used_ && crc16({buffer_.data(), used_}) == ((uint16_t(f.data[4]) << 8) | f.data[5]);
      reset();
      if (valid) {
        sender_ = f.data[0];
        return {buffer_.data(), size};
      }
    }
    return {};
  }

private:

  std::array<uint8_t, max_payload> buffer_{};
  size_t used_ = 0;
  uint64_t started_ = 0;
  int sender_ = -1;
};

} // namespace bike
