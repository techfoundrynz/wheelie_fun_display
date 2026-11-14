#include "telemetry_test.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
  telemetry_test::Clock clock;
  for (uint64_t us = 0; us < 1000000; ++us)
    if (clock.poll(us)) {
      assert(clock.next_us() > us);
      assert(!clock.poll(us));
    }
  assert(clock.updates == 60);
  assert(clock.skipped == 0);
  assert(clock.next_us() == 1000000);
  assert(clock.poll(1000000));
  assert(clock.poll(1100000));
  assert(clock.skipped == 5);
  assert(!clock.poll(1100000));
  assert(clock.next_us() == 1116667);

  // Render overruns skip slots rather than creating catch-up bursts or drift.
  telemetry_test::Clock loaded;
  for (uint64_t us = 0; us < 60000000; us += 23000)
    assert(loaded.poll(us));
  assert(loaded.updates + loaded.skipped == loaded.slot + 1);

  const auto first = telemetry_test::sample(0);
  bool speed_changed = false, power_changed = false, cells_changed = false;
  for (uint64_t frame = 0; frame < 3600; ++frame) {
    const auto us = (frame * 1000000 + 59) / 60;
    const auto data = telemetry_test::sample(us);
    assert(data.fresh(us / 1000));
    assert(data.bms.fresh(us / 1000));
    assert(data.speed_mps >= 1 && data.speed_mps <= 15);
    assert(data.battery_percent >= 0 && data.battery_percent <= 100);
    assert(data.duty_cycle >= 0 && data.duty_cycle <= 1);
    assert(std::isfinite(data.motor_current));
    assert(data.distance_abs_m >= first.distance_abs_m);
    assert(data.fault == 0);
    assert(data.bms.cell_count <= data.bms.cell_volts.size());
    assert(data.bms.temp_count <= data.bms.temps_c.size());
    for (unsigned i = 0; i < data.bms.cell_count; ++i)
      assert(data.bms.cell_volts[i] > 3 && data.bms.cell_volts[i] < 4.3);
    speed_changed |= data.speed_mps != first.speed_mps;
    power_changed |= data.battery_current * data.voltage != first.battery_current * first.voltage;
    cells_changed |= data.bms.cell_volts[0] != first.bms.cell_volts[0];
  }
  assert(speed_changed && power_changed && cells_changed);
  std::cout << "60 Hz cadence, missed slots and simulated telemetry passed\n";
}
