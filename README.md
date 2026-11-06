# Wheelie Fun Display

ESP-IDF + PlatformIO + Slint firmware for a VESC bike touch display, with Wheelie Fun Bike, wheel-e and generic VESC integrations. Vehicle protocols and features are selected independently of the shared display and transport code.

Start with [Getting started](GETTING-STARTED.MD), [the rider guide](docs/user-guide.md), or [building and installing](docs/building.md). The GitHub Pages artifact combines the styled documentation, [browser simulator](https://techfoundrynz.github.io/wheelie_fun_display/simulator/) and [USB firmware installer](https://techfoundrynz.github.io/wheelie_fun_display/install/).

![Dashboard](https://techfoundrynz.github.io/wheelie_fun_display/assets/images/ride-landscape.png)

## Supported targets

The hardware profile inherits `common` in [platformio.ini](platformio.ini). `default_envs` is the supported-target list used by CI and release packaging.

| Environment | Display | Hardware |
|---|---|---|
| `waveshare_esp32s3_206` | 410 × 502 | Waveshare AMOLED 2.06, CO5300 / FT3168 |
| `guition_jc4880p443c` | 480 × 800 | GUITION JC4880P443C, ST7701S / GT911, P4 + C6 |

See the [Waveshare board guide](docs/hardware-waveshare-206.md) for wiring, orientation and USB recovery. The GUITION profile requires an ESP32-P4 below revision 3.0 and compatible C6 radio firmware. Check [board setup](docs/hardware-guition-43.md) before installation.

Speed units and temperature units are independent saved settings. Angles use the degree symbol.

Resolution, corner-safe inset, flash size, pins, driver selection and power controls come from each PlatformIO profile using capability-based configuration. There are no board-name switches in the shared firmware. Hardware identity and release filenames derive from the environment name. The firmware version comes from `[common].custom_firmware_version`.

## Features

- The supplied Wheelie theme: black surfaces, grey cards, cyan accents, gold gauge, Roboto and the official brand wordmark.
- Responsive ride dashboard, bike picker and feature-gated ride-response/speed-limit controls. Bike writes require fresh stationary telemetry and zero controller fault. Tuning writes are reported as queued; bike locks additionally wait for matching telemetry state.
- Configurable corner-safe spacing for rounded panels, applied in all four orientations and on the keyboard/confirmation screens.
- Vehicle integrations for Wheelie Fun Bike, wheel-e and generic VESC. Bike > Vehicle integration selects Auto, WFB, wheel-e or VESC before connecting; the verified peer and integration are saved together in NVS. Matching names sort first without hiding other UART devices. Branding and available commands follow the integration.
- Bike menu PIN setup/change/removal, bike/screen locking and a locked-screen timeout (15/30/60/120 seconds). PINs are salted hashes in NVS. Tap wakes to configured brightness and consumes that contact; a PIN is still required. See [locking](docs/locking.md) and [integrations](docs/integrations.md).
- Optional VESC BLE proxy, off by default: connect the phone app to this display's editable BLE name (default `WB Disp`). BLE/UART preserve the VESC stream; CAN exposes a generic adapter for the phone's own CAN discovery and forwarding. The phone reserves the VESC link while connected; dashboard commands, polling and firmware maintenance pause. Recognized replies can still update the display passively.
- 250 ms opaque page slides with ease-out motion. Header and bottom navigation stay fixed; taps are blocked during the slide, and only the incoming/outgoing pages are retained. The unselected Settings cog uses subdued grey.
- Swipe horizontally between Ride, Tune and Bike. Pages follow the finger and snap on release; short or reversed drags return to the current tab. Vertical drags scroll the Tune content and Bike list.
- A vertically scrolling Settings page with reusable pages, rows, steppers and diagnostic cards.
- The Lucide cog in the top-right header opens Settings; the bottom navigation contains Ride, Tune and Bike. Runtime portrait/landscape selection under **Settings > Orientation**, saved across restarts. Display rotation and inverse touch mapping change together between completed frames. Landscape uses a side-by-side gauge and telemetry layout.
- Wi-Fi scanning, hidden SSIDs, saved connection/forget, and a reusable keyboard with uppercase, numbers, symbols, space, delete and password reveal.
- GitHub OTA from `techfoundrynz/wheelie_fun_display`: certificate-verified HTTPS, restricted redirect hosts, GitHub SHA256 digest, board-specific application identity, two application slots and rollback support. Bluetooth pauses during install and resumes on failure/cancellation. Restart is explicit after verification.
- About page with version/build/hash/device identity and live diagnostics: memory, stack, uptime, reset reason, boot partition, networking and protocol errors.

## Build and flash

For a published release, use the [browser installer](https://techfoundrynz.github.io/wheelie_fun_display/install/) and choose your exact display. Complete USB installation may reset saved settings; use on-device OTA for routine updates.

To build from source, install PlatformIO Core (validated with 6.2.0):

```powershell
pio run
python scripts/check_sdkconfig.py --all
```

Build or flash an individual board:

```powershell
pio run -e waveshare_esp32s3_206
pio run -e waveshare_esp32s3_206 -t upload --upload-port COM9
pio device monitor --port COM9
```

The firmware pins Espressif platform 8.0.0 / ESP-IDF 6.1 for both targets, with a pinned Slint compiler/runtime pair. The compiler and runtime are consumed together with release-asset checksum verification; no Rust toolchain is needed for firmware builds. Application outputs are `.pio/build/<environment>/firmware.bin`.

The OTA partition table replaces the original factory-only layout. **Install this version once over USB with its matching bootloader, partition table and OTA initialization data.** Future application images can then use OTA. App slots are 7 MiB each; NVS remains at its existing address. `sdkconfig.defaults` supplies shared settings; flash size comes from the PlatformIO profile. The build generates combined defaults and regenerates SDK configuration when either changes.

## Releases and OTA

```powershell
python -m pip install -r requirements-release.txt
python scripts/package_release.py
```

This stages each supported target as the OTA application `<environment>.bin` and complete USB image `<environment>-factory.bin`, plus a SHA256 manifest in `.pio/releases/`. Packaging also creates `licenses.zip` with project, asset and component notices, and `source-references.json` with the application commit and pinned Slint source. The release workflow publishes these alongside the firmware, then rebuilds Pages so its installer serves verified, version-matched images from the same origin. Publish those application images as assets of a public GitHub release in `techfoundrynz/wheelie_fun_display`. GitHub supplies the asset `digest` used for OTA validation. A release missing the matching asset or digest is rejected. There is currently no public release at that endpoint; the device reports this without altering firmware.

The release workflow builds all `default_envs`; a tag version is injected through `WFD_FIRMWARE_VERSION`. Private-repository authentication, prerelease channels and signed firmware/secure boot are not implemented. TLS and SHA256 establish transport/image integrity, not a separate signing authority.

On-device flow: **Settings > Wi-Fi**, choose a network and enter its password, then **Settings > Firmware updates**. Installation requires explicit confirmation. Failed or cancelled downloads leave the selected boot image unchanged. A newly booted update is accepted only after initialization and ten seconds of successful UI-loop execution; earlier resets retain bootloader rollback.

## Desktop simulator and verification

```powershell
cargo run --locked --manifest-path simulator/Cargo.toml
cargo run --locked --manifest-path simulator/Cargo.toml -- 502 410
cargo run --locked --manifest-path simulator/Cargo.toml -- --smoke-test
```

The simulator uses the same Slint screens. Add `--wired` to expose simulated CAN/UART choices. Bike/Wi-Fi data and connections are simulated; it never controls hardware or downloads firmware. Native dimensions can be supplied on the command line. The simulator's version is read from PlatformIO configuration too.

```sh
cmake -S tests -B .cache/host-tests -G Ninja -DBIKE_SANITIZERS=ON
cmake --build .cache/host-tests
ctest --test-dir .cache/host-tests --output-on-failure
```

Portable tests cover actual framing/telemetry sources, all packet split points, the 256-byte boundary, corruption, fuzzing, stale/fault/motion interlocks, update URL/digest policy and exhaustive touch transforms for the native display in all four orientations. Linux supports address/undefined-behavior sanitizers. On Windows, omit that option and use an installed compiler generator.

Formatting follows the repository `.clang-format`; CI pins clang-format 23.1.0. `.gitattributes` normalizes source line endings and preserves binary assets.

## Limits and documentation

Persisted vendor tuning, unsupported wheelie commands and feature/horn encodings are intentionally absent until signature/order/scaling uncertainties in the supplied protocol notes are resolved. Draft controls are not readbacks. BLE is the default transport; target-defined CAN or UART pin pairs expose wired choices in Settings. Upstream BLE pairing uses its own PIN prompt and bond. The proxy uses the bike lock PIN only and remains independent of the display's main-controller route. Real-controller pairing, PIN changes, wired operation and phone interoperability require hardware acceptance testing; see [open work](TODO.md).

See [connections](docs/connections.md), [PINs and locking](docs/pins-and-locking.md), [maintenance](docs/maintenance.md), [contributing](CONTRIBUTING.md), [architecture](docs/architecture.md), [performance decisions](docs/performance.md), [open work](TODO.md), and [third-party notices](THIRD_PARTY.md). Performance claims are limited to the measurements recorded for this display.

## Documentation site

The [GitHub Pages site](https://techfoundrynz.github.io/wheelie_fun_display/) combines rider guides, developer notes and the browser simulator. Its workflow builds one Pages artifact; simulator services are mocked and do not operate a vehicle.

```sh
python -m pip install -r requirements-docs.txt -r simulator/requirements-browser-test.txt
python -m playwright install chromium
python scripts/build_web_simulator.py --output .cache/pages/site/simulator
python scripts/generate_docs_screenshots.py --site .cache/pages/site
python scripts/build_docs.py --output .cache/pages/site
python scripts/build_flash_site.py --site .cache/pages/site --github-release
python -m http.server --directory .cache/pages/site
```

The documentation builder preserves the generated simulator and installer when rerun. The installer reports an unavailable state before the first public release. Use `--release-dir .pio/releases` instead of `--github-release` for an explicitly labelled local build preview. Source pages start at [GETTING-STARTED.MD](GETTING-STARTED.MD) and [CONTRIBUTING.md](CONTRIBUTING.md). Build and browser checks are described in [maintenance](docs/maintenance.md).

## License

This project's original firmware, UI, scripts and tests are licensed under **GPL-3.0-only**; see [LICENSE](LICENSE). Third-party components, Slint and fonts retain their respective notices and terms in [THIRD_PARTY.md](THIRD_PARTY.md).

## VS Code setup

```powershell
python scripts/sync_slint_lsp.py
```

The pinned fork LSP (which includes live preview) downloads into ignored `.slint/<tag>/`, and the machine-specific absolute `slint.lspBinaryPath` is written to ignored `.vscode/settings.json`. The tracked shared settings and folder-open task bootstrap other checkouts. Existing local settings are preserved. Reload the VS Code window after first setup. `--local`, `--restore`, and `--print-setting` remain available. The standalone desktop simulator is built separately from the same pinned fork.
