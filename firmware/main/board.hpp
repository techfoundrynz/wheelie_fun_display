#pragma once
#include "bike/orientation.hpp"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "hardware_metadata.h"
#include <cstdint>

namespace board
{

#ifndef DISPLAY_ROTATION

  #define DISPLAY_ROTATION 0
#endif
static_assert(bike::valid_rotation(DISPLAY_ROTATION), "DISPLAY_ROTATION must be 0, 90, 180 or 270 degrees");

constexpr auto orientation = bike::orientation_from_degrees(DISPLAY_ROTATION);
constexpr int panel_width = bike::panel_width, panel_height = bike::panel_height;

constexpr int width = bike::display_width(orientation), height = bike::display_height(orientation);

constexpr int tile_rows = 10;
constexpr const char *label = WFD_HARDWARE_LABEL;
constexpr const char *release_asset = HW_TYPE ".bin";
#if DISPLAY_NATIVE_RGB565
constexpr uint32_t display_clock_hz = DISP_DPI_CLOCK_MHZ * 1000000;
constexpr const char *display_bus_name = "MIPI DSI";
#else
constexpr uint32_t display_clock_hz = DISP_QSPI_HZ;
constexpr const char *display_bus_name = "QSPI";
#endif

struct Display {
  esp_lcd_panel_handle_t panel = nullptr;
  esp_lcd_touch_handle_t touch = nullptr;
  SemaphoreHandle_t dma_done = nullptr;
};

Display init();

void brightness(int percent); // UI task only, after DMA completion

esp_err_t init_radio();

// Present one complete logical RGB565 frame using hardware rotation. P4 only.
esp_err_t present_frame(Display &display, const void *pixels, bike::Orientation orientation);

// Submit one compact native-panel region; dma_done signals when its source is reusable.
esp_err_t draw_region(Display &display, int x0, int y0, int x1, int y1, const void *pixels);

// Complete presentation after all submitted regions are finished.
esp_err_t present(Display &display);

} // namespace board
