#include "bike/scanline_chunks.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <random>
#include <vector>

size_t overlapped_total = 0;

#define CHECK(condition)                                                                                               \
  do {                                                                                                                 \
    if (!(condition)) {                                                                                                \
      std::cerr << __LINE__ << ": " #condition "\n";                                                                   \
      std::abort();                                                                                                    \
    }                                                                                                                  \
  } while (false)

template <size_t W, size_t H, size_t Rows> void exercise(std::mt19937 &random, bool narrow = false) {
  struct Rect {
    size_t x0, x1, y0, y1;
  };

  std::array<Rect, 3> regions;
  for (size_t i = 0; i < regions.size(); ++i) {
    const size_t start = (i * W / 3) & ~size_t(1);
    const size_t end = ((i + 1) * W / 3) & ~size_t(1);
    const size_t x0 = start + 2 * (random() % ((end - start) / 2));
    const size_t x1 = x0 + 2 * (1 + random() % ((end - x0) / 2));
    const size_t y0 = 2 * (random() % (H / 2));
    const size_t y1 = y0 + 2 * (1 + random() % ((H - y0) / 2));
    regions[i] = {x0, x1, y0, y1};
  }
  if (narrow)
    regions = {Rect{0, 2, 0, H}, Rect{4, 6, 0, H}, Rect{8, 10, 0, H}};
  auto color = [](size_t x, size_t y) { return uint16_t(1 + ((y * W + x) % 65000)); };
  std::vector<uint16_t> panel(W * H), expected(W * H);
  std::vector<unsigned> writes(W * H);
  for (const auto &r : regions)
    for (size_t y = r.y0; y < r.y1; ++y)
      for (size_t x = r.x0; x < r.x1; ++x)
        expected[y * W + x] = color(x, y);
  std::array<std::vector<uint16_t>, 3> storage;
  std::array<std::span<uint16_t>, 3> buffers;
  for (size_t i = 0; i < buffers.size(); ++i) {
    storage[i].resize(W * Rows + 2, 0xa55a);
    buffers[i] = {storage[i].data() + 1, W * Rows};
  }

  struct Transfer {
    size_t x0, y0, x1, y1;
    std::span<uint16_t> source;
    std::vector<uint16_t> snapshot;
  };

  std::optional<Transfer> dma;
  size_t calls = 0, overlapped_lines = 0;
  auto submit = [&](size_t x0, size_t y0, size_t x1, size_t y1, std::span<uint16_t> data) {
    CHECK(!dma);
    CHECK(!(x0 % 2 || x1 % 2 || y0 % 2 || y1 % 2));
    CHECK(x0 < x1 && x1 <= W && y0 < y1 && y1 <= H);
    CHECK(data.size() == (x1 - x0) * (y1 - y0) && data.size() <= W * Rows);
    dma = Transfer{x0, y0, x1, y1, data, {data.begin(), data.end()}};
    ++calls;
  };
  auto wait = [&]() {
    CHECK(dma.has_value());
    CHECK(std::equal(dma->source.begin(), dma->source.end(), dma->snapshot.begin()));
    size_t offset = 0;
    for (size_t y = dma->y0; y < dma->y1; ++y)
      for (size_t x = dma->x0; x < dma->x1; ++x) {
        panel[y * W + x] = dma->source[offset++];
        ++writes[y * W + x];
      }
    dma.reset();
  };
  bike::ScanlineChunks<uint16_t, W, H, Rows> chunks(buffers);
  for (size_t y = 0; y < H; ++y)
    for (const auto &r : regions)
      if (y >= r.y0 && y < r.y1)
        chunks.line(
            y, r.x0, r.x1,
            [&](std::span<uint16_t> line) {
              if (dma) {
                const auto first = reinterpret_cast<uintptr_t>(line.data());
                const auto last = first + line.size_bytes();
                const auto source = reinterpret_cast<uintptr_t>(dma->source.data());
                CHECK(last <= source || first >= source + dma->source.size_bytes());
                ++overlapped_lines;
              }
              for (size_t x = r.x0; x < r.x1; ++x)
                line[x - r.x0] = color(x, y);
            },
            submit, wait);
  chunks.finish(submit, wait);
  CHECK(!dma && panel == expected);
  for (size_t i = 0; i < writes.size(); ++i)
    CHECK(writes[i] == (expected[i] ? 1u : 0u));
  for (const auto &buffer : storage)
    CHECK(buffer.front() == 0xa55a && buffer.back() == 0xa55a);
  overlapped_total += overlapped_lines;
  if (narrow) {
    CHECK(calls < 3 * ((H + Rows - 1) / Rows));
    // Small regions may fit in a single chunk per rectangle, flushing at end.
    if (H > W * Rows / 2)
      CHECK(overlapped_lines > 0);
  }
}

int main() {
  std::mt19937 random(12345);
  for (int i = 0; i < 2000; ++i)
    exercise<64, 54, 4>(random);
  for (int i = 0; i < 20; ++i)
    exercise<410, 502, 10>(random);
  exercise<64, 54, 4>(random, true);
  exercise<410, 502, 10>(random, true);
  CHECK(overlapped_total > 0);
  std::cout << "Scanline pixels, alignment, batching and DMA ownership passed\n";
}
