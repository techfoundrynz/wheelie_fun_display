#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace bike
{

// VESC datatypes.h firmware/bootloader/Black Magic programmer command IDs.
// Raw CAN forwarding can carry these operations without a visible serial ID.
constexpr bool proxy_firmware_session_command(uint8_t command) {
  return (command >= 1 && command <= 3) || command == 29 || (command >= 59 && command <= 61) ||
         (command >= 66 && command <= 74) || (command >= 80 && command <= 83) || command == 85 ||
         (command >= 102 && command <= 109);
}

struct ProxyReplyKey {
  std::array<uint8_t, 4> bytes{};
  uint8_t size = 0;

  bool matches(std::span<const uint8_t> prefix) const {
    if (!size || prefix.size() < size)
      return false;
    for (size_t i = 0; i < size; ++i)
      if (bytes[i] != prefix[i])
        return false;
    return true;
  }

  bool operator==(const ProxyReplyKey &) const = default;
};

class ProxyPollQueueCredits {
  struct Entry {
    ProxyReplyKey key;
    uint32_t epoch = 0;
  };

  std::array<Entry, 8> entries_{};

public:

  bool claim(ProxyReplyKey key, uint32_t epoch) {
    for (const auto &entry : entries_)
      if (entry.key == key && entry.epoch == epoch)
        return false;
    for (auto &entry : entries_)
      if (entry.epoch != epoch) {
        entry = {key, epoch};
        return true;
      }
    return false;
  }

  void release(ProxyReplyKey key, uint32_t epoch) {
    for (auto &entry : entries_)
      if (entry.key == key && entry.epoch == epoch)
        entry.epoch = 0;
  }
};

// Observes VESC framing without buffering the payload or limiting transparent
// phone packets to the dashboard decoder's capacity. Supports all length forms.
class ProxyFrameTracker {
  uint32_t remaining_ = 0, length_ = 0;
  uint16_t crc_ = 0, expected_crc_ = 0;
  uint8_t length_bytes_ = 0, stage_ = 0;
  std::array<uint8_t, 6> prefix_{};
  uint8_t prefix_size_ = 0;

public:

  bool boundary() const {
    return stage_ == 0;
  }

  std::span<const uint8_t> prefix() const {
    return {prefix_.data(), prefix_size_};
  }
  enum class Result {
    More,
    Complete,
    Invalid
  };

  Result byte(uint8_t value) {
    if (stage_ == 0) {
      if (value < 2 || value > 4)
        return Result::Invalid;
      length_bytes_ = value - 1;
      length_ = 0;
      prefix_size_ = 0;
      crc_ = expected_crc_ = 0;
      stage_ = 1;
    }
    else if (stage_ == 1) {
      length_ = (length_ << 8) | value;
      if (!--length_bytes_) {
        if (!length_) {
          stage_ = 0;
          return Result::Invalid;
        }
        remaining_ = length_;
        stage_ = 2;
      }
    }
    else if (stage_ == 2) {
      if (prefix_size_ < prefix_.size())
        prefix_[prefix_size_++] = value;
      crc_ ^= uint16_t(value) << 8;
      for (unsigned bit = 0; bit < 8; ++bit)
        crc_ = (crc_ & 0x8000) ? uint16_t((crc_ << 1) ^ 0x1021) : uint16_t(crc_ << 1);
      if (!--remaining_)
        stage_ = 3;
    }
    else if (stage_ == 3) {
      expected_crc_ = uint16_t(value) << 8;
      stage_ = 4;
    }
    else if (stage_ == 4) {
      expected_crc_ |= value;
      stage_ = 5;
    }
    else {
      stage_ = 0;
      return value == 3 && crc_ == expected_crc_ ? Result::Complete : Result::Invalid;
    }
    return Result::More;
  }
};

// Read-only requests are registered by the transport, which owns integration
// and controller routing. The broker knows only opaque reply prefixes.
class ProxyPollArbiter {
  struct KeyState {
    ProxyReplyKey key;
    uint32_t phone_replies = 0;
  };

  std::array<KeyState, 8> keys_{};
  ProxyFrameTracker phone_, upstream_;
  ProxyReplyKey active_{};
  ProxyReplyKey reserved_{};
  uint64_t deadline_ = 0;
  uint64_t reservation_until_ = 0;
  uint32_t phone_since_poll_ = 0;
  bool firmware_ = false, probing_ = false, suppressing_ = false, broken_ = false;
  std::array<uint8_t, 9> held_{};
  size_t held_size_ = 0;

public:

  bool register_key(ProxyReplyKey key) {
    if (!key.size || key.size > key.bytes.size())
      return false;
    for (auto &entry : keys_)
      if (entry.key == key)
        return true;
    for (auto &entry : keys_)
      if (!entry.key.size) {
        entry.key = key;
        return true;
      }
    return false;
  }

  void reset() {
    phone_ = {};
    upstream_ = {};
    active_ = {};
    reserved_ = {};
    deadline_ = 0;
    reservation_until_ = 0;
    phone_since_poll_ = 0;
    firmware_ = probing_ = suppressing_ = broken_ = false;
    held_size_ = 0;
    for (auto &entry : keys_)
      entry.phone_replies = 0;
  }

  bool pending() const {
    return active_.size != 0;
  }

  bool may_send_phone() const {
    return !pending() && !reserved_.size;
  }

  bool phone_boundary() const {
    return phone_.boundary();
  }

  uint32_t phone_since_poll() const {
    return phone_since_poll_;
  }

  bool upstream_boundary() const {
    return upstream_.boundary();
  }

  bool may_forward_phone(std::span<const uint8_t> prefix) const {
    if (prefix.size() >= 3 && prefix[0] == 34)
      prefix = prefix.subspan(2);
    if ((active_.size && active_.matches(prefix)) || (reserved_.size && reserved_.matches(prefix)))
      return false;
    return !(active_.size && !prefix.empty() && proxy_firmware_session_command(prefix[0]));
  }

  void abandon_display_polls() {
    active_ = reserved_ = {};
    deadline_ = reservation_until_ = 0;
    probing_ = suppressing_ = false;
    held_size_ = 0;
  }

  bool yield_phone_for_poll(bool poll_waiting) const {
    return poll_waiting && (reserved_.size || phone_since_poll_ >= 3) && phone_.boundary() && !firmware_ && !broken_;
  }

  bool reserve(ProxyReplyKey key, uint64_t now) {
    if (reserved_.size)
      return reserved_ == key;
    if (!yield_phone_for_poll(true) || pending())
      return false;
    reserved_ = key;
    reservation_until_ = now + 200;
    return true;
  }

  bool reserved() const {
    return reserved_.size != 0;
  }

  bool reservation_expired(uint64_t now, size_t phone_queued = 0) const {
    // The physical ingress queue has 12 slots. An unsent display request must
    // yield before its temporary drain window could overflow that phone queue.
    return reserved() && (now >= reservation_until_ || phone_queued >= 8);
  }

  void release_reservation() {
    reserved_ = {};
    reservation_until_ = 0;
    phone_since_poll_ = 0;
  }

  bool broken() const {
    return broken_;
  }

  bool timed_out(uint64_t now) const {
    return pending() && now >= deadline_;
  }

  uint32_t phone_replies(ProxyReplyKey key) const {
    for (const auto &entry : keys_)
      if (entry.key == key)
        return entry.phone_replies;
    return 0;
  }

  bool may_poll(ProxyReplyKey key, bool phone_waiting = false) const {
    if (phone_waiting || broken_ || firmware_ || pending() || !phone_.boundary())
      return false;
    if (!upstream_.boundary()) {
      // UART/BLE directions are independent. An already-forwarded phone reply
      // may finish while we send an unrelated request; preserve its frame role
      // and classify the later display reply only at its own frame boundary.
      if (probing_ || suppressing_)
        return false;
      const auto prefix = upstream_.prefix();
      bool disjoint = false;
      for (size_t i = 0; i < std::min<size_t>(key.size, prefix.size()); ++i)
        disjoint |= key.bytes[i] != prefix[i];
      if (!disjoint)
        return false; // Unknown/matching prefix: do not infer a different owner.
    }
    if (reserved_.size && reserved_ != key)
      return false;
    for (const auto &entry : keys_)
      if (entry.key == key)
        return !entry.phone_replies;
    return false;
  }

  bool begin(ProxyReplyKey key, uint64_t now, bool phone_waiting = false) {
    if (!may_poll(key, phone_waiting))
      return false;
    active_ = key;
    deadline_ = now + 3000;
    reserved_ = {};
    reservation_until_ = 0;
    phone_since_poll_ = 0;
    return true;
  }

  // Called only once the complete fragment has entered the upstream TX queue.
  bool phone(std::span<const uint8_t> bytes) {
    for (auto byte : bytes) {
      const auto result = phone_.byte(byte);
      if (result == ProxyFrameTracker::Result::Invalid)
        return !(broken_ = true);
      if (result == ProxyFrameTracker::Result::Complete) {
        if (phone_since_poll_ < 3)
          ++phone_since_poll_;
        auto prefix = phone_.prefix();
        if (prefix.size() >= 3 && prefix[0] == 34)
          prefix = prefix.subspan(2); // Serial CAN-forward wrapper, not a controller selection.
        // Controller firmware update/reboot sessions must not receive injected
        // requests in a bootloader or while a flash transaction is in progress.
        if (!prefix.empty() && proxy_firmware_session_command(prefix[0]))
          firmware_ = true;
        for (auto &entry : keys_)
          if (entry.key.matches(prefix))
            ++entry.phone_replies;
      }
    }
    return true;
  }

  // Emits every phone byte unchanged, excluding only the broker's own reply.
  // Header probing is bounded; even multi-megabyte phone replies stay streaming.
  template <class Emit, class Owned> bool upstream(std::span<const uint8_t> bytes, Emit emit, Owned owned) {
    for (auto byte : bytes) {
      if (upstream_.boundary()) {
        probing_ = pending();
        suppressing_ = false;
        held_size_ = 0;
      }
      const auto result = upstream_.byte(byte);
      if (probing_) {
        if (held_size_ == held_.size())
          return !(broken_ = true);
        held_[held_size_++] = byte;
        if (upstream_.prefix().size() >= active_.size || result != ProxyFrameTracker::Result::More) {
          suppressing_ = active_.matches(upstream_.prefix());
          probing_ = false;
          if (!suppressing_)
            for (size_t i = 0; i < held_size_; ++i)
              emit(held_[i]);
          else
            for (size_t i = 0; i < held_size_; ++i)
              owned(held_[i]);
          held_size_ = 0;
        }
      }
      else if (!suppressing_)
        emit(byte);
      else
        owned(byte);
      if (result == ProxyFrameTracker::Result::Invalid)
        return !(broken_ = true);
      if (result == ProxyFrameTracker::Result::Complete) {
        if (suppressing_)
          active_ = {};
        else
          for (auto &entry : keys_)
            if (entry.key.matches(upstream_.prefix()) && entry.phone_replies)
              --entry.phone_replies;
      }
    }
    return true;
  }

  template <class Emit> bool upstream(std::span<const uint8_t> bytes, Emit emit) {
    return upstream(bytes, emit, [](uint8_t) {});
  }
};

} // namespace bike
