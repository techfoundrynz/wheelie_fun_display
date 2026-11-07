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

NimBLE runs its host task. The phone relay uses three 16 KiB PSRAM byte rings for ingress, upstream receive and phone output. It validates a complete phone frame, including CRC, before any bytes of that frame reach the upstream. Frames are capped at 16 KiB including framing and coalesced into up to 512-byte batches with boundary and reply-key/firmware conflict guards. Normal display receive work retains its bounded packet queue. A dedicated transmission task uses negotiated ATT MTU minus three bytes. Unaccepted BLE `ENOMEM`/`EBUSY` writes retry the same bytes for up to two seconds with yields; NimBLE-accepted writes are not resent.

The application/UI task consumes receive fragments, checks connection generation, feeds the VESC decoder, updates telemetry and dispatches Slint input/rendering. UI properties are never touched from BLE callbacks. A mutex protects transport snapshots; bounded queues carry cross-task work. Old-generation RX and TX work is discarded after reconnect. Decoder and telemetry state are reset whenever the connection is not ready.

The application targets 10 Hz each for `COMM_GET_VALUES_SETUP` and the selected integration's package telemetry (up to 20 requests/s), subject to per-family credits and phone arbitration. Due polls retry until accepted. WFB uses `GET_ALL_DATA(mode=1)`; wheel-e uses `REALTIME_DATA` with state, pitch and roll masks. Generic VESC polls only standard telemetry. Poll cadence is independent of the untrusted reported loop time. Setup values provide speed in m/s (signed integer / 1000), battery and temperatures; package replies supply attitude and state. Unknown responses and replies for another package are ignored. Short/malformed replies cannot partially overwrite an existing sample. Setup and package freshness expire independently after 1500 ms.

## Read-only polling alongside the phone proxy

`transport::poll` brokers the display's read-only requests even while a phone is connected. Phone traffic has priority at complete-packet boundaries, with one display poll in flight and bounded per-family reply credits that expire after one second in both solo and proxy operation. Broker-owned replies are consumed locally rather than forwarded to the phone. Package command 36 and BMS command 96 require broker ownership. A phone command-47 setup reply can also update UI telemetry only when its controller ID at payload offset 56 matches the discovered controller or known solo ID; native CAN also requires the matching sender. Such packets carry `Packet.passive_proxy` and never contribute a fabricated owned-request RTT. Other phone replies cannot change dashboard data. After three complete phone packets, a waiting display poll can reserve an exact reply key for at most 200 ms, ending early under ingress queue pressure. Duplicate queued keys are suppressed; an expired unsent reservation yields to the phone without disconnecting it. An unfinished phone packet or phone firmware update temporarily suppresses poll injection. Initial discovery waits for the phone to leave if it had not completed before connection. Display poll admission is independent of phone authentication and notification subscription; unauthenticated phones receive no data. Configuration writes and display OTA remain blocked while the phone is connected. Normal phone departure drains admitted valid frames, discards a never-forwarded partial tail and waits for backend completion plus quiet receive before resuming the existing bike session and route. It does not rely on a controller-parser idle timeout. Source loss or corruption still invokes recovery.

## Trip and lifetime distance

Ride shows `TRIP` and `ODO` in kilometres or miles, following the speed-unit preference. Both come from the selected main controller's standard `COMM_GET_VALUES_SETUP` reply across BLE, CAN and UART, including WFB, wheel-e and generic VESC integrations. No extra polling or display-side accumulation is needed. In portrait, TRIP and ODO occupy the top corners of the speedometer area. The gauge uses its actual painted and text bounds for layout instead of reserving a full-circle placeholder.

Trip is the absolute distance travelled since the controller booted, so reversing also adds distance. Restarting only the display does not reset it. ODO is the controller-maintained lifetime counter. The display reads these values without writing or resetting either counter. The controller's wheel, gearing and motor-pole configuration determines distance accuracy.

The decoder reads absolute trip distance as signed 32-bit millimetres converted to metres, and the optional odometer as unsigned 32-bit metres. Older complete reply prefixes remain supported; missing odometers show `--`. Partial optional scalar fields are rejected transactionally. Disconnected or stale telemetry clears both readouts, preventing values from another vehicle/session remaining on screen.

