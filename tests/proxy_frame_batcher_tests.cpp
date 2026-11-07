#include "bike/protocol.hpp"
#include "bike/proxy_frame_batcher.hpp"
#include "bike/proxy_tx_drain.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>

#define CHECK(c)                                                                                                       \
  do {                                                                                                                 \
    if (!(c)) {                                                                                                        \
      std::cerr << __LINE__ << ": " #c "\n";                                                                           \
      std::exit(1);                                                                                                    \
    }                                                                                                                  \
  } while (0)
using Bytes = std::vector<uint8_t>;

Bytes wire(size_t payload_size, uint8_t command = 47) {
  Bytes payload(payload_size);
  for (size_t i = 0; i < payload.size(); ++i)
    payload[i] = uint8_t(i);
  payload[0] = command;
  Bytes result;
  if (payload_size <= 255)
    result = {2, uint8_t(payload_size)};
  else if (payload_size <= 65535)
    result = {3, uint8_t(payload_size >> 8), uint8_t(payload_size)};
  else
    result = {4, uint8_t(payload_size >> 16), uint8_t(payload_size >> 8), uint8_t(payload_size)};
  result.insert(result.end(), payload.begin(), payload.end());
  const auto crc = bike::crc16(payload);
  result.push_back(uint8_t(crc >> 8));
  result.push_back(uint8_t(crc));
  result.push_back(3);
  return result;
}

void fragmented_startup_and_coalescing() {
  bike::ProxyByteRing<> ring;
  bike::ProxyFrameBatcher batcher;
  ring.reset(2);
  Bytes expected;
  // Six hundred 20-byte startup notifications containing 2,000 small complete
  // requests. Coalesce them without imposing a packet-slot limit.
  const auto request = wire(1);
  for (unsigned i = 0; i < 2000; ++i)
    expected.insert(expected.end(), request.begin(), request.end());
  CHECK(expected.size() == 12000);
  for (size_t i = 0; i < expected.size(); i += 20)
    CHECK(ring.append(std::span(expected).subspan(i, 20)));
  Bytes received;
  std::array<uint8_t, 512> output{};
  unsigned batches = 0;
  while (ring.size()) {
    const auto count = batcher.prepare(ring, output);
    CHECK(count && count <= output.size() && !batcher.invalid() && !batcher.partial_frame());
    const auto before = ring.size();
    std::array<uint8_t, 512> retry{};
    CHECK(batcher.prepare(ring, retry) == count);
    CHECK(std::equal(output.begin(), output.begin() + count, retry.begin()) && ring.size() == before);
    received.insert(received.end(), output.begin(), output.begin() + count);
    CHECK(batcher.commit(ring, count));
    CHECK(!batcher.commit(ring, count));
    ++batches;
  }
  CHECK(received == expected && batches == 24);
}

void large_and_abandoned_frames() {
  for (const size_t payload_size : {size_t(1), size_t(255), size_t(256), size_t(2048), size_t(16378)}) {
    const auto expected = wire(payload_size);
    bike::ProxyByteRing<> ring;
    bike::ProxyFrameBatcher batcher;
    ring.reset(5);
    std::array<uint8_t, 512> output{};
    // Every proper prefix, even with a full header, must emit nothing.
    for (size_t i = 0; i + 1 < expected.size(); ++i) {
      CHECK(ring.append(std::span(expected).subspan(i, 1)));
      CHECK(batcher.prepare(ring, output) == 0);
      CHECK(!batcher.invalid() && !batcher.partial_frame());
    }
    CHECK(ring.append(std::span(expected).last(1)));
    Bytes received;
    while (ring.size()) {
      const auto count = batcher.prepare(ring, output, [](auto) { return true; });
      CHECK(count && !batcher.invalid());
      received.insert(received.end(), output.begin(), output.begin() + count);
      CHECK(batcher.commit(ring, count));
      CHECK(batcher.partial_frame() == (ring.size() != 0));
    }
    CHECK(received == expected);
  }
  bike::ProxyByteRing<> ring;
  bike::ProxyFrameBatcher batcher;
  ring.reset(8);
  auto incomplete = wire(2048);
  CHECK(ring.append(std::span(incomplete).first(1024)));
  std::array<uint8_t, 512> output{};
  CHECK(batcher.prepare(ring, output) == 0 && !batcher.partial_frame());
  // Disconnect before the whole frame arrived: no bytes ever reached upstream.
  ring.reset(9);
  batcher.reset();
  const auto next = wire(1, 0);
  CHECK(ring.append(next));
  CHECK(batcher.prepare(ring, output) == next.size() && batcher.commit(ring, next.size()));
}

