#include "board.hpp"
#include "boards/guition_st7701_init.hpp"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/ppa.h"
#include "eh_host_core.h"
#include "esp_cache.h"
#include "esp_check.h"
#include "esp_hosted_bt_host_stack.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_ldo_regulator.h"
#include "freertos/task.h"
#include <algorithm>
#include <atomic>
#include <cstring>

namespace board
{
namespace
{

esp_ldo_channel_handle_t phy_supply;
esp_lcd_dsi_bus_handle_t dsi;
esp_lcd_panel_io_handle_t command_io;
uint16_t *framebuffers[2]{};
unsigned back = 1;
ppa_client_handle_t rotator;
constexpr size_t framebuffer_bytes = size_t(panel_width) * panel_height * sizeof(uint16_t);
SemaphoreHandle_t refreshed;
std::atomic<uint32_t> refresh_generation{0};

static_assert(std::atomic<uint32_t>::is_always_lock_free);

bool brightness_ready = false;

bool IRAM_ATTR refresh_done(esp_lcd_panel_handle_t, esp_lcd_dpi_panel_event_data_t *, void *) {
  refresh_generation.fetch_add(1, std::memory_order_release);
  BaseType_t wake = pdFALSE;
  xSemaphoreGiveFromISR(refreshed, &wake);
  return wake == pdTRUE;
}

void reset_pin(int pin, unsigned low_ms, unsigned settle_ms) {
  gpio_config_t config{};
  config.pin_bit_mask = 1ULL << pin;
  config.mode = GPIO_MODE_OUTPUT;
  ESP_ERROR_CHECK(gpio_config(&config));
  ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(pin), 0));
  vTaskDelay(pdMS_TO_TICKS(low_ms));
  ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(pin), 1));
  vTaskDelay(pdMS_TO_TICKS(settle_ms));
}

void init_brightness() {
  ledc_timer_config_t timer{};
  timer.speed_mode = LEDC_LOW_SPEED_MODE;
  timer.duty_resolution = LEDC_TIMER_10_BIT;
  timer.timer_num = LEDC_TIMER_0;
  timer.freq_hz = 5000;
  timer.clk_cfg = LEDC_AUTO_CLK;
  ESP_ERROR_CHECK(ledc_timer_config(&timer));
  ledc_channel_config_t channel{};
  channel.gpio_num = DISP_BL;
  channel.speed_mode = LEDC_LOW_SPEED_MODE;
  channel.channel = LEDC_CHANNEL_0;
  channel.timer_sel = LEDC_TIMER_0;
  channel.duty = 0;
  ESP_ERROR_CHECK(ledc_channel_config(&channel));
  brightness_ready = true;
}

} // namespace

Display init() {
  init_brightness();
  Display display;
  refreshed = xSemaphoreCreateBinary();
  configASSERT(refreshed);

  esp_ldo_channel_config_t power{};
  power.chan_id = 3;
  power.voltage_mv = 2500;
  ESP_ERROR_CHECK(esp_ldo_acquire_channel(&power, &phy_supply));

  esp_lcd_dsi_bus_config_t bus{};
  bus.bus_id = 0;
  bus.num_data_lanes = DISP_DSI_LANES;
  bus.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT;
  bus.lane_bit_rate_mbps = DISP_DSI_MBPS;
  ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus, &dsi));
  esp_lcd_dbi_io_config_t control{};
  control.virtual_channel = 0;
  control.lcd_cmd_bits = 8;
  control.lcd_param_bits = 8;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsi, &control, &command_io));

  esp_lcd_dpi_panel_config_t dpi{};
  dpi.virtual_channel = 0;
  dpi.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
  dpi.dpi_clock_freq_mhz = DISP_DPI_CLOCK_MHZ;
  dpi.in_color_format = LCD_COLOR_FMT_RGB565;
  dpi.out_color_format = LCD_COLOR_FMT_RGB565;
  dpi.num_fbs = 2;
  dpi.video_timing.h_size = panel_width;
  dpi.video_timing.v_size = panel_height;
  dpi.video_timing.hsync_pulse_width = 12;
  dpi.video_timing.hsync_back_porch = 42;
  dpi.video_timing.hsync_front_porch = 42;
  dpi.video_timing.vsync_pulse_width = 2;
  dpi.video_timing.vsync_back_porch = 8;
  dpi.video_timing.vsync_front_porch = 166;
  ESP_ERROR_CHECK(esp_lcd_new_panel_dpi(dsi, &dpi, &display.panel));
  reset_pin(DISP_RST, 20, 120);
  for (const auto &entry : board_p4_panel_init_cmds) {
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(command_io, entry.cmd, entry.data, entry.len));
    if (entry.delay_ms)
      vTaskDelay(pdMS_TO_TICKS(entry.delay_ms));
  }
  void *first_framebuffer = nullptr, *second_framebuffer = nullptr;
  ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(display.panel, 2, &first_framebuffer, &second_framebuffer));
  framebuffers[0] = static_cast<uint16_t *>(first_framebuffer);
  framebuffers[1] = static_cast<uint16_t *>(second_framebuffer);
  const size_t bytes = size_t(panel_width) * panel_height * sizeof(uint16_t);
  for (auto *framebuffer : framebuffers) {
    std::memset(framebuffer, 0, bytes);
    ESP_ERROR_CHECK(esp_cache_msync(framebuffer, bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M));
  }
  esp_lcd_dpi_panel_event_callbacks_t callbacks{};
  callbacks.on_frame_buf_complete = refresh_done;
  ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(display.panel, &callbacks, nullptr));
  ESP_ERROR_CHECK(esp_lcd_panel_init(display.panel));
  ppa_client_config_t rotation_config{};
  rotation_config.oper_type = PPA_OPERATION_SRM;
  rotation_config.max_pending_trans_num = 1;
  ESP_ERROR_CHECK(ppa_register_client(&rotation_config, &rotator));

  i2c_master_bus_config_t i2c_config{};
  i2c_config.i2c_port = I2C_NUM_0;
  i2c_config.sda_io_num = static_cast<gpio_num_t>(I2C_SDA);
  i2c_config.scl_io_num = static_cast<gpio_num_t>(I2C_SCL);
  i2c_config.clk_source = I2C_CLK_SRC_DEFAULT;
  i2c_config.glitch_ignore_cnt = 7;
  i2c_config.flags.enable_internal_pullup = true;
  i2c_master_bus_handle_t i2c;
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_config, &i2c));
  reset_pin(TP_RST, 10, 50);
  int address = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS;
  if (i2c_master_probe(i2c, address, 100) != ESP_OK)
    address = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP;
  ESP_ERROR_CHECK(i2c_master_probe(i2c, address, 100));
  esp_lcd_panel_io_i2c_config_t touch_io{};
  touch_io.dev_addr = address;
  touch_io.control_phase_bytes = 1;
  touch_io.lcd_cmd_bits = 16;
  touch_io.flags.disable_control_phase = true;
  touch_io.scl_speed_hz = 400000;
  esp_lcd_panel_io_handle_t touch_control;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c, &touch_io, &touch_control));
  esp_lcd_touch_config_t touch{};
  touch.x_max = panel_width;
  touch.y_max = panel_height;
  touch.rst_gpio_num = GPIO_NUM_NC; // Reset completed before probing the strapped address.
  touch.int_gpio_num = GPIO_NUM_NC;
  ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(touch_control, &touch, &display.touch));
  brightness(70);
  return display;
}

