# Waveshare ESP32-S3 AMOLED 2.06-inch display

Use the `waveshare_esp32s3_206` target for the **Waveshare ESP32-S3 Touch AMOLED 2.06**. Its native resolution is 410 × 502, with a CO5300 AMOLED panel, FT3168 touch, 32 MB flash and 8 MB octal PSRAM. Wi-Fi and Bluetooth are built into the ESP32-S3; no radio companion needs provisioning.

![Software preview of the portrait Ride dashboard](images/ride-portrait.png)

The picture is generated from the current shared UI during the documentation build. Its connected values are illustrative.

## Install the firmware

1. Open the [browser firmware installer](https://techfoundrynz.github.io/wheelie_fun_display/install/) in a supported desktop browser.
2. Choose **Waveshare ESP32-S3 AMOLED 2.06**. Other Waveshare sizes use different firmware.
3. Connect a USB data cable, select the display’s port and follow the prompts.
4. After startup, check touch and orientation, then follow [getting started](../GETTING-STARTED.MD) to connect your bike.

A complete USB installation can reset saved settings. For ordinary updates, use **Settings > Firmware updates**; see [updates and maintenance](maintenance.md).

## Display, touch and orientation

Tap **Settings > Orientation** to cycle through 0°, 90°, 180° and 270°. The screen and touch controls rotate together, and your choice is saved.

The layout leaves room for the panel’s rounded corners. Check that you can reach the cog, bottom tabs and keyboard in your chosen mounting orientation.

## Board wiring

These are the shipped profile's internal connections, not suggested pins for external accessories.

| Function | GPIO / value |
|---|---|
| I2C SDA / SCL | 15 / 14 |
| QSPI clock / chip select | 11 / 12 |
| QSPI data 0–3 | 4 / 5 / 6 / 7 |
| Display reset / enable | 8 / 13 |
| Touch reset / interrupt | 9 / 38 |
| Panel column offset | 22 pixels |

BLE is the default vehicle transport. CAN and UART appear in Settings only after their pin pairs are configured in the target profile. Use a firmware build configured for your wiring and provide an external CAN transceiver if using CAN; see [connections](connections.md).

## Case and handlebar mount

![Portrait and landscape Waveshare 2.06 cases with handlebar clamps](images/waveshare-206-mounted.png)

The [case and mount guide](../cad/waveshare-206/README.md) has part downloads, dimensions, a fastener list and assembly views for a nominal **22.225 mm (7/8-inch)** handlebar.

- Rotate the same casing on its square mounting pattern for portrait or landscape, then match **Settings > Orientation**.
- Fit the [cable grommet](../cad/waveshare-206/README.md#cable-grommet-installation) to the 6 mm cable jacket before attaching oversized connectors.
- Check your real display and printed parts against the model before final assembly.

## USB recovery and checks

If the USB port is missing:

1. Use a known USB data cable.
2. Hold **BOOT**, tap **RESET**, then release **BOOT**.
3. Select the newly detected port in the installer. With PlatformIO, use `pio device list` to identify it.

After installation, confirm the dashboard, cog, Settings scrolling and touch at all four orientations. The connected S3 board has passed SDK 6.1 startup and rendering checks; real-bike, phone-proxy and OTA acceptance remain listed in [TODO](../TODO.md). Occasional touch I2C read errors are also tracked there; save the serial log when diagnosing a touch issue.

See [troubleshooting](troubleshooting.md), [performance configuration](performance.md), the [Waveshare product page](https://www.waveshare.com/esp32-s3-touch-amoled-2.06.htm) and [vendor examples](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06).

## Build and driver reference

To build from source, follow [building and installing](building.md) with environment `waveshare_esp32s3_206`. Release application images are `waveshare_esp32s3_206.bin`; full USB images are `waveshare_esp32s3_206-factory.bin`.

The profile uses a 240 MHz CPU, 80 MHz flash/octal PSRAM and 80 MHz panel QSPI. CO5300 uses an SH8601-compatible transport with a 22-pixel column offset; FT3168 uses the compatible FT5x06 driver. Display and touch resets use GPIOs directly. Safe insets are 38 pixels horizontally and 24 vertically. See [performance configuration](performance.md) for rendering details and [platformio.ini](../platformio.ini) for the profile.

Transitions stream cached page images through internal DMA tiles; scrolling renders dirty spans directly. Rotation repaints the full panel to clear old controls from the margins.
