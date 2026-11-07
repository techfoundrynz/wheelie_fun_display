#pragma once
#include "bike/proxy_byte_ring.hpp"
#include "bike/proxy_poll.hpp"
#include <array>
#include <span>

namespace bike
{

// Validate before forwarding, so a phone disappearing halfway through a frame
// cannot leave the controller parser waiting for bytes that will never arrive.
// Caller synchronizes the ring and batcher; preparing does not consume bytes.
class ProxyFrameBatcher {
  size_t remaining_ = 0, prepared_count_ = 0, prepared_remaining_ = 0;
  uint64_t prepared_consumed_ = 0, active_consumed_ = 0;
  uint32_t prepared_epoch_ = 0, active_epoch_ = 0;
  bool invalid_ = false, oversized_ = false;

  struct Frame {
    size_t size = 0;
    std::array<uint8_t, 6> prefix{};
    size_t prefix_size = 0;
  };

  template <size_t Capacity> Frame validate(const ProxyByteRing<Capacity> &ring, size_t offset) {
    std::array<uint8_t, 4> header{};
    const auto available = ring.copy(header, offset);
    if (!available)
      return {};
    const auto header_size = header[0];
    if (header_size < 2 || header_size > 4) {
      invalid_ = true;
      return {};
    }
    if (available < header_size)
      return {};
    size_t payload_size = 0;
    for (size_t i = 1; i < header_size; ++i)
      payload_size = (payload_size << 8) | header[i];
    if (!payload_size || (header_size == 3 && payload_size < 255) || (header_size == 4 && payload_size < 65535)) {
      invalid_ = true;
      return {};
    }
    const auto wire_size = payload_size + header_size + 3;
    if (wire_size > Capacity) {
      invalid_ = oversized_ = true;
      return {};
    }
    if (wire_size > ring.size() - offset)
      return {};
    ProxyFrameTracker tracker;
    std::array<uint8_t, 512> batch;
    size_t checked = 0;
    while (checked < wire_size) {
      const auto count =
          ring.copy(std::span(batch).first(std::min(batch.size(), wire_size - checked)), offset + checked);
      for (size_t i = 0; i < count; ++i) {
        const auto result = tracker.byte(batch[i]);
        if (result == ProxyFrameTracker::Result::Invalid ||
            (result == ProxyFrameTracker::Result::Complete && checked + i + 1 != wire_size)) {
          invalid_ = true;
          return {};
        }
      }
      checked += count;
    }
    if (!tracker.boundary()) {
      invalid_ = true;
      return {};
    }
    Frame frame;
    frame.size = wire_size;
    frame.prefix_size = tracker.prefix().size();
    std::copy(tracker.prefix().begin(), tracker.prefix().end(), frame.prefix.begin());
    return frame;
  }

public:

  void reset() {
    *this = {};
  }

  bool invalid() const {
    return invalid_;
  }

  bool oversized() const {
    return oversized_;
  }

  bool partial_frame() const {
    return remaining_ != 0;
  }

  template <size_t Capacity, class Allow>
  size_t prepare(const ProxyByteRing<Capacity> &ring, std::span<uint8_t> output, Allow allow) {
    prepared_count_ = 0;
    if (invalid_ || output.empty())
      return 0;
    if (remaining_ && (ring.epoch() != active_epoch_ || ring.consumed() != active_consumed_)) {
      invalid_ = true;
      return 0;
    }
    size_t count = 0, remaining = remaining_;
    if (remaining) {
      const auto take = std::min(remaining, output.size());
      if (ring.copy(output.first(take)) != take) {
        invalid_ = true;
        return 0;
      }
      count = take;
      remaining -= take;
    }
    while (!remaining && count < output.size() && count < ring.size()) {
      const auto frame = validate(ring, count);
      if (!frame.size) {
        // Report a malformed following frame only after this valid batch commits.
        // In particular, never prevent the tail of an already-started frame
        // from reaching the controller because the next frame is malformed.
        if (count)
          invalid_ = oversized_ = false;
        break;
      }
      if (!allow(std::span(frame.prefix).first(frame.prefix_size)))
        break;
      const auto space = output.size() - count;
      if (count && frame.size > space)
        break; // Coalesce complete small frames; don't split one just to fill a batch.
      const auto take = std::min(frame.size, space);
      ring.copy(output.subspan(count, take), count);
      count += take;
      remaining = frame.size - take;
    }
    prepared_count_ = count;
    prepared_remaining_ = remaining;
    prepared_consumed_ = ring.consumed();
    prepared_epoch_ = ring.epoch();
    return count;
  }

  template <size_t Capacity> size_t prepare(const ProxyByteRing<Capacity> &ring, std::span<uint8_t> output) {
    return prepare(ring, output, [](std::span<const uint8_t>) { return true; });
  }

  template <size_t Capacity> bool commit(ProxyByteRing<Capacity> &ring, size_t count) {
    if (!count || count != prepared_count_ || ring.epoch() != prepared_epoch_ ||
        ring.consumed() != prepared_consumed_ || !ring.consume(count))
      return false;
    remaining_ = prepared_remaining_;
    active_epoch_ = ring.epoch();
    active_consumed_ = ring.consumed();
    prepared_count_ = 0;
    return true;
  }
};

} // namespace bike
