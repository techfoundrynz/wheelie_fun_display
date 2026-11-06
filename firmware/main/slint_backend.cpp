#include "slint_backend.hpp"
#include "esp_timer.h"
#include "slint-platform.h"
#include "slint_rendering.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>

namespace
{

bike::Orientation orientation = board::orientation;

class Window final : public slint::platform::WindowAdapter {
public:

  slint::platform::SoftwareRenderer software{slint::platform::SoftwareRenderer::RepaintBufferType::ReusedBuffer};

  Window() {
    software.set_rendering_rotation(slint_rendering::rotation(orientation));
    software.set_dirty_region_alignment(2, 2);
  }

  bool dirty = true;

  slint::PhysicalSize size() override {
    return slint::PhysicalSize(
        {uint32_t(bike::display_width(orientation)), uint32_t(bike::display_height(orientation))});
  }

  slint::platform::AbstractRenderer &renderer() override {
    return software;
  }

  void request_redraw() override {
    dirty = true;
  }

  void set_visible(bool visible) override {
    if (visible)
      window().dispatch_resize_event(
          slint::LogicalSize({float(bike::display_width(orientation)), float(bike::display_height(orientation))}));
  }
};

Window *window_adapter;
board::Display hardware;
RenderStats stats;
bool pressed = false;
bool wait_for_touch_release = false;
bool sleeping = false, wake_requested = false;
bool input_cancel_requested = false;
uint64_t last_activity = 0;
slint::LogicalPosition last_point{};

class Platform final : public slint::platform::Platform {
  std::unique_ptr<slint::platform::WindowAdapter> create_window_adapter() override {
    auto w = std::make_unique<Window>();
    window_adapter = w.get();
    return w;
  }

  std::chrono::milliseconds duration_since_start() override {
    return std::chrono::milliseconds(esp_timer_get_time() / 1000);
  }
};

} // namespace

void init_slint_backend(board::Display display) {
  hardware = display;
  slint_rendering::initialize();
  slint::platform::set_platform(std::make_unique<Platform>());
}

void render_slint_frame() {
  if (!window_adapter)
    return;
  auto &win = window_adapter->window();
  if (input_cancel_requested) {
    input_cancel_requested = false;
    if (pressed) {
      win.dispatch_pointer_exit_event();
      win.dispatch_pointer_release_event(slint::LogicalPosition({-1.f, -1.f}), slint::PointerEventButton::Left);
    }
    pressed = false;
    wait_for_touch_release = true;
  }
  // A failed read is not evidence of a finger release. In particular, it must
  // not end the release gate used when rotating a held contact.
  esp_lcd_touch_point_data_t point{};
  uint8_t count = 0;
  const bool valid = esp_lcd_touch_read_data(hardware.touch) == ESP_OK &&
                     esp_lcd_touch_get_data(hardware.touch, &point, &count, 1) == ESP_OK;
  const bool down = count != 0;
  if (valid && down)
    last_activity = esp_timer_get_time() / 1000;
  if (valid && down && sleeping) {
    wake_requested = true;
    wait_for_touch_release = true;
  }
  else if (valid && wait_for_touch_release) {
    if (!down)
      wait_for_touch_release = false;
  }
  else if (valid && down) {
    const auto logical = bike::to_logical({point.x, point.y}, orientation);
    const auto next_point = slint::LogicalPosition({float(logical.x), float(logical.y)});
    if (!pressed)
      win.dispatch_pointer_press_event(next_point, slint::PointerEventButton::Left);
    else if (next_point.x != last_point.x || next_point.y != last_point.y)
      win.dispatch_pointer_move_event(next_point);
    last_point = next_point;
    pressed = true;
  }
  else if (valid && pressed) {
    win.dispatch_pointer_release_event(last_point, slint::PointerEventButton::Left);
    win.dispatch_pointer_exit_event();
    pressed = false;
  }
  slint::platform::update_timers_and_animations();
  if (!window_adapter->dirty)
    return;
  window_adapter->dirty = false;
  const int64_t started = esp_timer_get_time();
  const auto work = slint_rendering::render(window_adapter->software, hardware, orientation);
  const uint32_t elapsed = esp_timer_get_time() - started;
  auto average = [&](uint32_t previous, uint32_t value) { return stats.frames ? (previous * 7 + value) / 8 : value; };
  stats.frame_us = average(stats.frame_us, elapsed);
  stats.wait_us = average(stats.wait_us, work.wait_us);
  stats.chunks = average(stats.chunks, work.calls);
  stats.pixels = average(stats.pixels, work.pixels);
  stats.max_frame_us = std::max(stats.max_frame_us, elapsed);
  ++stats.frames;
}

void set_slint_orientation(bike::Orientation requested) {
  if (requested != orientation) {
    // Cancel in the old coordinate space, outside every control, before the
    // resize. A held finger must lift before it can start a new gesture.
    if (window_adapter && pressed) {
      auto &win = window_adapter->window();
      win.dispatch_pointer_exit_event();
      win.dispatch_pointer_release_event(slint::LogicalPosition({-1.f, -1.f}), slint::PointerEventButton::Left);
    }
    pressed = false;
    wait_for_touch_release = true;
  }
  orientation = requested;
  if (!window_adapter)
    return;
  window_adapter->software.set_rendering_rotation(slint_rendering::rotation(orientation));
  window_adapter->window().dispatch_resize_event(
      slint::LogicalSize({float(bike::display_width(orientation)), float(bike::display_height(orientation))}));
  window_adapter->dirty = true;
}

RenderStats slint_render_stats() {
  return stats; // UI task only, like rendering and About projection.
}

void set_slint_sleeping(bool asleep) {
  if (asleep && window_adapter && pressed) {
    auto &win = window_adapter->window();
    win.dispatch_pointer_exit_event();
    win.dispatch_pointer_release_event(slint::LogicalPosition({-1.f, -1.f}), slint::PointerEventButton::Left);
    pressed = false;
    wait_for_touch_release = true;
  }
  sleeping = asleep;
}

bool take_slint_wake() {
  const bool wake = wake_requested;
  wake_requested = false;
  return wake;
}

void cancel_slint_input() {
  input_cancel_requested = true;
}

uint64_t slint_last_activity_ms() {
  return last_activity;
}

uint32_t slint_loop_delay_ms(uint32_t iteration_us) {
  const bool active = pressed || (window_adapter && window_adapter->window().has_active_animations());
  const uint32_t budget_us = active ? 16667 : 10000;
  // Rendering and DMA waits count towards the budget. Always give the idle
  // task at least one tick; preserve watchdog coverage even when overloaded.
  return iteration_us < budget_us ? std::max<uint32_t>(1, (budget_us - iteration_us) / 1000) : 1;
}
