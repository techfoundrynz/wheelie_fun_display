#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace bike
{

// Caller supplies synchronization and can place the complete object in PSRAM.
// Appends are atomic: capacity pressure never overwrites an accepted stream byte.
template <size_t Capacity = 16384> class ProxyByteRing {

  static_assert(Capacity > 0);

  std::array<uint8_t, Capacity> bytes_{};
  size_t head_ = 0, size_ = 0, high_water_ = 0;
  uint64_t pushed_ = 0, consumed_ = 0;
  uint32_t epoch_ = 0;

public:

  void reset(uint32_t epoch) {
    head_ = size_ = high_water_ = 0;
    pushed_ = consumed_ = 0;
    epoch_ = epoch;
  }

  uint32_t epoch() const {
    return epoch_;
  }

  size_t size() const {
    return size_;
  }

  size_t free() const {
    return Capacity - size_;
  }

  size_t high_water() const {
    return high_water_;
  }

  uint64_t pushed() const {
    return pushed_;
  }

  uint64_t consumed() const {
    return consumed_;
  }

  bool append(std::span<const uint8_t> input) {
    if (input.size() > free())
      return false;
    if (input.empty())
      return true;
    const auto tail = (head_ + size_) % Capacity;
    const auto first = std::min(input.size(), Capacity - tail);
    std::copy_n(input.begin(), first, bytes_.begin() + tail);
    std::copy(input.begin() + first, input.end(), bytes_.begin());
    size_ += input.size();
    pushed_ += input.size();
    high_water_ = std::max(high_water_, size_);
    return true;
  }

  bool append(std::span<const uint8_t> input, uint32_t epoch) {
    return epoch == epoch_ && append(input);
  }

  // Copying does not commit consumption: a backpressured writer can retry the
  // same batch, then consume it only after upstream admission succeeds.
  size_t copy(std::span<uint8_t> output, size_t offset = 0) const {
    if (offset >= size_)
      return 0;
    const auto count = std::min(output.size(), size_ - offset);
    if (!count)
      return 0;
    const auto start = (head_ + offset) % Capacity;
    const auto first = std::min(count, Capacity - start);
    std::copy_n(bytes_.begin() + start, first, output.begin());
    std::copy_n(bytes_.begin(), count - first, output.begin() + first);
    return count;
  }

  bool consume(size_t count) {
    if (count > size_)
      return false;
    head_ = (head_ + count) % Capacity;
    size_ -= count;
    consumed_ += count;
    return true;
  }

  size_t pop(std::span<uint8_t> output) {
    const auto count = copy(output);
    consume(count);
    return count;
  }
};

} // namespace bike
