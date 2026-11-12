#include "bike/scanline_regions.hpp"
#include "driver/ppa.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "render_profile.hpp"
#include "slint_rendering.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <span>

namespace slint_rendering
{
namespace
{

using Pixel = slint::platform::Rgb565Pixel;
constexpr size_t tile_rows = 32;

struct Tile {
  Pixel *pixels = nullptr;
  SemaphoreHandle_t done = nullptr;
  std::atomic<uint32_t> pending{0};
};

Tile tiles[2];
ppa_client_handle_t copier;
SemaphoreHandle_t copy_credits;
std::function<PageSlideState()> read_slide;
std::function<void(int)> capture_page;
int captured_page = -1;
uint16_t *snapshots[2]{};
const uint16_t *pending_old = nullptr;
ppa_client_handle_t page_copier;
ppa_client_handle_t page_filler;
PageSlideState cached_slide;
bike::Orientation cached_orientation;
bool composing = false;
bike::SlideRect previous_damage;
std::array<bike::SlideRect, 32> painted;
size_t painted_count = 0;
bool pages_primed = false;
bike::Orientation primed_orientation;

void select_capture(int page) {
  if (captured_page == page)
    return;
  capture_page(page);
  captured_page = page;
}

uint32_t clear_page(uint16_t *destination, bike::SlideRect viewport) {
  const auto started = esp_timer_get_time();
  ppa_fill_oper_config_t fill{};
  fill.out.buffer = destination;
  fill.out.buffer_size = board::panel_width * board::panel_height * sizeof(Pixel);
  fill.out.pic_w = board::panel_width;
  fill.out.pic_h = board::panel_height;
  fill.out.block_offset_x = viewport.x;
  fill.out.block_offset_y = viewport.y;
  fill.out.fill_cm = PPA_FILL_COLOR_MODE_RGB565;
  fill.fill_block_w = viewport.width;
  fill.fill_block_h = viewport.height;
  fill.fill_color_val = 0;
  fill.mode = PPA_TRANS_MODE_BLOCKING;
  ESP_ERROR_CHECK(ppa_do_fill(page_filler, &fill));
  return esp_timer_get_time() - started;
}

uint32_t copy_page(const uint16_t *source, uint16_t *destination, bike::SlideRect src, bike::SlideRect dst) {
  if (src.width <= 0 || src.height <= 0)
    return 0;
  const auto started = esp_timer_get_time();
  ppa_srm_oper_config_t copy{};
  copy.in.buffer = source;
  copy.in.pic_w = board::panel_width;
  copy.in.pic_h = board::panel_height;
  copy.in.block_w = src.width;
  copy.in.block_h = src.height;
  copy.in.block_offset_x = src.x;
  copy.in.block_offset_y = src.y;
  copy.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  copy.out.buffer = destination;
  copy.out.buffer_size = board::panel_width * board::panel_height * sizeof(Pixel);
  copy.out.pic_w = board::panel_width;
  copy.out.pic_h = board::panel_height;
  copy.out.block_offset_x = dst.x;
  copy.out.block_offset_y = dst.y;
  copy.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  copy.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
  copy.scale_x = copy.scale_y = 1;
  copy.mode = PPA_TRANS_MODE_BLOCKING;
  ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(page_copier, &copy));
#if WFD_RENDER_PROFILE
  ++render_profile::last_copy_calls;
#endif
  return esp_timer_get_time() - started;
}

struct RasterWork {
  uint16_t *output = nullptr;
  board::FrameRect region;
  uint32_t wait_us = 0, pixels = 0;
};

bool IRAM_ATTR copy_done(ppa_client_handle_t, ppa_event_data_t *, void *context) {
  auto &tile = *static_cast<Tile *>(context);
  BaseType_t wake = pdFALSE;
  if (tile.pending.fetch_sub(1, std::memory_order_acq_rel) == 1)
    xSemaphoreGiveFromISR(tile.done, &wake);
  xSemaphoreGiveFromISR(copy_credits, &wake);
  return wake == pdTRUE;
}

} // namespace

