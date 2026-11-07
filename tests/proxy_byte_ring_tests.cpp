#include "bike/proxy_byte_ring.hpp"
#include <array>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <random>
#include <vector>

#define CHECK(c)                                                                                                       \
  do {                                                                                                                 \
    if (!(c)) {                                                                                                        \
      std::cerr << __LINE__ << ": " #c "\n";                                                                           \
      std::exit(1);                                                                                                    \
    }                                                                                                                  \
  } while (0)

void startup_burst() {
  bike::ProxyByteRing<> ring;
  ring.reset(7);
  std::vector<uint8_t> sent;
  std::array<uint8_t, 20> fragment;
  // Eight hundred small ATT writes fit by bytes, rather than filling twelve
  // packet slots with only 240 useful bytes while the worker is briefly busy.
  for (unsigned packet = 0; packet < 800; ++packet) {
    for (size_t i = 0; i < fragment.size(); ++i)
      fragment[i] = uint8_t(packet * fragment.size() + i);
    CHECK(ring.append(fragment, 7));
    sent.insert(sent.end(), fragment.begin(), fragment.end());
  }
  CHECK(ring.size() == 16000 && ring.high_water() == 16000 && ring.free() == 384);
  std::vector<uint8_t> received;
  std::array<uint8_t, 512> batch;
  unsigned batches = 0;
  while (ring.size()) {
    const auto before = ring.size();
    const auto count = ring.copy(batch);
    CHECK(ring.size() == before); // Simulated upstream backpressure does not consume.
    std::array<uint8_t, 512> retry;
    CHECK(ring.copy(retry) == count && std::equal(batch.begin(), batch.begin() + count, retry.begin()));
    received.insert(received.end(), batch.begin(), batch.begin() + count);
    CHECK(ring.consume(count));
    ++batches;
  }
  CHECK(received == sent && batches == 32);
  CHECK(ring.pushed() == 16000 && ring.consumed() == 16000 && ring.high_water() == 16000);
}

void bounded_slow_consumer() {
  bike::ProxyByteRing<> ring;
  ring.reset(1);
  std::deque<uint8_t> expected;
  std::mt19937 rng(0x4120);
  std::array<uint8_t, 185> input;
  std::array<uint8_t, 1024> output;
  bool saw_full = false, saw_wrap = false;
  uint64_t pushed = 0, consumed = 0;
  for (unsigned step = 0; step < 120000; ++step) {
    const size_t count = 1 + rng() % input.size();
    for (size_t i = 0; i < count; ++i)
      input[i] = uint8_t(rng());
    const bool fits = count <= ring.free();
    CHECK(ring.append(std::span(input).first(count)) == fits);
    if (fits) {
      expected.insert(expected.end(), input.begin(), input.begin() + count);
      pushed += count;
    }
    else
      saw_full = true;
    // Producer can outrun the consumer; failed appends must be all-or-nothing.
    if (step % 16 == 0) {
      const size_t limit = 1 + rng() % output.size();
      const auto count_out = ring.pop(std::span(output).first(limit));
      for (size_t i = 0; i < count_out; ++i) {
        CHECK(!expected.empty() && output[i] == expected.front());
        expected.pop_front();
      }
      consumed += count_out;
      saw_wrap |= consumed > 16384;
    }
    CHECK(ring.size() == expected.size() && ring.free() + ring.size() == 16384);
    CHECK(ring.pushed() == pushed && ring.consumed() == consumed);
    const auto before = ring.size();
    CHECK(!ring.consume(before + 1) && ring.size() == before);
  }
  CHECK(saw_full && saw_wrap && ring.high_water() <= 16384);
  while (!expected.empty()) {
    const auto count = ring.pop(output);
    for (size_t i = 0; i < count; ++i) {
      CHECK(output[i] == expected.front());
      expected.pop_front();
    }
  }
  CHECK(ring.size() == 0 && ring.pushed() == ring.consumed());
}

void epoch_and_small_capacity() {
  bike::ProxyByteRing<1> tiny;
  const std::array<uint8_t, 1> first{42}, next{99};
  std::array<uint8_t, 1> output{};
  CHECK(tiny.append(first) && !tiny.append(next));
  CHECK(tiny.pop(output) == 1 && output == first);
  CHECK(tiny.append(next) && tiny.pop(output) == 1 && output == next);
  bike::ProxyByteRing<31> ring;
  ring.reset(55);
  CHECK(ring.append(first, 55));
  ring.reset(56);
  CHECK(ring.epoch() == 56 && ring.size() == 0 && ring.free() == 31);
  CHECK(ring.high_water() == 0 && ring.pushed() == 0 && ring.consumed() == 0);
  CHECK(!ring.append(first, 55));
  CHECK(ring.append(next, 56) && ring.pop(output) == 1 && output == next);
  CHECK(ring.append({}) && ring.consume(0));
  CHECK(ring.copy({}) == 0 && ring.pop({}) == 0);
}

void offset_copy_bounds_and_wrap() {
  bike::ProxyByteRing<31> ring;
  std::array<uint8_t, 25> first{};
  for (size_t i = 0; i < first.size(); ++i)
    first[i] = uint8_t(i);
  CHECK(ring.append(first) && ring.consume(20));
  CHECK(ring.append(first)); // Logical bytes 20..24 followed by 0..24 cross storage end.
  for (size_t offset = 0; offset < 35; ++offset) {
    std::array<uint8_t, 64> out;
    out.fill(255);
    const auto count = ring.copy(out, offset);
    CHECK(count == (offset < 30 ? 30 - offset : 0));
    for (size_t i = 0; i < count; ++i) {
      const auto index = offset + i;
      CHECK(out[i] == (index < 5 ? index + 20 : index - 5));
    }
    CHECK(out[count] == 255 && ring.size() == 30 && ring.consumed() == 20);
  }
  std::array<uint8_t, 1> out{255};
  CHECK(ring.copy(out, size_t(-1)) == 0 && out[0] == 255);
}

int main() {
  startup_burst();
  offset_copy_bounds_and_wrap();
  bounded_slow_consumer();
  epoch_and_small_capacity();
  std::cout << "Byte-ring startup bursts, coalescing, bounded pressure, wrap and epochs passed\n";
}
