#pragma once
#include <atomic>
#include <cassert>
#include <cstdint>

namespace bike
{

// Counts accepted software-queue batches separately from the batch currently
// being written. The caller stops producers before using drained() for handoff.
// Never zero the count on a link reset: stale queued batches still own a count
// until the worker pops and discards them. RX queue resets do not affect it.
class ProxyTxDrain {
  std::atomic<uint32_t> queued_{0}, writing_epoch_{0};

public:

  void reserve() {
    queued_.fetch_add(1);
  }

  void rejected() {
    [[maybe_unused]] const auto previous = queued_.fetch_sub(1);
    assert(previous != 0);
  }

  class WriteGuard {
    ProxyTxDrain &owner;
    uint32_t epoch;

  public:

    WriteGuard(ProxyTxDrain &value, uint32_t packet_epoch) : owner(value), epoch(packet_epoch) {
      if (epoch) {
        // Publish the writer before removing the final queued count. A reader
        // observing zero queued work must still see this in-flight batch.
        owner.writing_epoch_.store(epoch);
        owner.rejected();
      }
    }

    WriteGuard(const WriteGuard &) = delete;

    WriteGuard &operator=(const WriteGuard &) = delete;

    ~WriteGuard() {
      if (epoch)
        owner.writing_epoch_.store(0);
    }
  };

  bool drained() const {
    // This order pairs with WriteGuard's writer-before-decrement transition.
    return queued_.load() == 0 && writing_epoch_.load() == 0;
  }
};

} // namespace bike
