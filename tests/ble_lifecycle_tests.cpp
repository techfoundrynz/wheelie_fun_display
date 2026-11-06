#include "bike/ble_lifecycle.hpp"
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
