#pragma once
#include <array>
#include <cassert>
#include <cstddef>
#include <span>

namespace bike
{

// One accumulator per dirty rectangle. Only the display owns an in-flight
// buffer; other buffers can be rasterized while its transfer completes.
template <class Pixel, size_t Width, size_t Height, size_t Rows, size_t Count = 3> class ScanlineChunks {

  static_assert(Rows >= 2 && Rows % 2 == 0);

  struct Chunk {
    size_t x0 = 0, x1 = 0, y0 = 0, rows = 0;
  };

  std::array<Chunk, Count> chunks{};
  std::array<std::span<Pixel>, Count> buffers;
  int in_flight = -1;

  template <class Wait> void drain(Wait &wait) {
    if (in_flight >= 0) {
      wait();
      in_flight = -1;
    }
  }

  template <class Submit, class Wait> void flush(size_t i, Submit &submit, Wait &wait) {
    auto &c = chunks[i];
    if (!c.rows)
      return;
    assert(!(c.x0 % 2 || c.x1 % 2 || c.y0 % 2 || c.rows % 2));
    assert(c.x1 <= Width && c.y0 + c.rows <= Height);
    // Address-window commands must wait for the preceding color transfer.
    // Retire it explicitly with a bounded wait before entering the driver.
    drain(wait);
    submit(c.x0, c.y0, c.x1, c.y0 + c.rows, buffers[i].first((c.x1 - c.x0) * c.rows));
    in_flight = static_cast<int>(i);
    c.rows = 0;
  }

public:

  explicit ScanlineChunks(std::array<std::span<Pixel>, Count> storage) : buffers(storage) {
    for (auto buffer : buffers)
      assert(buffer.size() == Width * Rows);
  }

  template <class Render, class Submit, class Wait>
  void line(size_t y, size_t x0, size_t x1, Render render, Submit &submit, Wait &wait) {
    assert(x0 < x1 && x1 <= Width && y < Height && !(x0 % 2 || x1 % 2));
    for (size_t i = 0; i < Count; ++i)
      if (chunks[i].rows && chunks[i].y0 + chunks[i].rows < y)
        flush(i, submit, wait);
    int slot = -1;
    for (size_t i = 0; i < Count; ++i) {
      const auto &c = chunks[i];
      if (c.rows && c.x0 == x0 && c.x1 == x1 && c.y0 + c.rows == y) {
        slot = static_cast<int>(i);
        break;
      }
    }
    if (slot < 0) {
      // Prefer a free buffer that DMA is not reading.
      for (size_t i = 0; i < Count; ++i)
        if (!chunks[i].rows && static_cast<int>(i) != in_flight) {
          slot = static_cast<int>(i);
          break;
        }
      if (slot < 0)
        for (size_t i = 0; i < Count; ++i)
          if (!(chunks[i].rows % 2)) {
            flush(i, submit, wait);
            slot = static_cast<int>(i);
            break;
          }
      assert(slot >= 0);
      chunks[slot] = {x0, x1, y, 0};
    }
    if (slot == in_flight)
      drain(wait);
    auto &c = chunks[slot];
    const size_t width = x1 - x0;
    const size_t capacity = (buffers[slot].size() / width) & ~size_t(1);
    assert(c.rows < capacity);
    render(buffers[slot].subspan(c.rows * width, width));
    if (++c.rows == capacity)
      flush(slot, submit, wait);
  }

  template <class Submit, class Wait> void finish(Submit &submit, Wait &wait) {
    for (size_t i = 0; i < Count; ++i)
      flush(i, submit, wait);
    drain(wait); // Orientation, brightness and the next frame may now proceed.
  }
};

} // namespace bike