void brightness(int percent) {
  if (!brightness_ready)
    return;
  ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, std::clamp(percent, 0, 100) * 1023 / 100));
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
}

esp_err_t init_radio() {
  ESP_RETURN_ON_ERROR(eh_host_init(nullptr), "radio", "Hosted initialization failed");
  ESP_RETURN_ON_ERROR(eh_host_connect_to_slave(), "radio", "C6 transport unavailable");
  esp_hosted_bt_host_stack_cfg_t config{};
  config.stack = ESP_HOSTED_BT_HOST_STACK_NIMBLE;
  config.bring_up_controller = true;
  config.controller_ready_timeout_ms = EH_BT_CTRL_DEFAULT_READY_TIMEOUT_MS;
  return esp_hosted_bt_host_stack_setup(&config);
}

esp_err_t present_frame(Display &display, const void *pixels, bike::Orientation orientation) {
  if (!pixels)
    return ESP_ERR_INVALID_ARG;
  ppa_srm_oper_config_t transform{};
  transform.in.buffer = pixels;
  transform.in.pic_w = transform.in.block_w = bike::display_width(orientation);
  transform.in.pic_h = transform.in.block_h = bike::display_height(orientation);
  transform.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  transform.out.buffer = framebuffers[back];
  transform.out.buffer_size = framebuffer_bytes;
  transform.out.pic_w = panel_width;
  transform.out.pic_h = panel_height;
  transform.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  // UI angles are clockwise; PPA's hardware angles are counterclockwise.
  switch (orientation) {
  case bike::Orientation::Landscape:
    transform.rotation_angle = PPA_SRM_ROTATION_ANGLE_270;
    break;
  case bike::Orientation::PortraitInverted:
    transform.rotation_angle = PPA_SRM_ROTATION_ANGLE_180;
    break;
  case bike::Orientation::LandscapeInverted:
    transform.rotation_angle = PPA_SRM_ROTATION_ANGLE_90;
    break;
  default:
    transform.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
  }
  transform.scale_x = transform.scale_y = 1.f;
  transform.mode = PPA_TRANS_MODE_BLOCKING;
  // PPA writes back its source and invalidates its destination before DMA.
  // Neither CPU nor DSI accesses the inactive output until the operation ends.
  ESP_RETURN_ON_ERROR(ppa_do_scale_rotate_mirror(rotator, &transform), "display", "PPA rotation failed");
  ESP_RETURN_ON_ERROR(esp_cache_msync(framebuffers[back], framebuffer_bytes, ESP_CACHE_MSYNC_FLAG_DIR_M2C), "display",
                      "PPA output synchronization failed");
  ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(display.panel, 0, 0, panel_width, panel_height, framebuffers[back]),
                      "display", "Framebuffer presentation failed");
  const uint32_t submitted = refresh_generation.load(std::memory_order_acquire);
  const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(500);
  // A refresh ISR can already have selected the previous buffer when presentation
  // changes the driver's index. Two subsequent edges guarantee it is no longer scanned.
  while (uint32_t(refresh_generation.load(std::memory_order_acquire) - submitted) < 2) {
    if (int32_t(deadline - xTaskGetTickCount()) <= 0 || xSemaphoreTake(refreshed, pdMS_TO_TICKS(50)) != pdTRUE) {
      if (int32_t(deadline - xTaskGetTickCount()) <= 0)
        return ESP_ERR_TIMEOUT;
    }
  }
  back ^= 1;
  return ESP_OK;
}

} // namespace board
