#include "bike/page_slide.hpp"
#include "bike/scanline_chunks.hpp"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "render_profile.hpp"
#include "slint_rendering.hpp"
#include <algorithm>
#include <cstring>

namespace slint_rendering
{
namespace
{

using Pixel = slint::platform::Rgb565BigEndianPixel;
constexpr size_t pixel_count = size_t(board::panel_width) * board::panel_height;
Pixel *frame;
Pixel *snapshots[2];
Pixel *tiles[3];
bool cache_valid = false;
bool refresh_on_contact = false;
uint64_t last_scene_update_us = 0;
int cache_page = -1;
bike::Orientation cache_orientation;
std::function<PageSlideState()> read_slide;
std::function<void(int)> capture_page;
int captured_page = -1;
bool composing = false, primed = false;
PageSlideState cached;
bike::Orientation cached_orientation, primed_orientation;

void capture(int page) {
  if (page != captured_page) {
    capture_page(page);
    captured_page = page;
  }
}

void clear(Pixel *target, bike::SlideRect rect) {
  for (int row = 0; row < rect.height; ++row)
    std::memset(static_cast<void *>(target + (rect.y + row) * board::panel_width + rect.x), 0,
                rect.width * sizeof(Pixel));
}

bike::SlideRect unite(bike::SlideRect a, bike::SlideRect b) {
  if (!a.width || !a.height)
    return b;
  if (!b.width || !b.height)
    return a;
  const int x = std::min(a.x, b.x), y = std::min(a.y, b.y);
  return {x, y, std::max(a.x + a.width, b.x + b.width) - x, std::max(a.y + a.height, b.y + b.height) - y};
}

bike::SlideRect rasterize(slint::platform::SoftwareRenderer &renderer, Pixel *page = nullptr,
                          bike::SlideRect viewport = {}) {
#if WFD_RENDER_PROFILE
  const auto started = esp_timer_get_time();
  bool first_line = true;
#endif
  const auto dirty = renderer.render_by_line<Pixel>([&](size_t y, size_t x0, size_t x1, auto draw) {
#if WFD_RENDER_PROFILE
    if (first_line) {
      render_profile::last_scene_us += esp_timer_get_time() - started;
      first_line = false;
    }
#endif
    auto *line = (page ? page : frame) + y * board::panel_width;
    draw(std::span<Pixel>(line + x0, x1 - x0));
    if (page) {
      auto chrome = [&](size_t left, size_t right) {
        if (right > left)
          std::memcpy(frame + y * board::panel_width + left, line + left, (right - left) * sizeof(Pixel));
      };
      if (int(y) < viewport.y || int(y) >= viewport.y + viewport.height)
        chrome(x0, x1);
      else {
        chrome(x0, std::min<size_t>(x1, viewport.x));
        chrome(std::max<size_t>(x0, viewport.x + viewport.width), x1);
      }
    }
  });
#if WFD_RENDER_PROFILE
  if (first_line)
    render_profile::last_scene_us += esp_timer_get_time() - started;
#endif
  const auto s = dirty.bounding_box_size();
  if (!s.width || !s.height)
    return {};
  const auto p = dirty.bounding_box_origin();
  return {int(p.x), int(p.y), int(s.width), int(s.height)};
}

template <class Draw> Work transfer(board::Display &display, Draw draw) {
  Work work;
  using Chunks = bike::ScanlineChunks<Pixel, board::panel_width, board::panel_height, board::tile_rows>;
  std::array<std::span<Pixel>, 3> buffers;
  for (size_t i = 0; i < buffers.size(); ++i)
    buffers[i] = {tiles[i], board::panel_width * board::tile_rows};
  Chunks chunks(buffers);
  auto wait = [&] {
    const auto before = esp_timer_get_time();
    ESP_ERROR_CHECK(xSemaphoreTake(display.dma_done, pdMS_TO_TICKS(1000)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT);
    work.wait_us += esp_timer_get_time() - before;
  };
  auto submit = [&](size_t x0, size_t y0, size_t x1, size_t y1, std::span<Pixel> data) {
    ESP_ERROR_CHECK(board::draw_region(display, x0, y0, x1, y1, data.data()));
    ++work.calls;
    work.pixels += data.size();
#if WFD_RENDER_PROFILE
    ++render_profile::last_copy_calls;
#endif
  };
  draw([&](size_t y, size_t x0, size_t x1, auto paint) { chunks.line(y, x0, x1, paint, submit, wait); });
  chunks.finish(submit, wait);
  return work;
}

Work present(board::Display &display, bike::SlideRect damage) {
  if (!damage.width || !damage.height)
    return {};
  return transfer(display, [&](auto line) {
    for (int y = damage.y; y < damage.y + damage.height; ++y)
      line(y, damage.x, damage.x + damage.width, [&](std::span<Pixel> target) {
        std::memcpy(target.data(), frame + y * board::panel_width + damage.x, target.size_bytes());
      });
  });
}

Work compose(board::Display &display, bike::SlideRect logical, bike::Orientation orientation, int direction,
             float progress, bike::SlideRect damage) {
  Work work;
  auto add = [&](Work next) {
    work.calls += next.calls;
    work.pixels += next.pixels;
    work.wait_us += next.wait_us;
  };
  const auto viewport = bike::native_slide_rect(logical, orientation);
  const auto outside = bike::split_slide_damage(damage, viewport);
  for (size_t i = 1; i < outside.size(); ++i)
    add(present(display, outside[i]));
  const int travel = std::clamp(int(float(logical.width) * progress + .5f), 0, logical.width);
  const auto blits = bike::page_slide_blits(logical, direction, travel);
  std::array<bike::SlideBlit, 2> native;
  for (size_t i = 0; i < blits.size(); ++i) {
    native[i] = {bike::native_slide_rect(blits[i].source, orientation),
                 bike::native_slide_rect(blits[i].destination, orientation)};
  }
  const std::array<std::span<const Pixel>, 2> images{std::span<const Pixel>(snapshots[0], pixel_count),
                                                     std::span<const Pixel>(snapshots[1], pixel_count)};
  add(transfer(display, [&](auto line) {
    for (int y = viewport.y; y < viewport.y + viewport.height; ++y)
      line(y, viewport.x, viewport.x + viewport.width, [&](std::span<Pixel> target) {
        // Keep odd-pixel seams inside aligned DMA transfers.
        ESP_ERROR_CHECK(bike::compose_slide_line(target, viewport.x, y, board::panel_width, images, native)
                            ? ESP_OK
                            : ESP_ERR_INVALID_ARG);
      });
  }));
  return work;
}

Work normal(slint::platform::SoftwareRenderer &renderer, board::Display &display, const PageSlideState &state,
            bike::Orientation orientation) {
  const auto vp = bike::native_slide_rect(state.viewport, orientation);
  if (!state.available || cache_page != state.incoming || cache_orientation != orientation)
    cache_valid = false;
#if WFD_RENDER_PROFILE
  const auto started = esp_timer_get_time();
  bool first_line = true;
#endif
  const bool contact = refresh_on_contact;
  size_t body_pixels = 0;
  const auto work = transfer(display, [&](auto line) {
    renderer.render_by_line<Pixel>([&](size_t y, size_t x0, size_t x1, auto draw) {
#if WFD_RENDER_PROFILE
      if (first_line) {
        render_profile::last_scene_us += esp_timer_get_time() - started;
        first_line = false;
      }
#endif
      line(y, x0, x1, [&](std::span<Pixel> target) {
        draw(target);
        // Mirror fixed chrome; recapture scrolling content before the next slide.
        auto mirror = [&](size_t left, size_t right) {
          if (right > left)
            std::memcpy(frame + y * board::panel_width + left, target.data() + left - x0,
                        (right - left) * sizeof(Pixel));
        };
        if (!state.available || int(y) < vp.y || int(y) >= vp.y + vp.height) {
          mirror(x0, x1);
        }
        else {
          mirror(x0, std::min<size_t>(x1, vp.x));
          mirror(std::max<size_t>(x0, vp.x + vp.width), x1);
          const int left = std::max<int>(x0, vp.x), right = std::min<int>(x1, vp.x + vp.width);
          if (right > left && cache_valid) {
            body_pixels += right - left;
            if (!contact && body_pixels > size_t(vp.width) * vp.height / 2)
              cache_valid = false;
            if (cache_valid)
              std::memcpy(snapshots[1] + y * board::panel_width + left, target.data() + left - x0,
                          (right - left) * sizeof(Pixel));
          }
        }
      });
    });
  });
  refresh_on_contact = false;
  return work;
}

} // namespace

void initialize() {
  frame = static_cast<Pixel *>(
      heap_caps_aligned_alloc(64, 3 * pixel_count * sizeof(Pixel), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  configASSERT(frame);
  snapshots[0] = frame + pixel_count;
  snapshots[1] = frame + 2 * pixel_count;
  std::memset(static_cast<void *>(frame), 0, 3 * pixel_count * sizeof(Pixel));
  for (auto &tile : tiles) {
    tile = static_cast<Pixel *>(
        heap_caps_malloc(board::panel_width * board::tile_rows * sizeof(Pixel), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    configASSERT(tile);
  }
}

void configure_page_slides(std::function<PageSlideState()> read, std::function<void(int)> capture_fn) {
  read_slide = std::move(read);
  capture_page = std::move(capture_fn);
}

bool page_slide_active() {
  return composing || (read_slide && read_slide().active);
}

void prepare_pointer_input() {
  if (!composing && captured_page >= 0) {
    refresh_on_contact = true;
    capture(-1);
  }
}

slint::platform::SoftwareRenderer::RenderingRotation rotation(bike::Orientation orientation) {
  using Rotation = slint::platform::SoftwareRenderer::RenderingRotation;
  switch (orientation) {
  case bike::Orientation::Landscape:
    return Rotation::Rotate90;
  case bike::Orientation::PortraitInverted:
    return Rotation::Rotate180;
  case bike::Orientation::LandscapeInverted:
    return Rotation::Rotate270;
  default:
    return Rotation::NoRotation;
  }
}

Work render(slint::platform::SoftwareRenderer &renderer, board::Display &display, bike::Orientation orientation) {
#if WFD_RENDER_PROFILE
  const auto started = esp_timer_get_time();
  render_profile::last_scene_us = 0;
  render_profile::current_phase = 0;
  render_profile::last_copy_calls = 0;
#endif
  auto state = read_slide();
  const auto v = state.viewport;
  const bool valid = v.x >= 0 && v.y >= 0 && v.width > 0 && v.height > 0 &&
                     v.x + v.width <= bike::display_width(orientation) &&
                     v.y + v.height <= bike::display_height(orientation);
  if (!state.content_active || (primed && primed_orientation != orientation))
    primed = false;
  bike::SlideRect damage;
  bool preloaded = false;
  if (!primed && state.available && !state.active && valid) {
    composing = false;
    capture(-2);
    rasterize(renderer);
    for (int page = 0; page < 7; ++page) {
      capture(page);
      rasterize(renderer);
      vTaskDelay(1);
    }
    capture(-2);
    rasterize(renderer);
    const auto vp = bike::native_slide_rect(v, orientation);
    capture(state.incoming);
    clear(snapshots[1], vp);
    rasterize(renderer, snapshots[1], vp);
    cache_page = state.incoming;
    cache_orientation = orientation;
    cache_valid = true;
    preloaded = true;
    primed = true;
    primed_orientation = orientation;
    damage = {0, 0, board::panel_width, board::panel_height};
  }
  if (state.active && state.available && valid && state.incoming >= 0 && state.outgoing >= 0) {
    const auto viewport = bike::native_slide_rect(v, orientation);
#if WFD_RENDER_PROFILE
    render_profile::current_phase = 2;
#endif
    if (!composing || cached.incoming != state.incoming || cached.outgoing != state.outgoing ||
        cached_orientation != orientation) {
#if WFD_RENDER_PROFILE
      render_profile::current_phase = 1;
#endif
      if (composing && cached.outgoing == state.outgoing && cached_orientation == orientation) {
        // Reversing across the origin keeps its original snapshot.
      }
      else if (composing && cached.incoming == state.outgoing && cached_orientation == orientation) {
        std::swap(snapshots[0], snapshots[1]);
      }
      else {
        if (!cache_valid || cache_page != state.outgoing || cache_orientation != orientation) {
          capture(-2);
          damage = unite(damage, rasterize(renderer));
          capture(state.outgoing);
          clear(snapshots[0], viewport);
          damage = unite(damage, rasterize(renderer, snapshots[0], viewport));
        }
        else {
          std::swap(snapshots[0], snapshots[1]);
        }
      }
      capture(state.incoming);
      clear(snapshots[1], viewport);
      damage = unite(damage, rasterize(renderer, snapshots[1], viewport));
      cached = state;
      cached_orientation = orientation;
      composing = true;
      last_scene_update_us = esp_timer_get_time();
      slint::platform::update_timers_and_animations();
      const auto fresh = read_slide();
      if (fresh.active && fresh.incoming == state.incoming && fresh.outgoing == state.outgoing)
        state.progress = fresh.progress;
      else
        state.progress = !fresh.active && fresh.incoming == state.outgoing ? 0.f : 1.f;
    }
    else if (esp_timer_get_time() - last_scene_update_us >= 30000) {
      // Refresh content every 30 ms; translate every frame.
      // Lock/modal changes bypass this limit through state.available.
      damage = unite(damage, rasterize(renderer, snapshots[1], viewport));
      last_scene_update_us = esp_timer_get_time();
    }
    const auto work = compose(display, v, orientation, state.direction, state.progress, damage);
#if WFD_RENDER_PROFILE
    render_profile::record(esp_timer_get_time() - started - work.wait_us, work.pixels, work.wait_us);
#endif
    return work;
  }
  else if (composing) {
#if WFD_RENDER_PROFILE
    render_profile::current_phase = 3;
#endif
    const auto viewport = bike::native_slide_rect(cached.viewport, cached_orientation);
    if (state.incoming == cached.outgoing)
      std::swap(snapshots[0], snapshots[1]);
    composing = false;
    if (state.available) {
      capture(state.incoming);
      damage = unite(damage, rasterize(renderer, snapshots[1], viewport));
    }
    else {
      capture(-2);
      damage = unite(damage, rasterize(renderer));
      capture(-1);
      damage = unite(damage, rasterize(renderer));
    }
    cache_page = state.incoming;
    cache_orientation = orientation;
    cache_valid = state.available;
    if (state.available) {
      const auto work = compose(display, cached.viewport, cached_orientation, 1, 1.f, damage);
#if WFD_RENDER_PROFILE
      render_profile::record(esp_timer_get_time() - started - work.wait_us, work.pixels, work.wait_us);
#endif
      return work;
    }
  }
  else if (!preloaded) {
    if (captured_page >= 0 && (!state.available || captured_page != state.incoming))
      capture(-1);
    const auto work = normal(renderer, display, state, orientation);
#if WFD_RENDER_PROFILE
    if (work.calls)
      render_profile::record(esp_timer_get_time() - started - work.wait_us, work.pixels, work.wait_us);
#endif
    return work;
  }
  if (!damage.width || !damage.height)
    return {};
  const auto work = preloaded ? compose(display, v, orientation, 1, 1.f, damage) : present(display, damage);
#if WFD_RENDER_PROFILE
  render_profile::record(esp_timer_get_time() - started - work.wait_us, work.pixels, work.wait_us);
#endif
  return work;
}

} // namespace slint_rendering
