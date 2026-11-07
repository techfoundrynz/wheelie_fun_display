#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace bike
{

class UpdateStorage {
public:

  virtual ~UpdateStorage() = default;

  virtual size_t capacity() const = 0;

  virtual bool begin(size_t size) = 0;

  virtual bool write(size_t offset, std::span<const uint8_t> bytes) = 0;

  virtual bool read(size_t offset, std::span<uint8_t> bytes) = 0;

  virtual bool finish() = 0;

  virtual void abort() = 0;

};

// VESC Tool prepends a big-endian size and CRC16 to the application image.
// Chunks containing only FF may be omitted; retries must not rewrite flash.
class VescUpload {
  UpdateStorage &storage;
  size_t declared = 0, size = 0, end = 0;
  uint16_t checksum = 0;
  bool header = false, opened = false;

public:

  explicit VescUpload(UpdateStorage &value) : storage(value) {
  }

  bool active() const {
    return opened;
  }

  size_t received() const {
    return end;
  }

  size_t total() const {
    return size ? size : declared;
  }

  void abort() {
    if (opened)
      storage.abort();
    declared = size = end = 0;
    header = opened = false;
  }

  bool begin(size_t length) {
    abort();
    if (!length || length > storage.capacity() || !storage.begin(length))
      return false;
    declared = length;
    opened = true;
    return true;
  }

  bool write(uint32_t offset, std::span<const uint8_t> bytes) {
    if (!opened || bytes.empty())
      return false;
    if (offset == 0) {
      if (bytes.size() < 6)
        return false;
      const size_t length = uint32_t(bytes[0]) << 24 | uint32_t(bytes[1]) << 16 | uint32_t(bytes[2]) << 8 | bytes[3];
      const uint16_t crc = uint16_t(bytes[4]) << 8 | bytes[5];
      if (!length || (length != declared && length + 6 != declared) || length > storage.capacity() ||
          (header && (length != size || crc != checksum)))
        return false;
      size = length;
      checksum = crc;
      header = true;
      bytes = bytes.subspan(6);
    }
    else {
      if (!header || offset < 6)
        return false;
      offset -= 6;
    }
    if (offset > size || bytes.size() > size - offset)
      return false;
    if (offset < end) {
      if (offset + bytes.size() > end)
        return false;
      std::array<uint8_t, 256> previous{};
      for (size_t at = 0; at < bytes.size(); at += previous.size()) {
        const auto count = std::min(previous.size(), bytes.size() - at);
        if (!storage.read(offset + at, std::span(previous).first(count)) ||
            !std::equal(previous.begin(), previous.begin() + count, bytes.begin() + at))
          return false;
      }
      return true;
    }
    if (!bytes.empty() && !storage.write(offset, bytes))
      return false;
    end = offset + bytes.size();
    return true;
  }

  bool finish() {
    if (!opened || !header)
      return false;
    uint16_t crc = 0;
    std::array<uint8_t, 512> bytes{};
    for (size_t offset = 0; offset < size; offset += bytes.size()) {
      const auto count = std::min(bytes.size(), size - offset);
      if (!storage.read(offset, std::span(bytes).first(count))) {
        abort();
        return false;
      }
      for (size_t i = 0; i < count; ++i) {
        crc ^= uint16_t(bytes[i]) << 8;
        for (int bit = 0; bit < 8; ++bit)
          crc = (crc & 0x8000) ? uint16_t((crc << 1) ^ 0x1021) : uint16_t(crc << 1);
      }
    }
    if (crc != checksum || !storage.finish()) {
      abort();
      return false;
    }
    opened = false;
    return true;
  }
};

} // namespace bike
