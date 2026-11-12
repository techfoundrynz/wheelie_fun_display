#pragma once
#include <cstdint>
#if __has_include("render_profile_config.hpp")
  #include "render_profile_config.hpp"
#endif

#ifndef WFD_RENDER_PROFILE
  #define WFD_RENDER_PROFILE 0
#endif
#ifndef WFD_RENDER_PROFILE_SECONDS
  #define WFD_RENDER_PROFILE_SECONDS 10
#endif
#ifndef WFD_RENDER_PROFILE_START
  #define WFD_RENDER_PROFILE_START 0
#endif

#if WFD_RENDER_PROFILE
  #include "esp_heap_caps.h"
  #include "esp_log.h"
  #include "esp_timer.h"
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "slint-platform.h"
  #include "slint_backend.hpp"
  #include "slint_rendering.hpp"
  #include <algorithm>
  #include <array>
  #include <limits>

namespace render_profile
{

inline bool active = false;
inline bool owns_input = false;
inline uint32_t last_submit_us = 0, last_refresh_us = 0;
inline uint32_t last_copy_us = 0;
inline uint32_t last_touch_us = 0;
inline uint32_t last_copy_calls = 0;
inline uint32_t last_scene_us = 0;
inline unsigned current_page = 0;
inline unsigned current_phase = 0;
inline bool motion_expected = false;
inline uint64_t motion_idle_us = 0, last_step_us = 0;

struct PageStatistics {
  uint64_t frames = 0, raster_us = 0, scene_us = 0;
  uint32_t slow_frames = 0, max_raster_us = 0;
};

inline std::array<PageStatistics, 7> page_stats;
inline std::array<PageStatistics, 4> phase_stats;

struct Statistics {
  uint64_t started_us = 0, frames = 0, raster_us = 0, submit_us = 0, refresh_us = 0;
  uint64_t dirty_pixels = 0, last_frame_us = 0, bucket_started_us = 0, wait_us = 0;
  uint64_t copy_us = 0;
  uint64_t touch_us = 0;
  uint64_t copy_calls = 0;
  uint64_t scene_us = 0;
  uint32_t max_raster_us = 0, bucket_frames = 0, min_fps_x100 = std::numeric_limits<uint32_t>::max();
  uint32_t min_bucket_at_ms = 0;
  uint32_t max_touch_us = 0;
  std::array<uint32_t, 128> intervals_ms{};
};

inline Statistics stats;

inline void begin(const char *kind, bike::Orientation orientation) {
  stats = {};
  page_stats = {};
  phase_stats = {};
  stats.started_us = stats.bucket_started_us = esp_timer_get_time();
  motion_idle_us = 0;
  last_step_us = stats.started_us;
  motion_expected = true;
  active = true;
  ESP_LOGI("render_profile", "START kind=%s rotation=%d", kind, int(orientation));
}

inline void record(uint32_t raster_us, uint64_t dirty_pixels, uint32_t wait_us) {
  if (!active)
    return;
  const uint64_t now = esp_timer_get_time();
  auto &page = page_stats[std::min<unsigned>(6, current_page)];
  ++page.frames;
  page.raster_us += raster_us;
  page.scene_us += last_scene_us;
  page.slow_frames += raster_us > 20000;
  page.max_raster_us = std::max(page.max_raster_us, raster_us);
  auto &phase = phase_stats[std::min<unsigned>(3, current_phase)];
  ++phase.frames;
  phase.raster_us += raster_us;
  phase.scene_us += last_scene_us;
  phase.slow_frames += raster_us > 20000;
  phase.max_raster_us = std::max(phase.max_raster_us, raster_us);
  if (stats.last_frame_us)
    ++stats.intervals_ms[std::min<uint64_t>(127, (now - stats.last_frame_us + 999) / 1000)];
  stats.last_frame_us = now;
  ++stats.frames;
  ++stats.bucket_frames;
  stats.raster_us += raster_us;
  stats.wait_us += wait_us;
  stats.submit_us += last_submit_us;
  stats.refresh_us += last_refresh_us;
  stats.dirty_pixels += dirty_pixels;
  stats.copy_us += last_copy_us;
  stats.touch_us += last_touch_us;
  stats.max_touch_us = std::max(stats.max_touch_us, last_touch_us);
  stats.copy_calls += last_copy_calls;
  stats.scene_us += last_scene_us;
  stats.max_raster_us = std::max(stats.max_raster_us, raster_us);
  if (now - stats.bucket_started_us >= 1000000) {
    const uint32_t fps = uint64_t(stats.bucket_frames) * 100000000 / (now - stats.bucket_started_us);
    if (fps < stats.min_fps_x100) {
      stats.min_fps_x100 = fps;
      stats.min_bucket_at_ms = (now - stats.started_us) / 1000;
    }
    stats.bucket_frames = 0;
    stats.bucket_started_us = now;
  }
}

inline void finish(const char *kind, bike::Orientation orientation) {
  active = false;
  const auto elapsed_us = esp_timer_get_time() - stats.started_us;
  const auto n = std::max<uint64_t>(1, stats.frames);
  uint32_t count = 0, p95 = 127;
  for (unsigned i = 0; i < stats.intervals_ms.size(); ++i) {
    count += stats.intervals_ms[i];
    if (count * 100 >= (n - 1) * 95) {
      p95 = i;
      break;
    }
  }
  ESP_LOGI("render_profile",
           "RESULT kind=%s rotation=%d frames=%llu duration_ms=%llu fps_x100=%llu min_1s_fps_x100=%u p95_ms=%u", kind,
           int(orientation), (unsigned long long)stats.frames, (unsigned long long)(elapsed_us / 1000),
           (unsigned long long)(stats.frames * 100000000 / elapsed_us), unsigned(stats.min_fps_x100), p95);
  // Keep the critical result below the secondary USB console's small FIFO.
  vTaskDelay(1);
  ESP_LOGI("render_profile", "PACING render_wait_avg_us=%llu motion_idle_ms=%llu min_bucket_at_ms=%u max_touch_us=%u",
           (unsigned long long)(stats.wait_us / n), (unsigned long long)(motion_idle_us / 1000), stats.min_bucket_at_ms,
           stats.max_touch_us);
  ESP_LOGI("render_profile", "MEMORY psram_free=%u psram_largest=%u internal_min=%u",
           unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
           unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)),
           unsigned(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)));
  ESP_LOGI("render_profile",
           "TIMING raster_avg_us=%llu submit_avg_us=%llu refresh_avg_us=%llu "
           "dirty_avg_pixels=%llu max_raster_us=%u copy_avg_us=%llu touch_avg_us=%llu copy_calls=%llu "
           "scene_us=%llu internal_free=%u internal_largest=%u dma_free=%u dma_largest=%u ui_stack_free=%u",
           (unsigned long long)(stats.raster_us / n), (unsigned long long)(stats.submit_us / n),
           (unsigned long long)(stats.refresh_us / n), (unsigned long long)(stats.dirty_pixels / n),
           unsigned(stats.max_raster_us), (unsigned long long)(stats.copy_us / n),
           (unsigned long long)(stats.touch_us / n), (unsigned long long)(stats.copy_calls / n),
           (unsigned long long)(stats.scene_us / n), unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
           unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
           unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)),
           unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)),
           unsigned(uxTaskGetStackHighWaterMark(nullptr)));
  for (unsigned i = 0; i < page_stats.size(); ++i) {
    const auto &page = page_stats[i];
    if (page.frames)
      ESP_LOGI(
          "render_profile", "PAGE index=%u frames=%llu raster_us=%llu scene_us=%llu slow_frames=%u max_raster_us=%u", i,
          (unsigned long long)page.frames, (unsigned long long)(page.raster_us / page.frames),
          (unsigned long long)(page.scene_us / page.frames), unsigned(page.slow_frames), unsigned(page.max_raster_us));
  }
  for (unsigned i = 0; i < phase_stats.size(); ++i) {
    const auto &phase = phase_stats[i];
    if (phase.frames)
      ESP_LOGI("render_profile",
               "PHASE index=%u frames=%llu raster_us=%llu scene_us=%llu slow_frames=%u max_raster_us=%u", i,
               (unsigned long long)phase.frames, (unsigned long long)(phase.raster_us / phase.frames),
               (unsigned long long)(phase.scene_us / phase.frames), unsigned(phase.slow_frames),
               unsigned(phase.max_raster_us));
  }
}

