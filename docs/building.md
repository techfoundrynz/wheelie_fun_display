# Building and installing

The firmware uses ESP-IDF through PlatformIO and a pinned Slint compiler/runtime pair. Firmware builds use matching prebuilt MCU compiler/runtime assets. Desktop/browser simulators need Rust. Both supported targets share the pinned platform and SDK in the common PlatformIO configuration.

## Supported hardware

| Environment | Native resolution | Hardware |
| --- | --- | --- |
| `waveshare_esp32s3_206` | 410 × 502 | Waveshare ESP32-S3 Touch AMOLED 2.06, CO5300 / FT3168, 32 MB flash |
| `guition_jc4880p443c` | 480 × 800 | GUITION JC4880P443C, ST7701S / GT911, 16 MB flash, P4 + C6 |

See the [Waveshare S3 2.06-inch setup guide](hardware-waveshare-206.md) for its board wiring and USB recovery. The [GUITION P4 4.3-inch target](hardware-guition-43.md) requires an ESP32-P4 below revision 3.0 and compatible C6 companion firmware. Both targets are included in `default_envs`.

[PlatformIO configuration](../platformio.ini) is the source of truth. Do not flash a different Waveshare size or revision with this target merely because it also uses an ESP32-S3.

## Prepare the tools

Install Python and PlatformIO Core, then use a terminal at the repository root. CI pins PlatformIO Core 6.2.0.

```sh
python -m pip install platformio==6.2.0
pio run
python scripts/check_sdkconfig.py --all
```

`pio run` builds all supported `default_envs`. The project pins Espressif platform 8.0.0 / ESP-IDF 6.1 in the common environment and a matching pinned Slint compiler/runtime pair. The Slint compiler and runtime are downloaded together with checksum verification.

Slint generation uses a content fingerprint of the UI/resources, compiler, font-preparation script and font plan/options. Unchanged inputs reuse the matching generated header/source pair: glyph ordering can vary between compiler runs, so unnecessary regeneration otherwise causes spurious changes and recompilation. Missing outputs or changed inputs regenerate the pair; the fingerprint is committed only after generation and font preparation succeed. Build checks cover preservation of both output hashes for unchanged inputs.

Each application image is `.pio/build/<environment>/firmware.bin`. The ESP project identity is derived as `wfd_<hardware-id>`; release filenames use the PlatformIO environment name.

## First USB installation

For a published release, open the [browser installer](https://techfoundrynz.github.io/wheelie_fun_display/install/), choose the exact display and connect with a USB data cable. The installer checks the chip family and flashes a complete matching image. It can reset saved settings; use OTA for normal updates. The P4 installer does not provision the C6 companion.

When uploading a local build with PlatformIO, discover the port rather than copying someone else's COM number:

```sh
pio device list
pio run -e waveshare_esp32s3_206 -t upload --upload-port YOUR_PORT
pio device monitor --port YOUR_PORT
```

Replace `YOUR_PORT` with the detected port, such as `COM9` on Windows or `/dev/ttyACM0` on Linux. If the board is not detected, use a known USB data cable and enter the board's download mode with BOOT/RESET as described by its manufacturer.

The first installation must include the matching bootloader, partition table, OTA initialization data and application. PlatformIO upload uses the configured target layout. Do not upload only an application image to a factory partition layout. After installation, verify the dashboard, touch, all four orientations and the boot log before connecting a bike.

## Configure an optional wired interface

BLE is the default. CAN and UART are available only when their pin pairs are defined in the target's `build_flags`. Choose unused, exposed GPIOs from the actual board schematic; the repository does not guess wiring.

| Build flag | Default / requirement |
| --- | --- |
| `CAN_TX`, `CAN_RX` | Both required to enable CAN. Connect through an external transceiver. |
| `CAN_BITRATE` | `500000`; supported values 125000, 250000, 500000 and 1000000. |
| `CAN_LOCAL_ID` | `254`; must be unique on the vehicle CAN bus. |
| `UART_TX`, `UART_RX` | Both required to enable UART. |
| `UART_BAUD` | `115200`; must match the vehicle. |
| `BIKE_UART_PORT` | `1`; ports 1 and 2 are supported, not the console port 0. |

Add the chosen `-D` definitions to the hardware profile and rebuild. The interfaces then appear in Settings. If a saved interface is unavailable in the current build, the display falls back to BLE. The main motor controller is found through discovery rather than a target CAN-ID build flag.

## Editor and live preview

```sh
python scripts/sync_slint_lsp.py
```

This downloads the pinned language server/live preview and writes the machine-specific path to ignored VS Code settings. Reload the editor after initial setup. The tracked shared settings remain portable; do not commit generated absolute tool paths.

## Desktop simulator

Install a Rust toolchain, then run:

```sh
cargo run --locked --manifest-path simulator/Cargo.toml
cargo run --locked --manifest-path simulator/Cargo.toml -- 502 410
cargo run --locked --manifest-path simulator/Cargo.toml -- --wired
cargo run --locked --manifest-path simulator/Cargo.toml -- --smoke-test
```

The `--wired` demo exposes simulated CAN/UART settings. Simulators use fake bike/Wi-Fi data and never drive hardware or download firmware. The [browser simulator](../simulator/) runs the actual shared Slint UI and software renderer in WebAssembly. Application services remain browser-only mocks: connections, Wi-Fi, OTA, PIN validation and pairing do not reach hardware. Demo PINs/preferences reset on reload; the demo upstream pairing PIN is `123456`. Serve the generated site over HTTP rather than opening it with `file://`.

## Host checks

On Linux, or in WSL with a C++ compiler, CMake and Ninja:

```sh
cmake -S tests -B .cache/host-tests -G Ninja -DBIKE_SANITIZERS=ON
cmake --build .cache/host-tests
ctest --test-dir .cache/host-tests --output-on-failure
```

On Windows, use an installed compiler generator and omit sanitizer flags if unsupported. Headless UI checks use the shared Slint screens:

```sh
cargo run --locked --manifest-path simulator/Cargo.toml --bin transition-checks -- 410 502
cargo run --locked --manifest-path simulator/Cargo.toml --bin transition-checks -- 502 410
```

See [contributing](../CONTRIBUTING.md) for choosing checks appropriate to a change and [release maintenance](site-maintenance.md) for release packaging.
