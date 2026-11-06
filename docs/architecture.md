# Architecture

The application uses ESP-IDF, PlatformIO and a pinned Slint runtime. Board integration, standard VESC BLE transport, protocol handling, state management and UI are implemented in this repository.

| Layer | Location | Responsibility |
|---|---|---|
| UI | `ui/` | Declarative responsive layout, theme, input and confirmation |
| Application | `firmware/main/main.cpp` | Polling, session tracking, state freshness, settings authorization and UI projection |
| Protocol | `firmware/components/bike/` | Shared VESC framing, CRC, vehicle integrations and transactional telemetry decoding |
| BLE | `firmware/main/ble.cpp` | NimBLE central lifecycle, GATT discovery, bounded queues and ordered transmission |
| Board | `firmware/main/board.cpp`, `board_guition_p4.cpp` | Profile-based power/reset, QSPI or MIPI-DSI display, touch and brightness |
| Renderer | `firmware/main/slint_backend.cpp`, `slint_renderer_*.cpp` | Single-task Slint ownership, touch dispatch and target-specific rendering/presentation |
| Runtime packaging | `firmware/components/slint_runtime/` | Matching prebuilt compiler, headers and MCU library |

## Data flow and ownership

NimBLE runs its host task. Notification callbacks copy bounded fragments into a 12-entry queue. A dedicated transmission task serializes VESC frame chunks, using negotiated ATT MTU minus three bytes and a bounded retry on controller-memory exhaustion. Queue overflow or a failed partial transmission disconnects rather than continuing a damaged byte stream.

The application/UI task consumes receive fragments, checks connection generation, feeds the VESC decoder, updates telemetry and dispatches Slint input/rendering. UI properties are never touched from BLE callbacks. A mutex protects transport snapshots; bounded queues carry cross-task work. Old-generation RX and TX work is discarded after reconnect. Decoder and telemetry state are reset whenever the connection is not ready.

The application requests `COMM_GET_VALUES_SETUP` and the selected integration's package telemetry at 5 Hz. WFB uses `GET_ALL_DATA(mode=1)`; wheel-e uses `REALTIME_DATA` with state, pitch and roll masks. Generic VESC polls only standard telemetry. Poll cadence is independent of the untrusted reported loop time. Setup values provide speed in m/s (signed integer / 1000), battery and temperatures; package replies supply attitude and state. Unknown responses and replies for another package are ignored. Short/malformed replies cannot partially overwrite an existing sample. Setup and package freshness expire independently after 1500 ms.

## CAN interrupt ownership

The CAN adapter uses ESP-IDF's handle-based TWAI driver. Eight persistent transmit slots retain frame descriptors and payloads until completion; a bounded internal queue copies received frames out of the interrupt context. Completion and fault queues transfer ownership to the wired worker, which alone changes slot state and runs protocol/discovery logic. ISR callbacks use FreeRTOS `FromISR` operations, avoiding compiler atomic helpers on Xtensa builds that disable hardware atomics.

Keep interrupt callback definitions out of line with `IRAM_ATTR`; inline class-body callbacks can produce unreachable COMDAT literal pools on Xtensa. The driver tests compile the production adapter against controlled SDK fakes to exercise early completion, buffer lifetime, queue overflow, failure and cleanup. Hardware CAN acceptance remains separate; see [transports](transports.md).

## Changes to bike settings

The rider chooses a draft, opens a confirmation, then explicitly applies it. The application rechecks the session, a 30-second confirmation expiry, fresh standard and selected-package telemetry, controller fault zero, and absolute speed below 0.2 m/s immediately before enqueueing a command. Pending confirmations are cancelled on disconnect. There is no automatic tune or mode write at startup/reconnect, and no motor-drive command path.

This is a display-side interlock, not a motor safety system; the controller remains responsible for accepting settings appropriately. BLE write-without-response and the documented live commands provide no proof of application. The UI distinguishes queueing from confirmation by the controller.

## Display and memory

2.06 wiring: I2C SDA15/SCL14; QSPI CS12/CLK11/D0-D3 4-7; display reset GPIO8 and enable GPIO13; touch reset GPIO9 and interrupt GPIO38. CO5300 uses the managed SH8601 QSPI transport with a 22-pixel column gap. FT3168 uses the compatible managed FT5x06 driver. No I/O expander is used.

