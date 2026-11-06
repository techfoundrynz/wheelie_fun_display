#pragma once
#include <cstdint>

namespace bike
{

inline bool proxy_accepts_tx(uint32_t packet_epoch, uint32_t requested, uint32_t active) {
  return packet_epoch ? requested == packet_epoch && active == packet_epoch : requested == 0;
}

inline bool proxy_continues_tx(uint32_t packet_epoch, uint32_t requested) {
  // Dashboard packets finish atomically after a claim; revoked phone fragments
  // stop, and the caller resets the central stream before dashboard resumes.
  return !packet_epoch || packet_epoch == requested;
}

// Transport ownership is explicit because VESC UART replies carry no request ID.
// A phone conversation excludes dashboard requests, including before subscribe.
struct ProxyLease {
  bool enabled = false, paused = false, synced = false, upstream_ready = false;
  bool connected = false, subscribed = false;
  uint32_t generation = 1, upstream_session = 0;

  bool advertise() const {
    return enabled && !paused && synced && upstream_ready && !connected;
  }

  bool accepts(uint32_t epoch, uint32_t session) const {
    return enabled && !paused && synced && upstream_ready && connected && subscribed && epoch == generation &&
           session == upstream_session;
  }

  bool begin_maintenance() {
    if (connected)
      return false;
    paused = true;
    return true;
  }

  void attach() {
    ++generation;
    connected = true;
    subscribed = false;
  }

  void detach() {
    ++generation;
    connected = subscribed = false;
  }

  bool set_upstream(bool ready, uint32_t session) {
    if (ready == upstream_ready && session == upstream_session)
      return false;
    ++generation;
    upstream_ready = ready;
    upstream_session = session;
    return connected;
  }
};

} // namespace bike
