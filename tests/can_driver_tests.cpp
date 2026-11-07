#include "can_driver.hpp"
#include "fake_sdk.hpp"
#include <cassert>
#include <iostream>

int main() {
  transport::CanDriver driver;
  assert(!driver.healthy());
  fail_allocation = true;
  assert(driver.open(1, 2, 500000) == ESP_ERR_NO_MEM);
  fail_allocation = false;
  for (fail_step = 1; fail_step <= 4; ++fail_step) {
    assert(driver.open(1, 2, 500000) != ESP_OK);
    assert(node == nullptr && allocations == 0);
  }
  fail_step = 0;
  assert(driver.open(1, 2, 500000) == ESP_OK && driver.healthy());
  bike::CanFrame raw;
  raw.extended = false;
  raw.id = 0x7ff;
  assert(driver.send(raw));
  assert(!node->pending.back()->header.ide && node->pending.back()->header.id == 0x7ff &&
         node->pending.back()->header.dlc == 0);
  finish_tx(0);
  assert(driver.healthy());
  raw.id = 0x800;
  assert(!driver.send(raw));
  raw.extended = true;
  raw.id = 0x20000000;
  assert(!driver.send(raw));
  raw.id = 0x1fffffff;
  raw.size = 8;
  assert(driver.send(raw) && node->pending.back()->header.ide);
  finish_tx(0);
  assert(driver.healthy());
  raw.remote = true;
  assert(!driver.send(raw));
  raw.remote = false;
  raw.size = 9;
  assert(!driver.send(raw) && node->pending.empty());
  bike::CanFrame input;
  input.id = 0x1234;
  input.size = 8;
  input.data.fill(19);
  for (unsigned i = 0; i < 8; ++i)
    assert(driver.send(input));
  input.data.fill(99);
  for (auto *queued : node->pending) {
    assert(queued->header.id == 0x1234 && queued->buffer_len == 8);
    for (unsigned i = 0; i < 8; ++i)
      assert(queued->buffer[i] == 19);
  }
  // A full asynchronous queue cannot overwrite frames still owned by hardware.
  assert(!driver.send(input));
  auto *completed = node->pending[3];
  finish_tx(3);
  assert(driver.send(input));
  assert(node->pending.back() == completed && completed->buffer[0] == 99);
  while (!node->pending.empty())
    finish_tx(0);
  assert(driver.healthy());
  // An ISR may report completion before the enqueue API returns. Ownership
  // remains with the slot until the next worker-side completion drain.
  complete_inside_transmit = true;
  for (unsigned i = 0; i < 20; ++i) {
    assert(driver.send(input));
    assert(node->pending.empty());
  }
  assert(driver.healthy());
  complete_inside_transmit = false;
  fail_transmit = true;
  for (unsigned i = 0; i < 12; ++i)
    assert(!driver.send(input));
  fail_transmit = false;
  assert(driver.send(input));
  finish_tx(0, false);
  assert(!driver.healthy());
  driver.close();
  assert(allocations == 0);
  assert(driver.open(1, 2, 500000) == ESP_OK);
  assert(driver.send(input));
  clock_us += 249000;
  assert(driver.healthy());
  clock_us += 1000;
  assert(!driver.healthy());
  // Cancellation destroys the hardware node before reclaiming pending storage.
  driver.close();
  assert(allocations == 0);
  assert(driver.open(1, 2, 500000) == ESP_OK);
  bike::CanFrame output;
  deliver_rx(0x12, 8, 42, true);
  deliver_rx(0x12, 9, 42);
  deliver_rx(0x12, 8, 42, false, true);
  deliver_rx(0x12, 8, 42, false, false, false);
  assert(!driver.receive(output) && driver.healthy());
  for (unsigned i = 0; i < 64; ++i)
    deliver_rx(0x1801 + i, 8, uint8_t(i));
  for (unsigned i = 0; i < 64; ++i) {
    assert(driver.receive(output));
    assert(output.id == 0x1801 + i && output.size == 8 && output.extended && !output.remote);
    for (auto b : output.data)
      assert(b == i);
  }
  assert(!driver.receive(output) && driver.healthy());
  for (unsigned i = 0; i < 80; ++i)
    deliver_rx(0x1801, 8, 4);
  assert(!driver.healthy());
  driver.close();
  assert(driver.open(1, 2, 500000) == ESP_OK);
  node->bus_off = true;
  assert(!driver.healthy() && !driver.send(input));
  driver.close();
  driver.close();
  assert(allocations == 0 && create_count == delete_count);
  std::cout << "CAN driver ownership, ISR receive, overflow, timeout, cancellation and startup failures passed\n";
}
