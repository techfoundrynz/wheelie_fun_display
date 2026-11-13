#pragma once
#include <array>
#include <cstdint>

namespace bike
{

// One bounded negotiation per physical BLE session. A peer override after
// success is authoritative; a missing completion never starts overlapping work.
class BleLinkParameters {
public:

  struct Action {
    uint16_t interval = 0; // 1.25 ms units; zero means no request.
    bool timed_out = false;
  };

private:

  static constexpr std::array<uint16_t, 3> intervals{6, 12, 24};
  uint32_t session = 0;
  uint64_t due = 0, deadline = 0;
  unsigned candidate = 0, busy_retries = 0;
  bool armed = false, waiting = false, done = false;

  void advance(uint64_t now) {
    waiting = false;
    busy_retries = 0;
    done = ++candidate == intervals.size();
    due = now + 500;
  }

public:

  Action next(uint64_t now, uint32_t current_session, bool ready, uint16_t actual_interval, uint16_t actual_latency,
              bool permitted = true) {
    if (!ready || current_session != session) {
      *this = {};
      session = current_session;
    }
    if (!ready || done)
      return {};
    if (!armed) {
      armed = true;
      due = now + 2000; // Let subscription, pairing and initial peer updates settle.
    }
    if (waiting) {
      if (now >= deadline) {
        done = true;
        waiting = false;
        return {0, true};
      }
      return {};
    }
    if (!permitted || now < due || !actual_interval)
      return {};
    // Never slow down a link which already beats the next fallback.
    if (actual_interval <= intervals[candidate] && actual_latency == 0) {
      done = true;
      return {};
    }
    waiting = true;
    deadline = now + 8000;
    return {intervals[candidate], false};
  }

  void submitted(uint32_t current_session, uint64_t now, bool accepted, bool transient) {
    if (current_session != session || !waiting || done || accepted)
      return;
    waiting = false;
    if (transient) {
      if (busy_retries++ < 3)
        due = now + 500;
      else
        done = true;
    }
    else
      advance(now);
  }

  void completed(uint32_t current_session, uint64_t now, bool success) {
    if (current_session != session || !waiting || done)
      return;
    if (success) {
      waiting = false;
      done = true;
    }
    else
      advance(now);
  }
};

} // namespace bike
