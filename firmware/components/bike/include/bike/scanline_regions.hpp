#pragma once
#include <cassert>
#include <cstddef>
#include <span>

namespace bike
{

struct ScanlineSpan {
  size_t y, x0, x1;
};

struct ScanlineRegion {
  size_t y, x0, x1, height;
};

// Slint can interleave several horizontal ranges on each row. Coalesce each
// range independently, without ever copying across a missing row or pixel.
inline size_t scanline_regions(std::span<const ScanlineSpan> spans, std::span<ScanlineRegion> regions) {
  assert(regions.size() >= spans.size());
  size_t count = 0;
  for (const auto &span : spans) {
    size_t i = 0;
    for (; i < count; ++i) {
      auto &region = regions[i];
      if (region.x0 == span.x0 && region.x1 == span.x1 && region.y + region.height == span.y) {
        ++region.height;
        break;
      }
    }
    if (i == count)
      regions[count++] = {span.y, span.x0, span.x1, 1};
  }
  return count;
}

} // namespace bike