On ESP32-S3, the pinned renderer aligns dirty regions to 2×2 pixels **before** rasterization. Three internal DMA-capable buffers, each at most native-panel-width × 10 RGB565 pixels, accumulate interleaved dirty rectangles. The renderer outputs panel byte order directly. Rasterization overlaps the preceding DMA transfer in a different buffer. Narrow spans pack additional even rows into the existing byte capacity. Each transfer has a one-second completion timeout; the buffer is not reused until its ISR completion semaphore is received. Brightness and rendering execute on the same task. This S3 path uses no full-screen framebuffer or PSRAM rasterization.

On ESP32-P4, Slint renders into a full-screen PSRAM canvas. PPA rotates/copies completed regions into the inactive native DSI framebuffer; a pair of scanout buffers separates rendering from display refresh. Cache synchronization and refresh completion protect buffer reuse. See [P4 hardware and memory details](hardware-guition-43.md).

NVS stores display preferences, the selected BLE address and Wi-Fi credentials. BLE `forget` clears its peer; Wi-Fi forget clears the saved network. Credentials use the ordinary ESP-IDF NVS store; bonding, encrypted credentials and remote control services are not implemented.


## Runtime orientation

Settings cycles through 0°, 90°, 180° and 270° while running, and the preference is saved. Application requests wait until the current input dispatch and frame presentation finish. Logical dimensions swap at 90° and 270°; the S3 software renderer or P4 PPA applies the selected rotation, and touch uses its inverse transform. Panel transfers and touch-controller configuration retain native coordinates.

For native dimensions W × H, touch uses these native-to-logical mappings:

| Rotation | Logical position from native (x, y) |
|---|---|
| 0° | (x, y) |
| 90° | (y, W − 1 − x) |
| 180° | (W − 1 − x, H − 1 − y) |
| 270° | (H − 1 − y, x) |

Host tests exhaust every pixel for all four rotations at both 410 × 502 and 480 × 800, and compare touch mapping against actual Slint-rendered fixtures. Changing orientation reuses the target's existing panel buffers.

## Hardware profiles and build metadata

Hardware environments inherit common, via independent hardware profiles in platformio.ini. HOR_RES, VER_RES, I2C/QSPI pins, driver selection, direct reset pins and display enable are profile capabilities. Firmware version is defined once by common.custom_firmware_version, with a release-tag override for CI. HW_TYPE is the environment name, which also determines the release asset filename and board-specific application project identity.

## Wi-Fi and OTA

A maintenance worker owns scans, bounded connection attempts, GitHub release checks and OTA writes. UI state is projected through a locked snapshot; worker callbacks never touch Slint. Public GitHub release metadata is bounded to 64 KiB. Redirects must retain HTTPS and one of the explicitly allowed GitHub asset hosts. The clock is synchronized before certificate-verified requests.

OTA selects the matching environment.bin asset and validates its GitHub SHA256 digest, size, image format/chip and board-specific project name. Only a complete verified image can be selected as the next boot partition. Cancellation and commit arbitrate through an atomic phase. The BLE transport pauses during flashing, resumes after cancellation/failure, and remains paused until an explicit restart after success. New-image acceptance occurs only after initialization and ten seconds of continued UI-loop operation; bootloader rollback remains available before acceptance.

The first USB installation must include the new two-slot partition table and OTA initialization image. Later application-only OTA preserves the running slot until commit. The device uses certificate-verified HTTPS and digest checks; a separate firmware signing authority and secure boot are not configured.

## Shared UI components

`ui/dashboard.slint` is the window and application-facing API: it owns shared state, responsive breakpoints, navigation transitions and modal ordering. Page components receive explicit properties and callbacks; mutable drafts and selections use two-way bindings. Firmware, native and browser simulators all use this same window.

