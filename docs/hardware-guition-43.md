# GUITION 4.3-inch ESP32-P4 display

The `guition_jc4880p443c` target is for the GUITION **JC4880P443C_I_W** / enclosed `_Y` board, described as JC4880P433 in the original article. It has a 480 × 800 ST7701S MIPI-DSI panel, GT911 touch, 16 MB flash, 32 MB PSRAM and an ESP32-C6 radio companion.

The supplied profile selects **ESP32-P4 revisions below 3.0**, including the original v1.3 engineering-sample board. ESP-IDF treats those chips and production v3.x chips as incompatible targets. Do not use this profile on a v3.x board; check the boot log or vendor revision before installation.

## Build

This board is included in the supported `default_envs`. To build it on its own:

```sh
pio run -e guition_jc4880p443c
```

The pinned Slint release supplies matching compiler and MCU runtime assets for ESP32-S3 and ESP32-P4. Firmware builds download and verify those assets; a local Rust runtime bootstrap is not required.

Both profiles use the common PlatformIO Espressif32 8.0.0 and ESP-IDF 6.1 pins. The P4 profile selects pre-v3 silicon and its qualified 360 MHz CPU setting. Shared settings live in `sdkconfig.defaults`; architecture-specific values live in `config/`. Managed component versions are pinned once in `firmware/main/idf_component.yml`; each chip has its own generated dependency lock.

For a published release, the [browser installer](https://techfoundrynz.github.io/wheelie_fun_display/install/) flashes the complete P4 image. It does not replace the C6 companion firmware; prepare that separately as described below.

## Display and touch

| Signal | Configuration |
| --- | --- |
| Native panel | 480 × 800, RGB565, two DSI lanes |
| DSI PHY | LDO channel 3 at 2.5 V; 500 Mbit/s per lane |
| DPI pixel clock | 34 MHz |
| Horizontal timing | sync 12, back porch 42, front porch 42 |
| Vertical timing | sync 2, back porch 8, front porch 166 |
| Panel reset | GPIO5 |
| Backlight | GPIO23, LEDC brightness control |
| Touch I2C | SDA GPIO7, SCL GPIO8, 400 kHz |
| Touch reset | GPIO3 |
| Touch addressing | Probe 0x5D, then 0x14; polling without an interrupt pin |

Slint renders into a persistent, full-screen RGB565 canvas in PSRAM. The P4 pixel processing accelerator (PPA) rotates each completed frame into the inactive native DSI framebuffer; two native framebuffers keep scanout separate from rendering. Presentation waits for refresh completion before reusing the old front buffer. This uses three full-screen allocations (2.30 MB total), with no internal-RAM scanline buffers. All four display orientations use PPA rotation and the matching native-coordinate touch transforms. PSRAM runs at 200 MHz. The supplied profile starts at 90° (800 × 480 landscape). `DISPLAY_ROTATION` sets the initial angle in the hardware profile; runtime selection is saved in NVS.

## Large-screen layout

Landscape Ride uses a full-width battery gauge at the top, then a larger speed dial on the left and trip and odometer above the four telemetry cards on the right. In portrait, telemetry cards and distance readings are larger. Stats groups its overview into three columns when the screen is wide; controller and BMS details remain scrollable.

![GUITION landscape Ride layout](images/ride-guition-landscape.png)

![GUITION portrait Ride layout](images/ride-guition-portrait.png)

![GUITION landscape Stats overview](images/stats-guition-landscape.png)

These previews are generated from the current shared UI with illustrative connected values during the Pages build. In the [browser simulator](../simulator/), select **GUITION 4.3-inch** under **Display model**, then use Rotate to explore all four orientations.

## ESP32-C6 radio firmware

The P4 has no built-in radio. Wi-Fi and both BLE roles use the C6 through ESP-Hosted: standard `esp_wifi` calls are supplied by `esp_wifi_remote`, and the P4 NimBLE host communicates with the C6 Bluetooth controller through the Hosted HCI tunnel.

| P4-to-C6 signal | P4 GPIO |
| --- | --- |
| SDIO slot | 1, four-bit bus, 25 MHz |
| CLK / CMD | 18 / 19 |
| D0 / D1 / D2 / D3 | 14 / 15 / 16 / 17 |
| Companion reset | 54 |

The target pins `esp_hosted` 3.1.0 and `esp_wifi_remote` 1.6.5. The C6 must run compatible ESP-Hosted firmware with **SDIO transport, Wi-Fi and Bluetooth controller HCI** enabled. An arbitrary vendor Wi-Fi demo is not sufficient. The P4 application image does not overwrite or automatically replace the C6 firmware. Hosted 3.x MCU hosts also support compatible older Hosted streaming/1.x/2.x companion images, so an existing image may work if those radio features are enabled. A matching 3.1.0 companion is recommended; verify its firmware and configuration before replacing it.

Use the matching ESP-Hosted 3.1.0 `examples/bluetooth/esp_hosted_nimble/bleprph_wifi_coex/cp` project when preparing the companion. Hosted 3.x replaces the old `slave/` project with per-example `cp/` projects. This coexistence profile enables Wi-Fi, controller-only Bluetooth and HCI over the Hosted transport. Build it for `esp32c6` using ESP-IDF 6.1, and verify SDIO and the board's C6 wiring in menuconfig before building. Flash it only through the C6's documented programming connection, not the P4 application port. Follow the board vendor's instructions for C6 boot mode and connector access.

After provisioning, test a normal BLE vehicle connection, encrypted pairing, phone proxy connection, Wi-Fi and OTA together. Verify the display stays responsive during flash writes and that no DSI underruns appear in the serial log. Those checks require this physical board and a compatible C6 image.

## Scope

Camera, audio, microSD, battery telemetry and RS-485 are not initialized by this application. Optional vehicle CAN/UART pins can be configured in the hardware profile after wiring the required interface; they are not guessed from expansion-header availability. The radio SDIO bus must remain reserved for the C6.

## Sources and notices

- [GUITION product specifications](https://www.guition.com/icms/upload/fb081940d6fc11f09850077a33e1404f/file/productmanager-productfile/5dfbe9a7c0fc44869270528bd2411b1e/Directory/JC4880P443C_I_W%20Specifications-EN-V1.0_1776241869964.pdf).
- [ESP-IDF P4 revision selection](https://github.com/espressif/esp-idf/blob/v6.1/components/esp_hw_support/port/esp32p4/Kconfig.hw_support).
- [Espressif hosted NimBLE/VHCI example](https://github.com/espressif/esp-hosted-mcu/tree/v3.1.0/examples/bluetooth/esp_hosted_nimble/bleprph_wifi_coex).
- [Espressif Hosted migration and companion compatibility](https://github.com/espressif/esp-hosted-mcu/blob/v3.1.0/docs/migration.md).
- [Espressif hosted companion project](https://github.com/espressif/esp-hosted-mcu/tree/v3.1.0/examples/bluetooth/esp_hosted_nimble/bleprph_wifi_coex/cp).
- [MIT-licensed board initialization reference](https://github.com/ultramcu/guition-jc4880p4-bsp/tree/324970bade0d1f4e52880fe8016580368bc1e06e). Its panel register table is retained with the original copyright and MIT notice in `firmware/main/boards/`; the display lifecycle and rendering integration are implemented in this project.
