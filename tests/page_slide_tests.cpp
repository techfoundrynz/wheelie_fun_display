#include "bike/page_slide.hpp"
#include <cassert>
#include <vector>

int main() {
  constexpr int width = 12, height = 18;
  // Simulate alternating buffers with arbitrary disjoint or overlapping dirty
  // rectangles. Restoring prior damage must never overwrite this frame's work.
  unsigned random_state = 749;
  auto random = [&](int limit) {
    random_state = random_state * 1664525u + 1013904223u;
    return int((random_state >> 8) % unsigned(limit));
  };
  std::vector<int> buffers[2]{std::vector<int>(width * height), std::vector<int>(width * height)};
  std::vector<int> expected(width * height);
  bike::SlideRect previous;
  for (int frame = 1; frame <= 2000; ++frame) {
    std::array<bike::SlideRect, 3> dirty;
    auto &back = buffers[frame % 2];
    const auto &front = buffers[(frame + 1) % 2];
    bike::SlideRect bounds;
    for (auto &r : dirty) {
      r.x = random(width);
      r.y = random(height);
      r.width = 1 + random(width - r.x);
      r.height = 1 + random(height - r.y);
      for (int y = r.y; y < r.y + r.height; ++y)
        for (int x = r.x; x < r.x + r.width; ++x)
          back[y * width + x] = expected[y * width + x] = frame;
      if (!bounds.width) {
        bounds = r;
      }
      else {
        const int right = std::max(bounds.x + bounds.width, r.x + r.width);
        const int bottom = std::max(bounds.y + bounds.height, r.y + r.height);
        bounds.x = std::min(bounds.x, r.x);
        bounds.y = std::min(bounds.y, r.y);
        bounds.width = right - bounds.x;
        bounds.height = bottom - bounds.y;
      }
    }
    std::array<bike::SlideRect, 128> missing;
    const auto count = bike::unpainted_slide_regions(previous, dirty, missing);
    assert(count);
    for (size_t i = 0; i < *count; ++i) {
      const auto &r = missing[i];
      for (int y = r.y; y < r.y + r.height; ++y)
        for (int x = r.x; x < r.x + r.width; ++x)
          back[y * width + x] = front[y * width + x];
    }
    assert(back == expected);
    previous = bounds;
  }
  const bike::SlideRect viewport{3, 4, 5, 8};
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      for (int w = 1; w <= width - x; ++w) {
        for (int h = 1; h <= height - y; ++h) {
          std::vector<int> coverage(width * height, 0), target(width * height, -1);
          const auto pieces = bike::split_slide_damage({x, y, w, h}, viewport);
          for (size_t i = 0; i < pieces.size(); ++i) {
            const auto &r = pieces[i];
            assert(r.width >= 0 && r.height >= 0);
            for (int yy = r.y; yy < r.y + r.height; ++yy) {
              for (int xx = r.x; xx < r.x + r.width; ++xx) {
                assert(xx >= x && xx < x + w && yy >= y && yy < y + h);
                ++coverage[yy * width + xx];
                target[yy * width + xx] = i == 0;
              }
            }
          }
          for (int yy = 0; yy < height; ++yy) {
            for (int xx = 0; xx < width; ++xx) {
              const bool dirty = xx >= x && xx < x + w && yy >= y && yy < y + h;
              const bool page = xx >= viewport.x && xx < viewport.x + viewport.width && yy >= viewport.y &&
                                yy < viewport.y + viewport.height;
              assert(coverage[yy * width + xx] == int(dirty));
              if (dirty)
                assert(target[yy * width + xx] == int(page));
            }
          }
        }
      }
    }
  }
  for (auto orientation : {bike::Orientation::Portrait, bike::Orientation::Landscape,
                           bike::Orientation::PortraitInverted, bike::Orientation::LandscapeInverted}) {
    const int logical_width = bike::display_width(orientation, width, height);
    const int logical_height = bike::display_height(orientation, width, height);
    const bike::SlideRect viewport{1, 2, logical_width - 3, logical_height - 4};
    std::vector<int> source[2]{std::vector<int>(width * height), std::vector<int>(width * height)};
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const auto logical = bike::to_logical({x, y}, orientation, width, height);
        for (int page = 0; page < 2; ++page)
          source[page][y * width + x] = 10000 * page + 100 * logical.y + logical.x;
      }
    }
    for (int direction : {-1, 1}) {
      for (int travel = -1; travel <= viewport.width + 1; ++travel) {
        std::vector<int> actual(width * height, -1), coverage(width * height, 0);
        const auto blits = bike::page_slide_blits(viewport, direction, travel);
        std::array<bike::SlideBlit, 2> native;
        for (int page = 0; page < 2; ++page) {
          const auto src = bike::native_slide_rect(blits[page].source, orientation, width, height);
          const auto dst = bike::native_slide_rect(blits[page].destination, orientation, width, height);
          native[page] = {src, dst};
          assert(src.width == dst.width && src.height == dst.height);
          assert(src.x >= 0 && src.y >= 0 && src.x + src.width <= width && src.y + src.height <= height);
          assert(dst.x >= 0 && dst.y >= 0 && dst.x + dst.width <= width && dst.y + dst.height <= height);
          for (int y = 0; y < dst.height; ++y) {
            for (int x = 0; x < dst.width; ++x) {
              const int offset = (dst.y + y) * width + dst.x + x;
              actual[offset] = source[page][(src.y + y) * width + src.x + x];
              ++coverage[offset];
            }
          }
        }
        std::vector<int> streamed(width * height, -1);
        const auto native_viewport = bike::native_slide_rect(viewport, orientation, width, height);
        const std::array<std::span<const int>, 2> images{source[0], source[1]};
        for (int y = native_viewport.y; y < native_viewport.y + native_viewport.height; ++y) {
          const auto row = std::span<int>(streamed).subspan(y * width + native_viewport.x, native_viewport.width);
          assert(bike::compose_slide_line(row, native_viewport.x, y, width, images, native));
        }
        assert(streamed == actual);
        const int distance = std::clamp(travel, 0, viewport.width);
        for (int y = 0; y < height; ++y) {
          for (int x = 0; x < width; ++x) {
            const auto logical = bike::to_logical({x, y}, orientation, width, height);
            const bool inside = logical.x >= viewport.x && logical.x < viewport.x + viewport.width &&
                                logical.y >= viewport.y && logical.y < viewport.y + viewport.height;
            const int offset = y * width + x;
            assert(coverage[offset] == (inside ? 1 : 0));
            if (!inside) {
              assert(actual[offset] == -1);
              continue;
            }
            const int old_x = logical.x + direction * distance;
            const bool old_page = old_x >= viewport.x && old_x < viewport.x + viewport.width;
            const int source_x = old_page ? old_x : logical.x - direction * (viewport.width - distance);
            assert(actual[offset] == (old_page ? 0 : 10000) + 100 * logical.y + source_x);
          }
        }
      }
    }
  }
}
