#include "bike/scanline_chunks.hpp"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "slint_rendering.hpp"

namespace slint_rendering
{
namespace
{

using Pixel = slint::platform::Rgb565BigEndianPixel;
constexpr int accumulator_count = 3; // pinned renderer DirtyRegion::MAX_COUNT
Pixel *tiles[accumulator_count];

} // namespace

void initialize() {
  for (auto &tile : tiles) {
    tile = static_cast<Pixel *>(
        heap_caps_malloc(board::panel_width * board::tile_rows * sizeof(Pixel), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    configASSERT(tile);
  }
}

slint::platform::SoftwareRenderer::RenderingRotation rotation(bike::Orientation value) {
  using Rotation = slint::platform::SoftwareRenderer::RenderingRotation;
  switch (value) {
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

Work render(slint::platform::SoftwareRenderer &renderer, board::Display &display, bike::Orientation) {
  uint32_t wait_us = 0, calls = 0, pixels = 0;
  using Chunks = bike::ScanlineChunks<Pixel, board::panel_width, board::panel_height, board::tile_rows>;
  std::array<std::span<Pixel>, accumulator_count> buffers;
  for (int i = 0; i < accumulator_count; ++i)
    buffers[i] = {tiles[i], board::panel_width * board::tile_rows};
  Chunks chunks(buffers);
  auto wait = [&]() {
    const int64_t before = esp_timer_get_time();
    if (xSemaphoreTake(display.dma_done, pdMS_TO_TICKS(1000)) != pdTRUE)
      ESP_ERROR_CHECK(ESP_ERR_TIMEOUT);
    wait_us += esp_timer_get_time() - before;
  };
  auto submit = [&](size_t x0, size_t y0, size_t x1, size_t y1, std::span<Pixel> data) {
    ESP_ERROR_CHECK(board::draw_region(display, x0, y0, x1, y1, data.data()));
    ++calls;
    pixels += data.size();
  };
  renderer.render_by_line<Pixel>(
      [&](size_t y, size_t x0, size_t x1, auto render) { chunks.line(y, x0, x1, render, submit, wait); });
  chunks.finish(submit, wait);
  if (calls) {
    const int64_t before_present = esp_timer_get_time();
    ESP_ERROR_CHECK(board::present(display));
    wait_us += esp_timer_get_time() - before_present;
  }
  return {wait_us, calls, pixels};
}

} // namespace slint_rendering
