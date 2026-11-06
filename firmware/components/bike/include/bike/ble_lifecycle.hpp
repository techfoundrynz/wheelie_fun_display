#pragma once
#include "link_status.hpp"
#include <cstdint>

namespace bike
{

// A selected address is a candidate, not a successful connection or an SMP
// bond. Remember it only after the VESC UART notification subscription works.
struct BleLifecycle {
  LinkStatus status = LinkStatus::Disconnected;
  bool remembered = false, scanning = false, paused = false, security_blocked = false;
  uint32_t generation = 1, session = 0;

  void restore(bool saved) {
    remembered = saved;
    status = saved ? LinkStatus::Retry : LinkStatus::Disconnected;
  }

  bool selectable() const {
    return !paused && (status == LinkStatus::Disconnected || status == LinkStatus::Scanning ||
                       status == LinkStatus::Retry || status == LinkStatus::SecurityRequired);
  }

  bool can_scan() const {
    return !paused && !security_blocked && !scanning &&
           (status == LinkStatus::Disconnected || status == LinkStatus::Retry);
  }

  void start_scan() {
    scanning = true;
    status = remembered ? LinkStatus::Scanning : LinkStatus::Disconnected;
  }

  void select(bool saved_candidate) {
    remembered = saved_candidate;
    security_blocked = false;
    scanning = false;
    ++generation;
    status = LinkStatus::Connecting;
  }

  bool current(uint32_t token) const {
    return token == generation && !paused;
  }

  bool connected(uint32_t token) {
    if (!current(token) || status != LinkStatus::Connecting)
      return false;
    status = LinkStatus::Discovering;
    return true;
  }

  bool subscribed(uint32_t token) {
    if (!current(token) || status != LinkStatus::Discovering)
      return false;
    status = LinkStatus::Ready;
    remembered = true;
    ++session;
    return true;
  }

  void retry() {
    scanning = false;
    ++generation;
    ++session;
    status = paused             ? LinkStatus::Paused
             : security_blocked ? LinkStatus::SecurityRequired
             : remembered       ? LinkStatus::Retry
                                : LinkStatus::Disconnected;
  }

  void security_required() {
    security_blocked = true;
    status = LinkStatus::SecurityRequired;
  }

  void forget() {
    remembered = false;
    security_blocked = false;
    retry();
  }

  void set_paused(bool value) {
    if (paused == value)
      return;
    paused = value;
    retry();
  }
};

} // namespace bike
