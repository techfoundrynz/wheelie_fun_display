#include "board.hpp"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_lcd_sh8601.h"
#include "esp_lcd_touch_ft5x06.h"
#include "freertos/task.h"
#include <algorithm>
#include <iterator>

namespace board
{
namespace
{

esp_lcd_panel_io_handle_t panel_io;

bool IRAM_ATTR dma_complete(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *ctx) {
  BaseType_t wake = pdFALSE;
  xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(ctx), &wake);
  return wake == pdTRUE;
}

const uint8_t zero[] = {0}, full[] = {0xff};
#if DISP_CO5300
// Waveshare 2.06 CO5300 QSPI initialization; compatible SH8601 transport.
const uint8_t spi_mode[] = {0x80}, control[] = {0x20};
const sh8601_lcd_init_cmd_t init_commands[] = {{0x11, zero, 0, 80}, {0xc4, spi_mode, 1, 0}, {0x53, control, 1, 1},
                                               {0x63, full, 1, 1},  {0x51, zero, 1, 1},     {0x29, zero, 0, 10}};
#else
  #error Select a supported panel controller in the hardware profile
#endif

} // namespace

Display init() {
#ifdef DISP_BL
  gpio_config_t enable{};
  enable.pin_bit_mask = 1ULL << DISP_BL;
  enable.mode = GPIO_MODE_OUTPUT;
  ESP_ERROR_CHECK(gpio_config(&enable));
  ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(DISP_BL), 1));
#endif
  i2c_master_bus_config_t bus{};
  bus.i2c_port = I2C_NUM_0;
  bus.sda_io_num = static_cast<gpio_num_t>(I2C_SDA);
  bus.scl_io_num = static_cast<gpio_num_t>(I2C_SCL);
  bus.clk_source = I2C_CLK_SRC_DEFAULT;
  bus.glitch_ignore_cnt = 7;
  bus.flags.enable_internal_pullup = true;
  i2c_master_bus_handle_t i2c;
  ESP_ERROR_CHECK(i2c_new_master_bus(&bus, &i2c));
  Display d;
  d.dma_done = xSemaphoreCreateBinary();
  configASSERT(d.dma_done);
  spi_bus_config_t spi{};
  spi.sclk_io_num = DISP_CLK;
  spi.data0_io_num = DISP_SDIO0;
  spi.data1_io_num = DISP_SDIO1;
  spi.data2_io_num = DISP_SDIO2;
  spi.data3_io_num = DISP_SDIO3;
  spi.max_transfer_sz = panel_width * tile_rows * 2;
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &spi, SPI_DMA_CH_AUTO));
  esp_lcd_panel_io_spi_config_t io{};
  io.cs_gpio_num = static_cast<gpio_num_t>(DISP_CS);
  io.dc_gpio_num = GPIO_NUM_NC;
  io.pclk_hz = DISP_QSPI_HZ;
  io.trans_queue_depth = 1;
  io.lcd_cmd_bits = 32;
  io.lcd_param_bits = 8;
  io.flags.quad_mode = true;
  io.on_color_trans_done = dma_complete;
  io.user_ctx = d.dma_done;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io, &panel_io));
  sh8601_vendor_config_t vendor{};
  vendor.init_cmds = init_commands;
  vendor.init_cmds_size = std::size(init_commands);
  vendor.flags.use_qspi_interface = true;
  esp_lcd_panel_dev_config_t panel{};
#ifdef DISP_RST
  panel.reset_gpio_num = static_cast<gpio_num_t>(DISP_RST);
#else
  panel.reset_gpio_num = GPIO_NUM_NC;
#endif
  panel.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  panel.bits_per_pixel = 16;
  panel.vendor_config = &vendor;
  ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(panel_io, &panel, &d.panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(d.panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(d.panel));
#ifdef PANEL_X_GAP
  ESP_ERROR_CHECK(esp_lcd_panel_set_gap(d.panel, PANEL_X_GAP, 0));
#endif
  esp_lcd_panel_io_i2c_config_t touch_io{};
  touch_io.dev_addr = ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS;
  touch_io.control_phase_bytes = 1;
  touch_io.lcd_cmd_bits = 8;
  touch_io.flags.disable_control_phase = true;
  touch_io.scl_speed_hz = 400'000;
  touch_io.transaction_timeout_ms = 20;
  esp_lcd_panel_io_handle_t tp_io;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c, &touch_io, &tp_io));
  esp_lcd_touch_config_t touch{};
  touch.x_max = panel_width;
  touch.y_max = panel_height;
#ifdef TP_RST
  touch.rst_gpio_num = static_cast<gpio_num_t>(TP_RST);
#else
  touch.rst_gpio_num = GPIO_NUM_NC;
#endif
  touch.int_gpio_num = static_cast<gpio_num_t>(TP_INT);
  ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft5x06(tp_io, &touch, &d.touch));
  brightness(70);
  return d;
}

void brightness(int percent) {
  const uint8_t value = std::clamp(percent, 0, 100) * 255 / 100;
  ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(panel_io, 0x02005100, &value, 1));
}

esp_err_t init_radio() {
  return ESP_OK;
}

bool radio_tx_ready() {
  return true;
}

esp_err_t draw_region(Display &display, int x0, int y0, int x1, int y1, const void *pixels) {
  return esp_lcd_panel_draw_bitmap(display.panel, x0, y0, x1, y1, pixels);
}

esp_err_t present(Display &) {
  return ESP_OK;
}

} // namespace board