void invalid_and_epoch_cases() {
  for (unsigned mutation = 0; mutation < 3; ++mutation) {
    bike::ProxyByteRing<> ring;
    bike::ProxyFrameBatcher batcher;
    auto invalid = wire(2048);
    if (mutation == 0)
      invalid[1234] ^= 1;
    else if (mutation == 1)
      invalid.back() = 2;
    else
      invalid[0] = 1;
    CHECK(ring.append(invalid));
    std::array<uint8_t, 512> output{};
    CHECK(batcher.prepare(ring, output) == 0 && batcher.invalid());
    CHECK(ring.size() == invalid.size() && !batcher.partial_frame());
  }
  for (const Bytes &header : {Bytes{2, 0}, Bytes{3, 0, 1}, Bytes{4, 0, 1, 0}, Bytes{3, 0x40, 0}, Bytes{4, 1, 0, 0}}) {
    bike::ProxyByteRing<> ring;
    bike::ProxyFrameBatcher batcher;
    CHECK(ring.append(header));
    std::array<uint8_t, 512> output{};
    CHECK(batcher.prepare(ring, output) == 0 && batcher.invalid());
    CHECK(batcher.oversized() == (header[1] >= 0x40 || header == Bytes({4, 1, 0, 0})));
  }
  bike::ProxyByteRing<> ring;
  bike::ProxyFrameBatcher batcher;
  std::array<uint8_t, 512> output{};
  ring.reset(1);
  CHECK(ring.append(wire(2048)));
  const auto count = batcher.prepare(ring, output);
  CHECK(count == 512);
  ring.reset(2);
  CHECK(ring.append(wire(2048)));
  CHECK(!batcher.commit(ring, count));
  batcher.reset();
  CHECK(batcher.prepare(ring, output) == 512 && batcher.commit(ring, 512));
  CHECK(batcher.partial_frame());
  ring.reset(3);
  CHECK(ring.append(wire(2048)));
  CHECK(batcher.prepare(ring, output) == 0 && batcher.invalid());
}

void predicate_and_wrapped_peek() {
  bike::ProxyByteRing<> ring;
  bike::ProxyFrameBatcher batcher;
  std::array<uint8_t, 512> output{};
  // Force the validated frame header and payload across the physical ring edge.
  Bytes padding(16380);
  CHECK(ring.append(padding) && ring.consume(padding.size()));
  const auto first = wire(1, 47), blocked = wire(2048, 36);
  CHECK(ring.append(first) && ring.append(blocked));
  auto allow = [](std::span<const uint8_t> prefix) { return prefix[0] != 36; };
  CHECK(batcher.prepare(ring, output, allow) == first.size());
  CHECK(batcher.commit(ring, first.size()));
  CHECK(batcher.prepare(ring, output, allow) == 0 && !batcher.invalid());
  CHECK(batcher.prepare(ring, output) == 512 && batcher.commit(ring, 512));
  // Once a validated frame is started, a later predicate cannot cut it off.
  CHECK(batcher.prepare(ring, output, [](auto) { return false; }) == 512);
  CHECK(batcher.commit(ring, 512));
  CHECK(batcher.partial_frame());
}

void malformed_following_frame_does_not_cut_valid_tail() {
  bike::ProxyByteRing<> ring;
  bike::ProxyFrameBatcher batcher;
  const auto valid = wire(600);
  auto invalid = wire(1);
  invalid.back() = 0;
  CHECK(ring.append(valid) && ring.append(invalid));
  std::array<uint8_t, 512> output{};
  CHECK(batcher.prepare(ring, output) == 512 && batcher.commit(ring, 512));
  CHECK(batcher.partial_frame());
  const auto tail = valid.size() - 512;
  CHECK(batcher.prepare(ring, output) == tail && !batcher.invalid());
  CHECK(batcher.commit(ring, tail) && !batcher.partial_frame());
  CHECK(batcher.prepare(ring, output) == 0 && batcher.invalid());
  CHECK(ring.size() == invalid.size());
}

