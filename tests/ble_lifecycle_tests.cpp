#include "bike/ble_lifecycle.hpp"
#include "bike/ble_signal.hpp"
#include "bike/proxy_tx_drain.hpp"
#include <cstdlib>
#include <iostream>

#define CHECK(condition)                                                                                               \
  do {                                                                                                                 \
    if (!(condition)) {                                                                                                \
      std::cerr << __LINE__ << ": " #condition "\n";                                                                   \
      std::exit(1);                                                                                                    \
    }                                                                                                                  \
  } while (false)

int main() {
  bike::ProxyTxDrain drain;
  CHECK(drain.drained());
  drain.reserve();
  CHECK(!drain.drained());
  drain.rejected(); // Failed enqueue must not leave phantom work.
  CHECK(drain.drained());
  drain.reserve();
  drain.reserve();
  {
    bike::ProxyTxDrain::WriteGuard writing(drain, 7);
    CHECK(!drain.drained());
  }
  CHECK(!drain.drained()); // Another queued batch remains.
  {
    bike::ProxyTxDrain::WriteGuard writing(drain, 7);
    CHECK(!drain.drained()); // Queue is empty, but this batch is still writing.
  }
  CHECK(drain.drained());
  drain.reserve();
  {
    // A canceled session/old epoch is discarded through exactly the same guard.
    bike::ProxyTxDrain::WriteGuard stale(drain, 8);
    CHECK(!drain.drained());
  }
  CHECK(drain.drained());
  {
    bike::ProxyTxDrain::WriteGuard dashboard(drain, 0);
    CHECK(drain.drained()); // Uncounted dashboard messages do not underflow it.
  }

  CHECK(bike::ble_signal_level(-127) == 0);
  CHECK(bike::ble_signal_level(-86) == 0);
  CHECK(bike::ble_signal_level(-85) == 1);
  CHECK(bike::ble_signal_level(-76) == 1);
  CHECK(bike::ble_signal_level(-75) == 2);
  CHECK(bike::ble_signal_level(-66) == 2);
  CHECK(bike::ble_signal_level(-65) == 3);
  CHECK(bike::ble_signal_level(-56) == 3);
  CHECK(bike::ble_signal_level(-55) == 4);
  CHECK(bike::ble_signal_level(0) == 4);

  using Status = bike::LinkStatus;
  bike::BleLifecycle fresh;
  fresh.restore(false);
  CHECK(fresh.status == Status::Disconnected);
  for (int i = 0; i < 100; ++i) {
    CHECK(fresh.can_scan());
    fresh.start_scan();
    CHECK(fresh.scanning && fresh.status == Status::Disconnected);
    fresh.retry();
    CHECK(fresh.status == Status::Disconnected && !fresh.remembered);
  }

  // A failed first attempt or failed service discovery does not become a
  // remembered bike. Late completions cannot promote a replacement session.
  fresh.select(false);
  const auto failed = fresh.generation;
  CHECK(fresh.status == Status::Connecting);
  fresh.retry();
  CHECK(!fresh.connected(failed));
  CHECK(fresh.status == Status::Disconnected);
  fresh.select(false);
  const auto discovery = fresh.generation;
  CHECK(fresh.connected(discovery));
  CHECK(fresh.status == Status::Discovering);
  fresh.retry();
  CHECK(!fresh.subscribed(discovery));
  CHECK(!fresh.remembered && fresh.status == Status::Disconnected);

  fresh.select(false);
  const auto working = fresh.generation;
  CHECK(!fresh.subscribed(working)); // Physical connect alone is insufficient.
  CHECK(fresh.connected(working));
  CHECK(fresh.subscribed(working));
  CHECK(fresh.remembered && fresh.status == Status::Ready);
  CHECK(!fresh.subscribed(working)); // No duplicate ready transition.
  fresh.retry();
  CHECK(fresh.status == Status::Retry);
  CHECK(!fresh.current(working));

  bike::BleLifecycle restored;
  restored.restore(true);
  CHECK(restored.status == Status::Retry && restored.remembered);
  restored.start_scan();
  CHECK(restored.status == Status::Scanning);
  restored.select(true);
  auto token = restored.generation;
  CHECK(restored.connected(token));
  CHECK(restored.subscribed(token));
  const auto session = restored.session;
  restored.set_paused(true);
  CHECK(restored.status == Status::Paused && !restored.current(token));
  CHECK(!restored.can_scan() && !restored.selectable());
  restored.set_paused(true);
  CHECK(restored.session == session + 1); // Repeated pause is idempotent.
  restored.set_paused(false);
  CHECK(restored.status == Status::Retry && restored.remembered);

  restored.select(false); // A different choice has not earned persistence yet.
  token = restored.generation;
  CHECK(restored.connected(token));
  restored.security_required();
  restored.retry();
  CHECK(restored.status == Status::SecurityRequired);
  CHECK(!restored.can_scan()); // Do not loop unsupported passkey requests.
  CHECK(restored.selectable());
  restored.forget();
  CHECK(restored.status == Status::Disconnected && !restored.remembered);
  CHECK(restored.can_scan() && !restored.current(token));

  // Forget while OTA is paused cannot silently restart Bluetooth.
  restored.set_paused(true);
  restored.forget();
  CHECK(restored.status == Status::Paused);
  restored.set_paused(false);
  CHECK(restored.status == Status::Disconnected);
  std::cout << "BLE lifecycle transitions passed\n";
}