template <class UI> void step(const UI &ui, bike::Orientation &orientation, uint64_t now_ms, uint64_t ui_started_ms) {
  static unsigned state = 0, stage = WFD_RENDER_PROFILE_START;
  static uint64_t ready_at = 0;
  static bike::Orientation original_orientation;
  static int original_page = 0;
  static bool original_animations = true;
  static slint::LogicalPosition last_point{};
  static bool swipe_down = false;
  static bool stats_prepared = false;
  static unsigned positioning_drags = 0;
  static int swipe_direction = 1;
  static uint64_t swipe_started_ms = 0;
  static uint64_t swipe_ready_ms = 0;
  constexpr std::array rotations{bike::Orientation::Landscape, bike::Orientation::Portrait,
                                 bike::Orientation::LandscapeInverted, bike::Orientation::PortraitInverted};
  auto configure = [&](bike::Orientation requested, int page, bool animations) {
    ui->set_animations_enabled(false);
    ui->set_page(page);
    orientation = requested;
    set_slint_orientation(orientation);
    ui->set_rotation(int(orientation));
    ui->set_initial_width(bike::display_width(orientation));
    ui->set_initial_height(bike::display_height(orientation));
    ui->set_animations_enabled(animations);
  };
  auto input_viewport = [&] {
    const float w = ui->get_page_viewport_width(), h = ui->get_page_viewport_height();
    return w > 0 && h > 0 ? std::array<float, 4>{ui->get_page_viewport_x(), ui->get_page_viewport_y(), w, h}
                          : std::array<float, 4>{0, 0, float(bike::display_width(orientation)),
                                                 float(bike::display_height(orientation))};
  };
  const bool stats_flicks = stage >= 12;
  const bool swipes = stage >= 8 && !stats_flicks;
  const bool scroll = stage < 8 && stage % 2;
  const char *kind = stats_flicks ? "stats_flicks" : swipes ? "swipes" : scroll ? "scroll" : "transitions";
  if (active) {
    const auto now = esp_timer_get_time();
    if (!motion_expected)
      motion_idle_us += now - last_step_us;
    last_step_us = now;
  }
  if (state == 0) {
    // Leave a manual-check window after startup.
    if (now_ms - ui_started_ms < 60000 || !ui->get_page_input_allowed())
      return;
    original_orientation = orientation;
    ui->window().dispatch_pointer_exit_event();
    ui->window().dispatch_pointer_release_event(slint::LogicalPosition({-1.f, -1.f}), slint::PointerEventButton::Left);
    owns_input = true;
    original_page = ui->get_page();
    original_animations = ui->get_animations_enabled();
    state = 1;
  }
  if (state != 4 && !ui->get_page_input_allowed()) {
    ui->window().dispatch_pointer_exit_event();
    ui->window().dispatch_pointer_release_event(slint::LogicalPosition({-1.f, -1.f}), slint::PointerEventButton::Left);
    active = false;
    owns_input = false;
    configure(original_orientation, original_page, original_animations);
    state = 4;
    ESP_LOGW("render_profile", "ABORT modal or locked screen; preferences restored without NVS writes");
    return;
  }
  if (state == 1) {
    configure(rotations[stats_flicks ? stage - 12
                        : swipes     ? stage - 8
                                     : stage / 2],
              stats_flicks ? 1
              : scroll     ? 3
                           : 0,
              true);
    swipe_down = false;
    stats_prepared = false;
    positioning_drags = 0;
    swipe_direction = 1;
    swipe_ready_ms = 0;
    ready_at = now_ms + 1000;
    state = 2;
  }
  if (state == 2 && now_ms >= ready_at) {
    if (stats_flicks && !stats_prepared) {
      // Position with in-bounds contacts; leaving the window can cancel the drag.
      const auto v = input_viewport();
      const auto start = slint::LogicalPosition({v[0] + v[2] * .5f, v[1] + v[3] * .2f});
      const auto top = slint::LogicalPosition({start.x, v[1] + v[3] * .8f});
      slint_rendering::prepare_pointer_input();
      ui->window().dispatch_pointer_press_event(start, slint::PointerEventButton::Left);
      slint::platform::update_timers_and_animations();
      ui->window().dispatch_pointer_move_event(top);
      slint::platform::update_timers_and_animations();
      ui->window().dispatch_pointer_exit_event();
      ui->window().dispatch_pointer_release_event(slint::LogicalPosition({-1.f, -1.f}),
                                                  slint::PointerEventButton::Left);
      slint::platform::update_timers_and_animations();
      if (++positioning_drags < 12) {
        ready_at = now_ms + 20;
        return;
      }
      // Stay away from scroll bounds, where animation would correctly stop.
      const auto middle = slint::LogicalPosition({v[0] + v[2] * .5f, v[1] + v[3] * .8f});
      ui->window().dispatch_pointer_press_event(middle, slint::PointerEventButton::Left);
      slint::platform::update_timers_and_animations();
      ui->window().dispatch_pointer_move_event(slint::LogicalPosition({middle.x, middle.y - v[3] * .4f}));
      slint::platform::update_timers_and_animations();
      ui->window().dispatch_pointer_exit_event();
      ui->window().dispatch_pointer_release_event(slint::LogicalPosition({-1.f, -1.f}),
                                                  slint::PointerEventButton::Left);
      slint::platform::update_timers_and_animations();
      stats_prepared = true;
      ready_at = now_ms + 200;
      return;
    }
    begin(kind, orientation);
    if (scroll) {
      const auto v = input_viewport();
      last_point = slint::LogicalPosition({v[0] + v[2] * .5f, v[1] + v[3] * .8f});
      ESP_LOGI("render_profile", "SCROLL_VIEWPORT x=%d y=%d width=%d height=%d", int(v[0]), int(v[1]), int(v[2]),
               int(v[3]));
      slint_rendering::prepare_pointer_input();
      ui->window().dispatch_pointer_press_event(last_point, slint::PointerEventButton::Left);
    }
    state = 3;
  }
  if (state != 3)
    return;
  if (scroll && ui->get_page() != 3) {
    active = false;
    ui->window().dispatch_pointer_exit_event();
    ui->window().dispatch_pointer_release_event(slint::LogicalPosition({-1.f, -1.f}), slint::PointerEventButton::Left);
    state = 1;
    ESP_LOGW("render_profile", "RESTART scroll interrupted by navigation");
    return;
  }
  const auto elapsed_us = esp_timer_get_time() - stats.started_us;
  if (elapsed_us < uint64_t(WFD_RENDER_PROFILE_SECONDS) * 1000000) {
    if (swipes || stats_flicks) {
      const auto v = input_viewport();
      if (!swipe_down && now_ms >= swipe_ready_ms && !ui->get_transitioning()) {
        if (swipes)
          swipe_direction = ui->get_page() == 0 ? 1 : -1;
        swipe_started_ms = now_ms;
        last_point = slint::LogicalPosition({v[0] + v[2] * (stats_flicks          ? .5f
                                                            : swipe_direction > 0 ? .8f
                                                                                  : .2f),
                                             v[1] + v[3] * (stats_flicks ? swipe_direction > 0 ? .6f : .4f : .5f)});
        slint_rendering::prepare_pointer_input();
        ui->window().dispatch_pointer_press_event(last_point, slint::PointerEventButton::Left);
        swipe_down = true;
      }
      if (swipe_down) {
        const float travel = std::clamp(float(now_ms - swipe_started_ms) / 400.f, 0.f, 1.f);
        const float distance = stats_flicks ? .2f : .6f;
        const float x = swipe_direction > 0 ? (stats_flicks ? .6f : .8f) - distance * travel
                                            : (stats_flicks ? .4f : .2f) + distance * travel;
        last_point =
            slint::LogicalPosition({v[0] + v[2] * (stats_flicks ? .5f : x), v[1] + v[3] * (stats_flicks ? x : .5f)});
        ui->window().dispatch_pointer_move_event(last_point);
        if (travel >= 1.f) {
          ui->window().dispatch_pointer_release_event(last_point, slint::PointerEventButton::Left);
          ui->window().dispatch_pointer_exit_event();
          swipe_down = false;
          swipe_ready_ms = now_ms + (stats_flicks ? 20 : 0);
          if (stats_flicks)
            swipe_direction = -swipe_direction;
        }
      }
    }
    else if (!scroll) {
      if (!ui->get_transitioning())
        ui->set_page((ui->get_page() + 1) % 7);
    }
    else {
      const float phase = float((elapsed_us / 1000) % 1200) / 600.f;
      const float travel = phase <= 1.f ? phase : 2.f - phase;
      const auto v = input_viewport();
      last_point = slint::LogicalPosition({v[0] + v[2] * .5f, v[1] + v[3] * (.8f - .6f * travel)});
      ui->window().dispatch_pointer_move_event(last_point);
    }
    motion_expected = !stats_flicks || swipe_down || ui->window().has_active_animations();
  }
  else {
    if (scroll || swipe_down) {
      ui->window().dispatch_pointer_release_event(last_point, slint::PointerEventButton::Left);
      ui->window().dispatch_pointer_exit_event();
    }
    finish(kind, orientation);
    if (++stage < 16)
      state = 1;
    else {
      configure(original_orientation, original_page, original_animations);
      state = 4;
      owns_input = false;
      ESP_LOGI("render_profile", "DONE preferences restored without NVS writes");
    }
  }
}

} // namespace render_profile
#endif
