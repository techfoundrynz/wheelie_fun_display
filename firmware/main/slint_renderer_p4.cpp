#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "slint_rendering.hpp"
#include <span>

namespace slint_rendering
{
namespace
{

using Pixel = slint::platform::Rgb565Pixel;
constexpr size_t pixel_count = size_t(board::panel_width) * board::panel_height;
Pixel *framebuffer;
bike::Orientation presented_orientation = board::orientation;
bool presented = false;

} // namespace

void initialize() {
  // The logical buffer is persistent: Slint can retain unchanged pixels while
  // PPA rotates each completed frame into the inactive DSI scanout buffer.
  framebuffer = static_cast<Pixel *>(
      heap_caps_aligned_calloc(64, pixel_count, sizeof(Pixel), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  configASSERT(framebuffer);
}

slint::platform::SoftwareRenderer::RenderingRotation rotation(bike::Orientation) {
  return slint::platform::SoftwareRenderer::RenderingRotation::NoRotation;
}

Work render(slint::platform::SoftwareRenderer &renderer, board::Display &display, bike::Orientation orientation) {
  const auto changed = renderer.render(std::span<Pixel>(framebuffer, pixel_count), bike::display_width(orientation));
  const auto size = changed.bounding_box_size();
  if ((!size.width || !size.height) && presented && orientation == presented_orientation)
    return {};
  const auto before = esp_timer_get_time();
  ESP_ERROR_CHECK(board::present_frame(display, framebuffer, orientation));
  presented_orientation = orientation;
  presented = true;
  return {uint32_t(esp_timer_get_time() - before), 1, uint32_t(pixel_count)};
}

} // namespace slint_rendering
