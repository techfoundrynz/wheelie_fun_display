# Third-party references and notices

- **VESC Express:** [upstream source](https://github.com/vedderb/vesc_express), GNU GPL version 3 or later. Copyright 2022–2024 Benjamin Vedder and 2023 Rasmus Söderhielm, as recorded in the consulted command and CAN transport files. Its adapter identity and firmware-upload command layouts informed this project's independently implemented CAN/BLE adapter and ESP-IDF OTA receiver. The upload receiver adds bounded parsing, transfer ownership, duplicate verification, CRC checks and board validation. Upstream license: [GPL-3.0](https://github.com/vedderb/vesc_express/blob/main/LICENSE). This repository's GPL-3.0-only license remains applicable to its original implementation.

- **Slint** runtime/compiler: [techfoundrynz/slint mcu-v1.19.5](https://github.com/techfoundrynz/slint/releases/tag/mcu-v1.19.5), commit `8076b03255964faf155c06a38eac6d797d0d2140`. Original notices and license options are preserved in `firmware/components/slint_runtime/`. Embedded use requires the applicable GPLv3 or commercial Slint terms; the desktop royalty-free option excludes embedded systems. The original project source is GPL-3.0-only; third-party notices and font licenses remain applicable.
- **VESC wire verification:** [upstream commands.c](https://github.com/vedderb/bldc/blob/master/comm/commands.c), `COMM_GET_VALUES_SETUP` speed uses a signed integer scaled by 1000. [VESC Tool BLE implementation](https://github.com/vedderb/vesc_tool/blob/master/bleuart.cpp) confirms NUS UUIDs. These were consulted as protocol references; their implementation code is not copied.
- **Fonts:** Roboto and Sonsie One from [google/fonts](https://github.com/google/fonts), under SIL Open Font License 1.1. Font files and respective OFL notices are in `ui/fonts/`.
- The `docs/wheelie-*.md` files contain WFB protocol and visual design references. Entries marked inferred or unconfirmed require device verification.

- The 2.06-inch CO5300 commands, direct GPIO reset, 22-pixel column offset and FT3168 wiring were checked against the [Waveshare reference](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06). Hardware uses 32 MB flash and 8 MB octal PSRAM.

- The Slint editor setup script stages the pinned language server and maintains shared editor/task conventions. The fork's LSP supplies language support and the live preview runtime.

- Firmware control icons are from [Lucide](https://lucide.dev), with the [ISC/MIT license notices](ui/assets/LUCIDE-LICENSE) preserved. The website minibike in `docs/site/bike.svg` is original project artwork under this repository's GPL-3.0-only license.

- **ESP Web Tools** 10.4.0 powers the browser firmware installer under Apache-2.0. Its pinned distribution and runtime dependency versions are recorded in `docs/flasher/vendor-lock.json`. The site hosts the unmodified JavaScript and complete bundled license texts under `install/vendor/esp-web-tools/THIRD_PARTY_NOTICES.txt`. [Project source](https://github.com/esphome/esp-web-tools/tree/fab2ba48253d19fd29a15af57bec771d34978b62).

- **GUITION ST7701S initialization table:** adapted from [ultramcu/guition-jc4880p4-bsp](https://github.com/ultramcu/guition-jc4880p4-bsp/tree/324970bade0d1f4e52880fe8016580368bc1e06e), under MIT. Original copyright and license are preserved in `firmware/main/boards/`.

- **Documentation application:** React and the TanStack libraries use the MIT license. Exact runtime and build-tool versions are pinned in [pnpm-lock.yaml](pnpm-lock.yaml). The generated site includes [web dependency license texts](assets/web-dependency-notices.txt), including copyright notices for React, TanStack and their dependencies. Some listed packages are build tools rather than browser runtime code.