void fair_coalescing_counts_planned_frames() {
  bike::ProxyByteRing<> ring;
  bike::ProxyFrameBatcher batcher;
  bike::ProxyPollArbiter arbiter;
  std::array<uint8_t, 512> output{};
  const auto small = wire(1, 0);
  for (unsigned i = 0; i < 100; ++i)
    CHECK(ring.append(small));
  unsigned planned = 0;
  auto allow = [&](auto prefix) {
    if (arbiter.phone_since_poll() + planned >= 3 || !arbiter.may_forward_phone(prefix))
      return false;
    ++planned;
    return true;
  };
  const auto count = batcher.prepare(ring, output, allow);
  CHECK(count == 3 * small.size() && planned == 3);
  CHECK(batcher.commit(ring, count) && arbiter.phone(std::span(output).first(count)));
  CHECK(arbiter.phone_since_poll() == 3);
  planned = 0;
  CHECK(batcher.prepare(ring, output, allow) == 0); // Not another85 frames in the next512-byte batch.

  // A previously validated partial frame still finishes, then counts against
  // the local planned budget before another new frame may begin.
  ring.reset(1);
  batcher.reset();
  arbiter.reset();
  CHECK(arbiter.phone(small) && arbiter.phone(small));
  const auto large = wire(600, 0);
  CHECK(ring.append(large) && ring.append(small));
  CHECK(batcher.prepare(ring, output) == 512 && batcher.commit(ring, 512));
  CHECK(arbiter.phone(output));
  planned = batcher.partial_frame() ? 1 : 0;
  const auto tail = batcher.prepare(ring, output, allow);
  CHECK(tail == large.size() - 512);
  CHECK(batcher.commit(ring, tail) && arbiter.phone(std::span(output).first(tail)));
  CHECK(arbiter.phone_since_poll() == 3 && ring.size() == small.size());
}

void clean_phone_departure_finishes_already_started_frame() {
  bike::ProxyByteRing<> ring;
  bike::ProxyFrameBatcher batcher;
  bike::ProxyTxDrain drain;
  bike::ProxyFrameTracker remote;
  std::array<uint8_t, 512> output{};
  const auto complete = wire(600, 0);
  const auto abandoned = wire(80, 47);
  CHECK(ring.append(complete));
  CHECK(ring.append(std::span(abandoned).first(7)));
  std::vector<Bytes> accepted;
  auto admit = [&](size_t count) {
    drain.reserve();
    accepted.emplace_back(output.begin(), output.begin() + count);
    CHECK(batcher.commit(ring, count));
  };
  CHECK(batcher.prepare(ring, output) == 512);
  admit(512);
  CHECK(batcher.partial_frame() && !drain.drained());
  // Phone leaves here. Its valid frame remainder is already in the ring and
  // must still be admitted; the following incomplete frame must never be sent.
  const auto tail = batcher.prepare(ring, output);
  CHECK(tail == complete.size() - 512);
  admit(tail);
  CHECK(!batcher.partial_frame());
  CHECK(batcher.prepare(ring, output) == 0 && !batcher.invalid());
  CHECK(ring.consume(ring.size()));
  CHECK(!drain.drained()); // Empty ingress does not mean old queued/writing bytes finished.
  unsigned packets = 0;
  for (const auto &batch : accepted) {
    bike::ProxyTxDrain::WriteGuard writing(drain, 3);
    CHECK(!drain.drained());
    for (auto byte : batch) {
      const auto result = remote.byte(byte);
      CHECK(result != bike::ProxyFrameTracker::Result::Invalid);
      packets += result == bike::ProxyFrameTracker::Result::Complete;
    }
  }
  CHECK(drain.drained() && remote.boundary() && packets == 1);
}

int main() {
  fragmented_startup_and_coalescing();
  malformed_following_frame_does_not_cut_valid_tail();
  fair_coalescing_counts_planned_frames();
  clean_phone_departure_finishes_already_started_frame();
  large_and_abandoned_frames();
  invalid_and_epoch_cases();
  predicate_and_wrapped_peek();
  std::cout << "Frame validation, byte coalescing, large frames, abandoned tails and transactional handoff passed\n";
}