| UI source | Responsibility |
|---|---|
| `ui/pages/ride.slint` | Portrait and landscape telemetry layouts |
| `ui/pages/tune.slint` | Ride presets and speed-limit drafts |
| `ui/pages/bike.slint` | Vehicle discovery, integration selection and locking actions |
| `ui/pages/settings.slint` | Display, units, transport and proxy preferences |
| `ui/pages/wifi.slint` | Network discovery and saved connections |
| `ui/pages/updates.slint` | Release checks, downloads and restart actions |
| `ui/pages/about.slint` | Version information and diagnostic rows |
| `ui/models.slint` | Shared peer, network and diagnostic row types, re-exported by the window |
| `ui/header.slint`, `ui/navigation.slint` | Connection status, Settings access and primary tabs |
| `ui/battery-meter.slint`, `ui/speedometer.slint` | Reusable telemetry indicators |
| `ui/settings.slint`, `ui/controls.slint` | Shared rows, steppers, actions and metric cards |
| `ui/dialogs/` | Bike-change and firmware-install confirmations |

Ride, Tune, Bike, Wi-Fi, Updates and About inherit `TransitionPage`, retaining the same incoming/outgoing-page lifecycle without adding a layout wrapper. Settings uses its scrolling `Page` inside a `TransitionPage`. Gesture offsets remain owned by `PageDeck` and are forwarded to scrolling content. New Slint files are included automatically by the recursive firmware UI dependency scan.

Page provides a vertically scrolling content layout. Action, Metric, SettingsRow, Stepper, SectionTitle and InfoRow are reusable themed components. Keyboard provides case/symbol modes, password masking/reveal, space and delete, with bounded SSID/password editors in the application. Settings links to Wi-Fi, Updates and About; About has separate version and live-diagnostics views. Speed and temperature preferences are independent, while fault/overheat decisions use native protocol units.

The header Settings cog uses a Lucide SVG and a reusable 48-pixel IconAction touch target. Ride, Tune and Bike remain in the bottom navigation; Settings and its subpages highlight the header cog.

Rounded-panel layout uses the profile-supplied DISPLAY_SAFE_INSET (32 pixels on the 2.06 board) through a reusable SafeArea component. This conservative symmetric inset is rotation invariant; it is not a claimed measurement of the panel corner radius. The fixed header shows the connection state or a more specific alert; vehicle branding remains separate from the status controls.

PageDeck and TransitionPage implement opaque 250 ms ease-out navigation with a retained outgoing page. Only the active and previous page exist during a slide; the previous page is released at completion. A clipped content viewport keeps the header, status and bottom navigation fixed inside the safe area. A pointer blocker prevents controls on moving pages from receiving clicks. Programmatic changes coalesce to the latest requested page; resize/rotation and modal input snap to the requested page. No screen capture or additional display framebuffer is used.

Primary tabs use a shared SwipeGestureHandler with dominant-axis locking. Horizontal drags retain only the current and adjacent tab, with a bounded offset and no wrap at Ride/Bike. A 40-pixel deliberate drag commits; short or reversed drags snap back. Snap duration scales with remaining distance. The recognizer cancels child presses before dragging, so Apply/peer actions cannot be triggered by a swipe. Vertical motion is delegated to SwipeScroll, a clipped non-interactive Flickable with clamped manual offsets; this avoids nested recognizers swallowing quick vertical drags. Orientation changes and modal input cancel active gestures.

VehicleIntegration provides independent WFB, wheel-e and generic VESC implementations for branding, capability flags, polling, decoding and command construction. Instances are static and share the VESC transport. A verified NVS association binds address, integration and cached name atomically. The stream session invalidates pending work when changing vehicles. The Bike picker prioritizes matching names and resolves selections by address; the bridge uses its own user-configurable BLE name, defaulting to `WB Disp`.

The security module owns PIN verification, NVS records, lock transactions and screen idling. PinPad exposes masked numeric entry above all other UI. A same-session, fresh stopped-state reply confirms an explicit bike lock/unlock request. The input backend consumes the wake contact until release and restores configured brightness between completed display frames. See the integration and locking guides for supported commands and verification limits.


Shared C++ APIs live in `firmware/components/bike/include/bike/` under namespace `bike`. LinkStatus describes transport-independent connection state; BleLifecycle owns the BLE-specific state transitions. WFB-only command helpers retain explicit `wfb_` names, and WfbIntegration remains a product-specific implementation. Generic build/test variables use the `BIKE_` prefix. Product IDs, branding and supplied protocol references retain their original names.

The ESP application project identity is `wfd_<hardware-id>`. It identifies the display hardware for OTA validation independently of the selected vehicle integration and displayed brand.