Wire layout and units follow [VESC command serialization](https://github.com/vedderb/bldc/blob/master/comm/commands.c) and the [controller distance implementation](https://github.com/vedderb/bldc/blob/master/motor/mc_interface.c).

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

NVS stores display preferences, the selected BLE address and Wi-Fi credentials. BLE `forget` clears its peer; Wi-Fi forget clears the saved network. Credentials use the ordinary ESP-IDF NVS store. Upstream BLE and phone proxy SMP bonds are persisted independently; the entered upstream passkey is not saved as a credential. The local screen-lock credential is stored as a salted hash. NVS flash encryption is not configured.


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
| `ui/pages/stats.slint` | Read-only controller and optional BMS telemetry |
| `ui/pages/settings.slint` | Display, units, vehicle, screen lock, phone connection and system sections |
| `ui/pages/wifi.slint` | Network discovery and saved connections |
| `ui/pages/updates.slint` | Release checks, downloads and restart actions |
| `ui/pages/about.slint` | Version information and diagnostic rows |
| `ui/models.slint` | Shared peer, network and diagnostic row types, re-exported by the window |
| `ui/header.slint`, `ui/navigation.slint` | Connection status, Settings access and primary tabs |
| `ui/battery-meter.slint`, `ui/speedometer.slint`, `ui/distance-summary.slint` | Reusable telemetry indicators |
| `ui/settings.slint`, `ui/controls.slint` | Shared rows, steppers, actions and metric cards |
| `ui/dialogs/` | Bike-change and firmware-install confirmations |

Ride, Stats, Tune, Wi-Fi, Updates and About inherit `TransitionPage`, retaining the same incoming/outgoing-page lifecycle without adding a layout wrapper. Settings uses its scrolling `Page` inside a `TransitionPage`. Gesture offsets remain owned by `PageDeck` and are forwarded to scrolling content. New Slint files are included automatically by the recursive firmware UI dependency scan.

Page provides a vertically scrolling content layout. Action, Metric, SettingsRow, Stepper, SectionTitle and InfoRow are reusable themed components. Keyboard provides case/symbol modes, password masking/reveal, space and delete, with bounded SSID/password editors in the application. Settings links to Wi-Fi, Updates and About; About has separate version and live-diagnostics views. Speed and temperature preferences are independent, while fault/overheat decisions use native protocol units.

`MarqueeText` keeps connection status on one line in a clipped viewport. Overflow pans at 32 px/s with 1.2-second pauses at either end; text and width changes reset it. Its timer stops when text fits, animations are disabled, or a modal/sleep state hides the header. Gauge arcs use the renderer's native round caps; painted-height accounting preserves the landscape battery gap.

The header Settings cog uses a Lucide SVG and a reusable 48-pixel IconAction touch target. Ride, Stats and Tune remain in the bottom navigation; Settings and its subpages highlight the header cog.

Rounded-panel layout uses profile-supplied `DISPLAY_SAFE_INSET_X` and `DISPLAY_SAFE_INSET_Y` through a reusable SafeArea component. X is left/right clearance and Y is top/bottom clearance in logical UI coordinates; these roles stay the same through all four orientations.

PageDeck and TransitionPage implement opaque 250 ms ease-out navigation with a retained outgoing page. Only the active and previous page exist during a slide; the previous page is released at completion. A clipped content viewport keeps the header, status and bottom navigation fixed inside the safe area. A pointer blocker prevents controls on moving pages from receiving clicks. Programmatic changes coalesce to the latest requested page; resize/rotation and modal input snap to the requested page. No screen capture or additional display framebuffer is used.

Primary tabs use a shared SwipeGestureHandler with dominant-axis locking. Horizontal drags retain only the current and adjacent tab, with a bounded offset and no wrap at Ride/Bike. A 40-pixel deliberate drag commits; short or reversed drags snap back. Snap duration scales with remaining distance. The recognizer cancels child presses before dragging, so Apply/peer actions cannot be triggered by a swipe. Vertical motion is delegated to SwipeScroll, a clipped non-interactive Flickable with clamped manual offsets; this avoids nested recognizers swallowing quick vertical drags. Orientation changes and modal input cancel active gestures.

VehicleIntegration provides independent WFB, wheel-e and generic VESC implementations for branding, capability flags, polling, decoding and command construction. Instances are static and share the VESC transport. A verified NVS association binds address, integration and cached name atomically. The stream session invalidates pending work when changing vehicles. The Settings vehicle section's picker prioritizes matching names and resolves selections by address; the bridge uses its saved user-configurable BLE name (default `WB Disp`) for wheel-e/generic VESC. Only active WFB forces the runtime name `WFB` and hides the editor, without mutating that saved preference.

Before connection verification, UI branding and feature previews follow an explicitly selected integration. Auto waits for discovery. A ready session always supplies the verified integration, while protocol decoding, polling and command dispatch use only that verified transport integration. Previewing WFB controls never enables disconnected writes.

The security module owns PIN verification, NVS records, lock transactions and screen idling. PinPad selects six-digit numeric or six-dot pattern input using properties. Local PINs submit automatically after six digits; patterns submit on gesture release after six distinct dots. Upstream BLE pairing uses numeric entry with explicit Submit and accepts one to six digits. Incorrect codes clear the entry while retaining the existing retry limits. A same-session, fresh stopped-state reply confirms an explicit bike lock/unlock request. The input backend consumes the wake contact until release and restores configured brightness between completed display frames. See the integration and locking guides for supported commands and verification limits.


Shared C++ APIs live in `firmware/components/bike/include/bike/` under namespace `bike`. LinkStatus describes transport-independent connection state; BleLifecycle owns the BLE-specific state transitions. WFB-only command helpers retain explicit `wfb_` names, and WfbIntegration remains a product-specific implementation. Generic build/test variables use the `BIKE_` prefix. Product IDs, branding and supplied protocol references retain their original names.

The ESP application project identity is `wfd_<hardware-id>`. It identifies the display hardware for OTA validation independently of the selected vehicle integration and displayed brand.

Settings combines vehicle discovery and integration, screen locking, display/units, phone proxy and system controls. Vehicle and screen-lock groups are reusable section components. Stats is a read-only primary tab. Standard setup telemetry supplies electrical/energy values, duty, eRPM, PID position, controller ID/count and uptime; pitch/roll require fresh integration telemetry. Standard BMS command 96 receives an initial read-only probe on Ready and is then polled at 1 Hz while Stats is open. During a phone session, command-96 polling requires a successful reply earlier in the same vehicle session. Pack, cells, balancing, temperatures, SOC and SOH are VESC-cached values without a BMS-source timestamp. Hardware BMS acceptance remains open.
