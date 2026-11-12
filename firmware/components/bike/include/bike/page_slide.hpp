#pragma once
#include "orientation.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <optional>
#include <span>

namespace bike
{

struct SlideRect {
  int x = 0, y = 0, width = 0, height = 0;
};

struct SlideBlit {
  SlideRect source, destination;
};

// Fill one aligned output row from translated page images. The seam itself
// may fall on an odd pixel; no unaligned panel address window is needed.
template <class Pixel>
bool compose_slide_line(std::span<Pixel> target, int x, int y, int stride,
                        const std::array<std::span<const Pixel>, 2> &images, const std::array<SlideBlit, 2> &blits) {
  size_t painted = 0;
  for (size_t i = 0; i < blits.size(); ++i) {
    const auto &src = blits[i].source;
    const auto &dst = blits[i].destination;
    if (dst.width <= 0 || dst.height <= 0 || y < dst.y || y >= dst.y + dst.height)
      continue;
    const int left = std::max(x, dst.x), right = std::min(x + int(target.size()), dst.x + dst.width);
    if (right <= left)
      continue;
    const int source_x = src.x + left - dst.x, source_y = src.y + y - dst.y;
    if (stride <= 0 || source_x < 0 || source_y < 0 || source_x + right - left > stride)
      return false;
    const size_t offset = size_t(source_y) * stride + source_x, count = right - left;
    if (offset + count > images[i].size())
      return false;
    std::memcpy(target.data() + left - x, images[i].data() + offset, count * sizeof(Pixel));
    painted += count;
  }
  return painted == target.size();
}

// The intersection followed by up to four disjoint pieces outside the viewport.
constexpr std::array<SlideRect, 5> split_slide_damage(SlideRect r, SlideRect viewport) {
  const int l = std::max(r.x, viewport.x), t = std::max(r.y, viewport.y);
  const int right = std::min(r.x + r.width, viewport.x + viewport.width);
  const int bottom = std::min(r.y + r.height, viewport.y + viewport.height);
  if (right <= l || bottom <= t)
    return {{{}, r, {}, {}, {}}};
  return {{{l, t, right - l, bottom - t},
           {r.x, r.y, r.width, t - r.y},
           {r.x, bottom, r.width, r.y + r.height - bottom},
           {r.x, t, l - r.x, bottom - t},
           {right, t, r.x + r.width - right, bottom - t}}};
}

// Restore previous damage without overwriting this frame's newly painted pixels.
template <size_t Capacity>
std::optional<size_t> unpainted_slide_regions(SlideRect previous, std::span<const SlideRect> painted,
                                              std::array<SlideRect, Capacity> &output) {
  static_assert(Capacity > 0);
  if (previous.width <= 0 || previous.height <= 0)
    return 0;
  output[0] = previous;
  size_t count = 1;
  std::array<SlideRect, Capacity> next;
  for (auto dirty : painted) {
    size_t next_count = 0;
    for (size_t i = 0; i < count; ++i) {
      const auto pieces = split_slide_damage(output[i], dirty);
      for (size_t j = 1; j < pieces.size(); ++j) {
        if (pieces[j].width <= 0 || pieces[j].height <= 0)
          continue;
        if (next_count == Capacity)
          return std::nullopt;
        next[next_count++] = pieces[j];
      }
    }
    std::copy_n(next.begin(), next_count, output.begin());
    count = next_count;
  }
  return count;
}

constexpr SlideRect native_slide_rect(SlideRect r, Orientation orientation, int width = panel_width,
                                      int height = panel_height) {
  switch (orientation) {
  case Orientation::Landscape:
    return {width - r.y - r.height, r.x, r.height, r.width};
  case Orientation::PortraitInverted:
    return {width - r.x - r.width, height - r.y - r.height, r.width, r.height};
  case Orientation::LandscapeInverted:
    return {r.y, height - r.x - r.width, r.height, r.width};
  default:
    return r;
  }
}

// Two integer translations partition the viewport exactly, including the seam.
constexpr std::array<SlideBlit, 2> page_slide_blits(SlideRect viewport, int direction, int travel) {
  travel = std::clamp(travel, 0, viewport.width);
  direction = direction < 0 ? -1 : 1;
  std::array<SlideBlit, 2> result{};
  const int offsets[]{-direction * travel, direction * (viewport.width - travel)};
  for (unsigned i = 0; i < 2; ++i) {
    const int left = std::max(viewport.x, viewport.x + offsets[i]);
    const int right = std::min(viewport.x + viewport.width, viewport.x + viewport.width + offsets[i]);
    result[i] = {{left - offsets[i], viewport.y, right - left, viewport.height},
                 {left, viewport.y, right - left, viewport.height}};
  }
  return result;
}

} // namespace bike
