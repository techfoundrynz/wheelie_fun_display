#include "bike/proxy_lease.hpp"
#include <cstdlib>
#include <iostream>

#define CHECK(condition)                                                                                               \
  do {                                                                                                                 \
    if (!(condition)) {                                                                                                \
      std::cerr << __LINE__ << ": " #condition "\n";                                                                   \
      std::abort();                                                                                                    \
    }                                                                                                                  \
  } while (false)

int main() {
  CHECK(bike::proxy_accepts_tx(0, 0, 0));
  CHECK(!bike::proxy_accepts_tx(0, 7, 0)); // Claim immediately closes the dashboard queue.
  CHECK(bike::proxy_continues_tx(0, 7));   // An already-started complete dashboard packet finishes.
  CHECK(!bike::proxy_accepts_tx(7, 7, 0)); // Phone waits for the TX-worker packet boundary.
  CHECK(bike::proxy_accepts_tx(7, 7, 7));
  CHECK(!bike::proxy_continues_tx(7, 0));  // Revocation stops the old phone fragment.
  CHECK(!bike::proxy_accepts_tx(7, 8, 8)); // A reconnect cannot revive the old phone queue.
  bike::ProxyLease lease;
  CHECK(!lease.enabled && !lease.advertise());
  CHECK(!lease.accepts(lease.generation, lease.upstream_session));
  lease.synced = true;
  lease.set_upstream(true, 12);
  CHECK(!lease.advertise()); // Even with a bike, proxy stays off by default.
  lease.enabled = true;
  CHECK(lease.advertise());
  const auto before = lease.generation;
  lease.attach();
  CHECK(lease.connected && !lease.advertise() && !lease.subscribed);
  CHECK(!lease.begin_maintenance()); // A connected but unsubscribed phone owns UART too.
  CHECK(!lease.accepts(lease.generation, 12));
  CHECK(lease.accepts_display(lease.generation, 12)); // Local read-only telemetry continues while pairing/unsubscribed.
  lease.paused = true;
  CHECK(!lease.accepts_display(lease.generation, 12));
  lease.paused = false;
  CHECK(!lease.accepts_display(lease.generation - 1, 12));
  lease.subscribed = true;
  CHECK(lease.accepts(lease.generation, 12));
  CHECK(!lease.accepts(before, 12));
  CHECK(!lease.accepts(lease.generation, 11));
  const auto phone_epoch = lease.generation;
  CHECK(!lease.set_upstream(true, 12));
  CHECK(lease.generation == phone_epoch && lease.accepts(phone_epoch, 12));
  CHECK(lease.set_upstream(false, 13));
  CHECK(!lease.accepts(phone_epoch, 12));
  lease.detach();
  CHECK(!lease.connected && !lease.subscribed);
  lease.set_upstream(true, 14);
  lease.attach();
  lease.subscribed = true;
  CHECK(lease.accepts(lease.generation, 14));
  CHECK(!lease.accepts(phone_epoch, 14)); // Reconnection never revives queued bytes.
  lease.enabled = false;
  CHECK(!lease.accepts(lease.generation, 14));
  CHECK(!lease.accepts_display(lease.generation, 14));
  lease.detach();
  CHECK(lease.begin_maintenance());
  lease.enabled = true;
  CHECK(!lease.advertise());
  lease.paused = false;
  CHECK(lease.advertise());

  // Every flag combination: privacy, subscription and ownership gates must all apply.
  for (unsigned flags = 0; flags < 64; ++flags) {
    bike::ProxyLease state;
    state.enabled = flags & 1;
    state.paused = flags & 2;
    state.synced = flags & 4;
    state.upstream_ready = flags & 8;
    state.connected = flags & 16;
    state.subscribed = flags & 32;
    const bool available = state.enabled && !state.paused && state.synced && state.upstream_ready;
    CHECK(state.advertise() == (available && !state.connected));
    CHECK(state.accepts(state.generation, state.upstream_session) ==
          (available && state.connected && state.subscribed));
    CHECK(!state.accepts(state.generation + 1, state.upstream_session));
    CHECK(!state.accepts(state.generation, state.upstream_session + 1));
  }
  std::cout << "Proxy default-off, ownership, maintenance and stale-stream guards passed\n";
}