void configure_page_slides(std::function<PageSlideState()> read, std::function<void(int)> capture) {
  const size_t pixels = size_t(board::panel_width) * board::panel_height;
  auto *storage = static_cast<uint16_t *>(
      heap_caps_aligned_alloc(128, 2 * pixels * sizeof(Pixel), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  configASSERT(storage);
  snapshots[0] = storage;
  snapshots[1] = storage + pixels;
  ppa_client_config_t config{};
  config.oper_type = PPA_OPERATION_FILL;
  config.max_pending_trans_num = 1;
  ESP_ERROR_CHECK(ppa_register_client(&config, &page_filler));
  read_slide = std::move(read);
  capture_page = std::move(capture);
}

bool page_slide_active() {
  return composing || previous_damage.width > 0 || (read_slide && read_slide().active);
}

void prepare_pointer_input() {
  if (!composing && captured_page >= 0)
    select_capture(-1);
}

void initialize() {
  auto *storage = static_cast<Pixel *>(heap_caps_aligned_alloc(64, 2 * tile_rows * board::panel_width * sizeof(Pixel),
                                                               MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  configASSERT(storage);
  for (size_t i = 0; i < 2; ++i) {
    auto &tile = tiles[i];
    tile.pixels = storage + i * tile_rows * board::panel_width;
    tile.done = xSemaphoreCreateBinary();
    configASSERT(tile.pixels && tile.done);
  }
  copy_credits = xSemaphoreCreateCounting(4, 4);
  configASSERT(copy_credits);
  ppa_client_config_t config{};
  config.oper_type = PPA_OPERATION_SRM;
  config.max_pending_trans_num = 4;
  ESP_ERROR_CHECK(ppa_register_client(&config, &copier));
  ppa_event_callbacks_t callbacks{};
  callbacks.on_trans_done = copy_done;
  ESP_ERROR_CHECK(ppa_client_register_event_callbacks(copier, &callbacks));
  config.max_pending_trans_num = 1;
  ESP_ERROR_CHECK(ppa_register_client(&config, &page_copier));
}

slint::platform::SoftwareRenderer::RenderingRotation rotation(bike::Orientation orientation) {
  ESP_ERROR_CHECK(board::wait_frame());
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

static RasterWork rasterize(slint::platform::SoftwareRenderer &renderer, uint16_t *target_output = nullptr,
                            uint16_t *page_output = nullptr, bike::SlideRect viewport = {}) {
#if WFD_RENDER_PROFILE
  const auto started = esp_timer_get_time();
  bool first_line = true;
#endif
  uint16_t *output = target_output;
  uint32_t wait_us = 0, pixels = 0;
  unsigned active_tile = 0;
  auto wait_tile = [&](Tile &tile) {
    if (!tile.pending.load(std::memory_order_acquire))
      return;
    const auto before = esp_timer_get_time();
    while (tile.pending.load(std::memory_order_acquire))
      ESP_ERROR_CHECK(xSemaphoreTake(tile.done, pdMS_TO_TICKS(500)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT);
    wait_us += esp_timer_get_time() - before;
  };

  const size_t rows = tile_rows;
  std::array<bike::ScanlineSpan, tile_rows * 4> spans;
  size_t span_count = 0, first_y = 0;
  auto flush = [&] {
    if (!span_count)
      return;
    if (!output) {
      const auto before = esp_timer_get_time();
      ESP_ERROR_CHECK(board::acquire_frame(&output));
      wait_us += esp_timer_get_time() - before;
    }
#if WFD_RENDER_PROFILE
    const auto copy_started = esp_timer_get_time();
#endif
    std::array<bike::ScanlineRegion, tile_rows * 4> regions;
    const auto groups = bike::scanline_regions(std::span<const bike::ScanlineSpan>(spans.data(), span_count), regions);
    auto &tile = tiles[active_tile];
    xSemaphoreTake(tile.done, 0);
    // A submission sentinel prevents an early callback from completing a tile
    // while more split regions are still being queued.
    tile.pending.store(1, std::memory_order_release);
    auto submit = [&](bike::SlideRect s, uint16_t *destination) {
      if (s.width <= 0 || s.height <= 0)
        return;
      const auto before = esp_timer_get_time();
      ESP_ERROR_CHECK(xSemaphoreTake(copy_credits, pdMS_TO_TICKS(500)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT);
      wait_us += esp_timer_get_time() - before;
      ppa_srm_oper_config_t copy{};
      copy.in.buffer = tile.pixels;
      copy.in.pic_w = board::panel_width;
      copy.in.pic_h = tile_rows;
      copy.in.block_w = s.width;
      copy.in.block_h = s.height;
      copy.in.block_offset_x = s.x;
      copy.in.block_offset_y = s.y - first_y;
      copy.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
      copy.out.buffer = destination;
      copy.out.buffer_size = board::panel_width * board::panel_height * sizeof(Pixel);
      copy.out.pic_w = board::panel_width;
      copy.out.pic_h = board::panel_height;
      copy.out.block_offset_x = s.x;
      copy.out.block_offset_y = s.y;
      copy.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
      copy.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
      copy.scale_x = copy.scale_y = 1;
      copy.mode = PPA_TRANS_MODE_NON_BLOCKING;
      copy.user_data = &tile;
      tile.pending.fetch_add(1, std::memory_order_release);
      ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(copier, &copy));
#if WFD_RENDER_PROFILE
      ++render_profile::last_copy_calls;
#endif
    };
    for (size_t i = 0; i < groups; ++i) {
      const auto &s = regions[i];
      const bike::SlideRect damage{int(s.x0), int(s.y), int(s.x1 - s.x0), int(s.height)};
      if (page_output) {
        const auto pieces = bike::split_slide_damage(damage, viewport);
        for (size_t j = 0; j < pieces.size(); ++j)
          submit(pieces[j], j == 0 ? page_output : output);
      }
      else {
        submit(damage, output);
      }
    }
    if (tile.pending.fetch_sub(1, std::memory_order_acq_rel) == 1)
      xSemaphoreGive(tile.done);
#if WFD_RENDER_PROFILE
    render_profile::last_copy_us += esp_timer_get_time() - copy_started;
#endif
    span_count = 0;
    active_tile ^= 1;
    wait_tile(tiles[active_tile]);
  };
  const auto changed = renderer.render_by_line<Pixel>([&](size_t y, size_t x0, size_t x1, auto rasterize) {
#if WFD_RENDER_PROFILE
    if (first_line) {
      render_profile::last_scene_us += esp_timer_get_time() - started;
      first_line = false;
    }
#endif
    if (span_count && (y < first_y || y - first_y >= rows || span_count == spans.size()))
      flush();
    if (!span_count)
      first_y = y;
    auto *target = tiles[active_tile].pixels + (y - first_y) * board::panel_width + x0;
    rasterize(std::span<Pixel>(target, x1 - x0));
    spans[span_count++] = {y, x0, x1};
    pixels += x1 - x0;
  });
  flush();
  for (auto &tile : tiles)
    wait_tile(tile);
  for (const auto &[origin, size] : changed.rectangles()) {
    configASSERT(painted_count < painted.size());
    painted[painted_count++] = {origin.x, origin.y, int(size.width), int(size.height)};
  }
  const auto size = changed.bounding_box_size();
  if (!size.width || !size.height)
    return {output, {}, wait_us, pixels};
  const auto origin = changed.bounding_box_origin();
  return {output, {int(origin.x), int(origin.y), int(size.width), int(size.height)}, wait_us, pixels};
}

Work render(slint::platform::SoftwareRenderer &renderer, board::Display &display, bike::Orientation orientation) {
  painted_count = 0;
#if WFD_RENDER_PROFILE
  const auto started = esp_timer_get_time();
  render_profile::last_scene_us = 0;
  render_profile::last_copy_us = 0;
  render_profile::last_copy_calls = 0;
  render_profile::current_phase = 0;
#endif
  auto state = read_slide ? read_slide() : PageSlideState{};
  const auto &v = state.viewport;
  const bool valid = v.x >= 0 && v.y >= 0 && v.width > 0 && v.height > 0 &&
                     v.x + v.width <= bike::display_width(orientation) &&
                     v.y + v.height <= bike::display_height(orientation);
  if (!state.content_active || (pages_primed && primed_orientation != orientation))
    pages_primed = false;
  uint16_t *primed_output = nullptr;
  uint32_t prime_wait = 0;
  if (!pages_primed && state.available && !state.active && valid) {
    composing = false;
    pending_old = nullptr;
    const auto before_acquire = esp_timer_get_time();
    ESP_ERROR_CHECK(board::acquire_frame(&primed_output));
    prime_wait += esp_timer_get_time() - before_acquire;
    // Warm retained layouts offscreen. The full-window clear also invalidates
    // chrome after resizing or interrupting a slide.
    select_capture(-2);
    painted_count = 0;
    const auto cleared = rasterize(renderer, primed_output);
    prime_wait += cleared.wait_us;
    for (int page = 0; page < 7; ++page) {
      select_capture(page);
      painted_count = 0;
      const auto warm = rasterize(renderer, primed_output);
      prime_wait += warm.wait_us;
    }
    select_capture(-1);
    painted_count = 0;
    pages_primed = true;
    primed_orientation = orientation;
  }
  RasterWork work;
  if (state.active && valid && state.incoming >= 0 && state.outgoing >= 0 && board::last_frame()) {
#if WFD_RENDER_PROFILE
    render_profile::current_phase = 2;
#endif
    const auto viewport = bike::native_slide_rect(v, orientation);
    bool prepared = false;
    if (!composing || cached_slide.incoming != state.incoming || cached_slide.outgoing != state.outgoing ||
        cached_orientation != orientation) {
#if WFD_RENDER_PROFILE
      render_profile::current_phase = 1;
#endif
      if (composing && cached_slide.outgoing == state.outgoing && cached_orientation == orientation) {
        if (pending_old) {
          const auto before_acquire = esp_timer_get_time();
          ESP_ERROR_CHECK(board::acquire_frame(&work.output));
          work.wait_us += esp_timer_get_time() - before_acquire;
          work.wait_us += copy_page(pending_old, snapshots[0], viewport, viewport);
          pending_old = nullptr;
        }
      }
      else if (composing && cached_slide.incoming == state.outgoing && cached_orientation == orientation) {
        work.wait_us += copy_page(snapshots[1], snapshots[0], viewport, viewport);
        pending_old = nullptr;
      }
      else {
        // Read-only until the next acquisition retires this framebuffer.
        pending_old = board::last_frame();
      }
      select_capture(state.incoming);
      if (!work.output) {
        const auto before_acquire = esp_timer_get_time();
        ESP_ERROR_CHECK(board::acquire_frame(&work.output));
        work.wait_us += esp_timer_get_time() - before_acquire;
      }
      work.wait_us += clear_page(work.output, viewport);
      const auto captured = rasterize(renderer, work.output);
      configASSERT(captured.output);
      work.output = captured.output;
      work.wait_us += captured.wait_us;
      work.pixels += captured.pixels;
      work.wait_us += copy_page(work.output, snapshots[1], viewport, viewport);
      // Keep a stationary Slint scene; restore retired buffers before applying damage.
      cached_slide = state;
      cached_orientation = orientation;
      composing = true;
      prepared = true;
      slint::platform::update_timers_and_animations();
      const auto refreshed = read_slide();
      if (refreshed.active && refreshed.incoming == state.incoming && refreshed.outgoing == state.outgoing)
        state.progress = refreshed.progress;
      else
        state.progress = !refreshed.active && refreshed.incoming == state.outgoing ? 0.f : 1.f;
    }
    if (!prepared) {
      const auto before_acquire = esp_timer_get_time();
      ESP_ERROR_CHECK(board::acquire_frame(&work.output));
      work.wait_us += esp_timer_get_time() - before_acquire;
      if (pending_old) {
        // Save the outgoing viewport before overwriting its retired framebuffer.
        work.wait_us += copy_page(pending_old, snapshots[0], viewport, viewport);
        pending_old = nullptr;
      }
      const auto updated = rasterize(renderer, work.output, snapshots[1], viewport);
      work.wait_us += updated.wait_us;
      work.pixels += updated.pixels;
    }
    const int travel = std::clamp(int(float(v.width) * state.progress + .5f), 0, v.width);
    const auto regions = bike::page_slide_blits(v, state.direction, travel);
    for (unsigned i = 0; i < regions.size(); ++i) {
      const auto src = bike::native_slide_rect(regions[i].source, orientation);
      const auto dst = bike::native_slide_rect(regions[i].destination, orientation);
      const auto *source = i == 0 && pending_old ? pending_old : snapshots[i];
      work.wait_us += copy_page(source, work.output, src, dst);
      work.pixels += src.width * src.height;
    }
    work.region = {0, 0, board::panel_width, board::panel_height};
    configASSERT(painted_count < painted.size());
    painted[painted_count++] = viewport;
  }
  else {
    if (composing) {
#if WFD_RENDER_PROFILE
      render_profile::current_phase = 3;
#endif
      const auto before_acquire = esp_timer_get_time();
      ESP_ERROR_CHECK(board::acquire_frame(&work.output));
      work.wait_us += esp_timer_get_time() - before_acquire;
      const auto viewport = bike::native_slide_rect(cached_slide.viewport, cached_orientation);
      const bool returning = state.incoming == cached_slide.outgoing;
      const auto *settled = returning ? (pending_old ? pending_old : snapshots[0]) : snapshots[1];
      if (settled != work.output)
        work.wait_us += copy_page(settled, work.output, viewport, viewport);
      // Keep the stationary scene until pointer input releases capture.
      select_capture(state.available ? state.incoming : -1);
      composing = false;
      pending_old = nullptr;
      const auto updated = rasterize(renderer, work.output);
      work.wait_us += updated.wait_us;
      work.pixels += updated.pixels;
      work.region = {0, 0, board::panel_width, board::panel_height};
      configASSERT(painted_count < painted.size());
      painted[painted_count++] = viewport;
    }
    else {
      if (captured_page >= 0 && (!state.available || captured_page != state.incoming))
        select_capture(-1);
      work = rasterize(renderer, primed_output);
    }
  }
  work.wait_us += prime_wait;
  if (primed_output) {
    // Every preload used this same retired buffer, including its fixed chrome.
    painted_count = 1;
    painted[0] = {0, 0, board::panel_width, board::panel_height};
    work.region = {0, 0, board::panel_width, board::panel_height};
  }
  // The retired buffer is two frames old. Restore missing regions from the
  // latest front buffer without overwriting freshly painted pixels.
  std::array<bike::SlideRect, 128> missing;
  const auto count = bike::unpainted_slide_regions(
      previous_damage, std::span<const bike::SlideRect>(painted.data(), painted_count), missing);
  configASSERT(count);
  bike::SlideRect next_damage;
  for (size_t i = 0; i < painted_count; ++i) {
    const auto &r = painted[i];
    if (!next_damage.width) {
      next_damage = r;
    }
    else {
      const int right = std::max(next_damage.x + next_damage.width, r.x + r.width);
      const int bottom = std::max(next_damage.y + next_damage.height, r.y + r.height);
      next_damage.x = std::min(next_damage.x, r.x);
      next_damage.y = std::min(next_damage.y, r.y);
      next_damage.width = right - next_damage.x;
      next_damage.height = bottom - next_damage.y;
    }
  }
  if (*count && board::last_frame()) {
    if (!work.output) {
      const auto before_acquire = esp_timer_get_time();
      ESP_ERROR_CHECK(board::acquire_frame(&work.output));
      work.wait_us += esp_timer_get_time() - before_acquire;
    }
    for (size_t i = 0; i < *count; ++i)
      work.wait_us += copy_page(board::last_frame(), work.output, missing[i], missing[i]);
    work.region = {0, 0, board::panel_width, board::panel_height};
  }
  previous_damage = {next_damage.x, next_damage.y, next_damage.width, next_damage.height};
  if (!work.region.width || !work.region.height)
    return {};
  const auto before_present = esp_timer_get_time();
  ESP_ERROR_CHECK(board::present_frame(display, work.region));
#if WFD_RENDER_PROFILE
  render_profile::record(esp_timer_get_time() - started - work.wait_us, work.pixels, work.wait_us);
#endif
  return {work.wait_us + uint32_t(esp_timer_get_time() - before_present), 1, work.pixels};
}

} // namespace slint_rendering
