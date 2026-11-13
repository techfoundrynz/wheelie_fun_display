# Performance configuration

This records board-specific configuration and local measurements. Startup work timings and memory samples do not establish a sustained frame rate.

## P4 link time on Windows

The pinned GCC 15.2 RISC-V linker stalls in relaxation on the generated Slint object's roughly 2.1 million symbols (143 MB in the measured build). Replaying the link with relaxation disabled completed in 2.28 seconds; the normal relink was still running after 500 seconds when its inputs changed. This identifies the bottleneck, rather than an end-to-end build speedup.

Only the generated P4 `dashboard.cpp` uses `-mno-relax`. Debug information is retained, and the SDK, renderer and runtime keep normal relaxation. A global disable increased executable text by about 167 KiB and is not used.

SDK changes invalidate cached linker scripts and bootloader configuration. Post-link checks enforce the P4 XiP protection boundaries: a stale template previously caused an early instruction-access fault.

## S3 runtime configuration

| Setting | Selected value | Reason |
|---|---|---|
| CPU | 240 MHz | ESP32-S3 performance clock |
| UI task stack | 32 KiB internal SRAM | Keep layout/rendering headroom and permit flash/NVS operations with caches disabled |
| C/C++ optimization | Performance (`-O2`) | Retain assertions and checks |
| Flash | QIO, 80 MHz, 32 MB | Supported target configuration; verify cold boot on this board |
| PSRAM | Octal, 80 MHz | Board has 8 MB; avoid experimental clock settings |
| Instruction fetch from PSRAM | Enabled | Execute application instructions from external RAM |
| Read-only data / full XiP helper | Enabled | Keep application instructions and read-only resources in PSRAM |
| Instruction cache | 32 KB, 32-byte lines | Supported maximum; 64-byte instruction lines do not exist here |
| Data cache | 64 KB, 64-byte lines | Modest measured throughput gain; PSRAM allocation policy preserves internal headroom |
| Always-internal allocation threshold | 0 bytes | Ordinary allocations prefer PSRAM; DMA buffers, task stacks and internal-only driver allocations remain internal |
| Internal reserve | 32 KB | Leave internal-only allocations available |
| NimBLE host allocations | External PSRAM | Preserve internal RAM for controller, driver and task requirements |
| NVS page/key cache | PSRAM | Persistent bond/settings lookups must not consume DMA/stack headroom |
| Large transport state | Explicit PSRAM allocations | Keep reassembly, decoded-packet rings and controller discovery outside internal heap |
| Wi-Fi/LwIP allocation preference | PSRAM preferred | CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP |
| Wi-Fi DMA/cache pools | RX 4, TX 8, TX cache 16 | Explicit limits; PSRAM mode otherwise defaults to 16 RX + 16 TX DMA buffers |
| Wi-Fi RX block-ack window | 4 | Keep the window within the four static RX buffers, per IDF Kconfig guidance |
| TLS heap | External PSRAM | TLS buffers must not exhaust internal DMA/task memory |
| PSRAM test, assertions, watchdogs | Retained | No check is disabled for speed |
| Slint | `mcu-v1.19.5` | Hardened bitmap/geometry caches and direct scanline rendering |
| Panel QSPI | 80 MHz | Hardware-profile clock supplied by DISP_QSPI_HZ |

`scripts/check_sdkconfig.py` checks the generated configuration so silent Kconfig fallback is detected. ESP-IDF's derived image/header flash mode can say `dio` while the bootloader's selected `CONFIG_ESPTOOLPY_FLASHMODE_QIO=y` enables QIO after initial boot; audit the choice symbol as well as the runtime boot log.

The UI embeds bitmap Roboto fonts, limiting large-font glyphs to numeric characters. The header displays connection status beside the Settings cog. On ESP32-S3, ordinary scrolling renders dirty spans directly through three internal scanline buffers. Page transitions use the streamed page compositor described below.

The backend now incorporates the applicable transfer changes described below:

- Each dirty rectangle accumulates independently, avoiding a flush for every interleaved scanline.
- Narrow dirty spans use the full existing byte allocation, with an even row capacity calculated from span width. Each of the three buffers holds twenty native-width rows: 49,200 bytes total.
- A submitted color transfer overlaps rasterization into another buffer. Buffer reuse, address-window commands, brightness, rotation and frame completion wait for DMA ownership to return, with a bounded timeout. An uncompleted transfer is a fatal error rather than permission to overwrite its buffer.
- Rendering and transfer waits count towards the event-loop budget. Active touch/animation targets approximately 60 Hz; idle input polling retains a 10 ms budget. Every iteration still yields at least one FreeRTOS tick, preserving idle-task watchdog service. Telemetry now targets 10 Hz per family (up to 20 setup-plus-integration requests/s), subject to reply credits and phone arbitration; UI data projection targets 10 Hz.
- Held contacts dispatch pointer moves only when coordinates change.

About > Diagnostics exposes rendered frame count, exponentially averaged frame work and DMA wait, maximum frame work, and average panel transactions. These are work timings, not a delivered FPS measurement. Measure delivered frames over wall-clock time during the same scrolling workload.

The portable scanline test exercises the production accumulator with 2,000 randomized interleaved-region cases and native-size cases, checking exact pixel coverage, even windows, allocation canaries, delayed-DMA buffer immutability and batching. AddressSanitizer and UndefinedBehaviorSanitizer are enabled in the Linux host run.

The hardware profile supplies the 80 MHz panel clock. This halves theoretical full-screen wire time versus the driver's 40 MHz reference default (about 10.3 ms versus 20.6 ms for 410x502 RGB565, excluding command overhead). It does not by itself double delivered FPS. Address-window caching is not implemented. QIO80 flash, octal80 PSRAM, instruction/read-only XiP from PSRAM and pinned renderer caches are enabled. The clock settings are unchanged by the streamed-renderer optimization.

The connected board has passed memory initialization, instruction/read-only copying and the PSRAM test. The user confirmed dashboard, touch, rotations and transition edges. Matched physical FPS measurements are recorded below; sustained radio-load scrolling and extended heap/stack minima still require hardware validation. The following earlier memory investigations used the configurations stated with each capture, rather than today's allocation threshold.

Large task-only BLE packet queue payloads are explicitly allocated in PSRAM with internal FreeRTOS control blocks. Dynamic FreeRTOS queue creation otherwise consumes internal RAM despite the ordinary malloc threshold; with both BLE roles enabled, that exhausted space for the 12 KiB maintenance-task stack. DMA buffers and all task stacks retain internal allocations.

A quiet landscape boot with Wi-Fi PSRAM enabled but unrestricted IDF buffer defaults retained only about 1.1 KiB internal free. A 128-byte malloc-threshold trial did not improve that earlier baseline. The cause was IDF raising the static Wi-Fi pools to 16 RX and 16 TX buffers when PSRAM mode was enabled. Explicit RX4/TX8/cache16 limits avoid that hidden increase. TLS heap allocation also explicitly uses PSRAM.

With RX4/TX8/cache16, TLS/NimBLE external heaps and the original 512-byte threshold, the final quiet boot retained 30,203 bytes internal free and a 15,872-byte largest block, compared with 1,139 bytes free in the earlier unrestricted-pool quiet boot. Both captures used the saved landscape layout. This establishes startup headroom, not an extended radio/TLS soak or FPS comparison.


With persistent SMP bonds enabled, the first integrated transport build had only 6,383 bytes internal free at UI acceptance. Explicit PSRAM state allocation plus `CONFIG_NVS_ALLOCATE_CACHE_IN_SPIRAM=y` raised this to 19,147 bytes (largest block 7,424 bytes) in the final 24-second startup capture. That capture used the earlier 512-byte allocation threshold, internal task stacks, three DMA scanline buffers and 80 MHz panel clock. That earlier 16 KiB UI stack retained only 1,640 bytes at its startup high-water mark. The current UI task uses a 32 KiB internal stack; this capacity is audited in the generated SDK configuration. Startup and all-screen high-water measurements must be repeated after layout changes. NVS operations are low-frequency and trade some metadata-access speed for internal driver/stack headroom. Sustained radio/load and all-screen minima still need hardware measurement.

## Immutable font metadata

Generated font tables need constant initialization to remain in flash-mapped read-only memory. The pinned compiler emits slice helper calls that otherwise put immutable glyph metadata in internal DRAM. After Slint code generation, `scripts/prepare_slint_fonts.py` rewrites only recognized static `BitmapGlyph`, `BitmapGlyphs` and `BitmapFont` resources with constexpr slices and `constinit const`. Unknown initializer forms fail preparation or compilation. Runtime UI expressions and image resources are untouched.

In the measured four-weight baseline, ELF inspection confirmed all 48 font resources (79,072 bytes) reside in flash-mapped memory, with none of this font metadata in internal RAM. Static RAM fell from 128,832 to 49,760 bytes in that baseline. The current font-size set also includes 120 px digits; every build audits the actual linked font resources rather than assuming this historical count or byte total. Font pixels, glyph coverage, metrics and weights are unchanged; this is a placement improvement, not an FPS claim. The UI task keeps its 32 KiB internal stack so flash/NVS operations can safely run when caches are disabled.

A 24-second device startup capture of the optimized build reached UI acceptance without panic or reset: internal free 51,447 bytes, largest internal block 31,744 bytes and UI stack high-water free 17,864 bytes. The panel remained at 80 MHz. These are startup headroom measurements; exercise all pages, active communications and updates when measuring worst-case runtime minima.

## ESP32-S3 streamed page transitions

The S3 renderer retains seven page trees and three native RGB565 images in PSRAM: fixed chrome and two page snapshots, totaling 1,234,920 bytes. Transitions compose rows directly into three internal twenty-row DMA buffers. Ordinary scrolling rasterizes dirty spans into those buffers. This avoids repeatedly laying out and rasterizing two moving pages.

Small updates refresh the stationary snapshot; scrolling invalidates it for recapture before the next slide. During slides, translation follows each rendered input frame; incoming content is rerendered no more than once every 30 ms. Telemetry projection remains 10 Hz, and lock/modal changes take effect immediately. Rotation repaints all 205,820 native pixels before incremental rendering resumes, preventing old controls from remaining in the margins.

### S3 measurements

Measured on 12 November 2026 with a Waveshare AMOLED 2.06, local firmware 0.1.4 and ESP-IDF 6.1. The baseline used the original scanline renderer, 32 KiB data cache, instruction-only PSRAM copying and a 512-byte malloc threshold. The selected configuration uses a 64 KiB data cache, instruction/read-only XiP and a zero-byte threshold; clock speeds are unchanged.

Each stage ran for 30 seconds after settling, using real UI pointer events without a connected controller or mocked telemetry. FPS counts completed QSPI/DMA frames over wall time; minimum FPS is the lowest complete, non-overlapping one-second bucket.

| Rotation | Slides before, average FPS | Slides after, average FPS | Minimum after | Settings before, average FPS | Settings after, average FPS | Minimum after |
| --- | --- | --- | --- | --- | --- | --- |
| 0° | 22.34 | 36.69 | 35.36 | 29.33 | 35.34 | 34.38 |
| 90° | 25.82 | 39.43 | 37.43 | 32.85 | 35.39 | 34.34 |
| 180° | 20.71 | 35.97 | 34.28 | 26.22 | 33.19 | 32.10 |
| 270° | 25.01 | 39.49 | 37.25 | 32.23 | 35.48 | 34.38 |

| Rotation | Drags before, average FPS | Drags after, average FPS | Minimum after | Stats before, average FPS | Stats after, average FPS | Minimum after |
| --- | --- | --- | --- | --- | --- | --- |
| 0° | 18.09 | 36.18 | 35.36 | 31.04 | 33.93 | 32.77 |
| 90° | 19.62 | 38.43 | 37.58 | 31.87 | 33.71 | 31.28 |
| 180° | 18.06 | 35.80 | 34.86 | 28.20 | 32.20 | 30.78 |
| 270° | 19.58 | 38.39 | 37.62 | 32.10 | 33.97 | 31.65 |

All sixteen stages completed without a reset, abort or idle-motion interval. Frame-interval p95 was 72–75 ms for slides, 52–66 ms for drags, 30–33 ms for Settings and 46–64 ms for Stats. Initial page capture still causes longer frames; these averages do not establish uniform latency or 50 FPS.

Stage-end internal heap stayed at 37,951 bytes free (largest block 19,456), compared with a baseline sample of 39 bytes during page cycling. DMA-capable free memory was 30,163 bytes, PSRAM free was 1.99–2.02 MB, and UI stack high-water free was 12,692 bytes. The boot-inclusive internal-heap minimum was 18,132 bytes. Occasional FT3168 I²C errors persist; measured touch polls stayed below 1 ms. Transactions have a 20 ms timeout.

The display, touch, rotations and transition edges passed physical checks. Normal firmware with profiling disabled passed startup and five controlled USB-serial-JTAG restarts, each retaining 39,103 bytes internal free and 12,948 bytes free UI stack. These checks do not qualify controller/radio load, OTA, temperature or power-cycle reliability.

Local logs: `.cache/s3-fps-baseline-validation.log`, `.cache/s3-fps-final-validation.log` and `.cache/s3-production-boot.log`.

## ESP32-P4 rendering

The GUITION renderer uses two native RGB565 DSI scanout buffers and two page snapshots in 250 MHz PSRAM (3,072,000 bytes total). Slint renders rotated dirty spans through two internal 32-row tiles sharing 61,440 bytes. PPA copies overlap rasterization; completion signals prevent tile reuse before DMA finishes.

Slint paints each frame's damage once. Before reusing a retired scanout buffer, PPA restores unchanged regions from the latest front buffer. Both buffers are restored after a slide to prevent stale edges on subsequent scrolling. The incoming page stays stationary and live while PPA translates the snapshots; reversal and cancellation preserve the outgoing image.

Both targets retain seven page trees and prepare layouts offscreen after startup, rotation or credential screens. Pointer input releases the stationary capture before hit testing. Stats and Tune provide bounded scroll momentum, stopped by a new contact or cancellation. Page slides include the side margins while header/navigation stay fixed; GUITION corner-safe insets are zero.

The P4 uses 400 MHz CPU, 256 KiB L2 cache with 128-byte lines, instruction/read-only XiP and a 32-byte malloc threshold. Task stacks and DMA buffers remain internal. Watchdogs, assertions and PSRAM testing remain enabled. See Espressif's [configuration reference](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/kconfig-reference.html#config-spiram-speed) and [speed guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-guides/performance/speed.html).

The pinned P4 toolchain needs `_GLIBCXX_HAVE_ATOMIC_LOCK_POLICY=1` to avoid leaking lazy pthread mutexes through mutex-based shared ownership. The S3 toolchain already uses atomics. Staged Slint headers also prune expired model listeners, and About diagnostics update only changed rows. The P4 SDIO memory pool reserves receive-buffer headroom during page-allocation churn.

### P4 measurements

Measured on 12 November 2026 with a GUITION P4 v3.2, local firmware 0.1.4 and 250 MHz PSRAM. Each stage ran for 30 seconds after settling, without a connected controller or mocked telemetry. FPS counts completed framebuffer submissions over wall time; minimum FPS is the lowest complete, non-overlapping one-second bucket.

| Rotation | Automatic slides, average FPS | Minimum FPS | Settings scroll, average FPS | Minimum FPS |
| --- | --- | --- | --- | --- |
| 0° | 55.02 | 53.43 | 56.20 | 55.63 |
| 90° | 54.27 | 52.03 | 55.81 | 54.65 |
| 180° | 54.50 | 52.07 | 51.43 | 50.71 |
| 270° | 54.30 | 52.04 | 56.93 | 55.84 |

Frame-interval p95 was 38–42 ms for slides and 24–28 ms for Settings, including initial page capture. The averages do not mean every frame finishes within 20 ms.

| Rotation | Horizontal drags, average FPS | Minimum FPS | Stats flicks, average FPS | Minimum FPS |
| --- | --- | --- | --- | --- |
| 0° | 58.32 | 57.13 | 60.38 | 58.18 |
| 90° | 58.53 | 57.87 | 58.12 | 55.39 |
| 180° | 57.46 | 56.48 | 56.92 | 54.45 |
| 270° | 58.51 | 57.30 | 58.07 | 55.10 |

Stats tests alternate 400 ms drags over 20% of viewport height and 20 ms of release momentum, away from scroll bounds. All orientations recorded zero idle-motion time. Repeated stages used the same renderer and SDK configuration; only diagnostic positioning/logging changed.

Transition/Settings stage-end samples retained 72,403–83,219 bytes internal free (largest block 31,744). The final Stats repeat retained 76,539 bytes internal free and a 25,600-byte largest DMA block. These are samples, not continuous minima.

Physical checks covered display/touch at all four angles, stale-edge removal, reversals, cancellation and flick momentum. Normal firmware passed startup and five controlled USB-serial-JTAG restarts, retaining 94,619–94,739 bytes internal free and 11,400 bytes free UI stack. An earlier startup panic during a rapid flash/reset sequence did not recur; its cause remains unconfirmed. Controller/radio load, OTA, temperature and power-cycle reliability remain unqualified. P4 was build-tested, but not physically reprofiled, during the later S3 changes.

Local logs: `.cache/p4-fps-final-full-validation.log`, `.cache/p4-fps-final-repeat-validation.log`, `.cache/p4-fps-stats-positioned-validation.log` and `.cache/p4-production-boot.log`.

## Reproduce FPS measurements

1. Set `WFD_RENDER_PROFILE=1` and `WFD_RENDER_PROFILE_SECONDS=30`, then build and upload the matching board environment. Profiling is disabled by default.
2. Check the display during the 60-second manual window, then capture all sixteen `render_profile: RESULT` records. The diagnostic cycles all seven pages using UI pointer events, owns input during measurement and restores preferences without NVS writes. A modal or locked screen aborts it.
3. To resume, set `WFD_RENDER_PROFILE_START_STAGE` to the zero-based stage index: 0–7 alternate slides/Settings at 90°, 0°, 270°, 180°; 8–11 test horizontal drags in that order; 12–15 test Stats flicks.
4. Unset all three variables, rebuild and flash normal firmware. Do not distribute diagnostic images.

Results include wall-clock FPS, minimum one-second FPS, frame-interval p95, render/copy/wait timings and memory samples. Counters cannot detect panel artifacts, so inspect the display too. Both targets passed builds and SDK audits; all 22 UI scenarios passed with snapshots enabled and disabled, and all 14 sanitizer-backed host suites passed.

## PIN verification memory headroom

A boot-PIN crash was traced against the matching ELF to lazy allocation of the hardware SHA mutex (`lock_init_generic` -> `esp_crypto_sha_aes_lock_acquire` -> `psa_hash_compute`). The affected UI retained only 1,291 bytes of internal heap, with a 640-byte largest block after its first frame. Small UI allocations had consumed the remaining internal memory.

The malloc threshold in this earlier capture was 512 bytes, preserving the fast path for small UI allocations. The battery text halo now uses fixed glyph layers rather than allocating 32 dynamic repeater item trees for its two labels; its offsets, opacity and soft appearance are unchanged. Security initialization exercises SHA before the first UI frame, allocating its hardware lock early. Recoverable crypto errors leave the PIN screen locked and report a retry message without recording an incorrect attempt or changing the stored PIN. The stored hash format is unchanged. Task stacks, FreeRTOS control blocks and explicitly internal DMA buffers retain their allocation policy. On the connected S3, the PIN-screen startup sample retained 2,743 bytes internal free (largest block 1,408 bytes), compared with 1,291 bytes (largest 640 bytes) in the crashing image. The flashed image logged a successful screen unlock, and the user confirmed that pressing OK returned to the dashboard without a reboot. Internal headroom on this screen remains tight; this capture establishes the unlock fix, not sustained radio/TLS memory margins or a rendering-speed comparison.


The subsequent PIN-screen redesign retains the 512-byte threshold and uses fixed keypad rows instead of nested repeaters. A startup sample of that flashed S3 image retained 41,711 bytes of internal heap, with a 10,752-byte largest block and 17,632 bytes of free UI stack at its high-water mark. The redesigned screen shows visible digits and automatically submits the sixth accepted digit. The user confirmed the layout and automatic unlock on the connected display, matching the successful unlock in the serial capture. These startup samples compare different PIN layouts and do not establish rendering-speed or sustained radio-load performance.

The final backspace/Clear icon build retained 14,151 bytes internal free (largest block 7,680 bytes) at its first-frame startup sample, with 17,920 bytes of free UI stack at the high-water mark. Its serial capture also recorded a successful automatic unlock. The earlier 41,711-byte sample followed 13 rendered frames, so these samples are not equivalent first-frame measurements.

## Pattern rendering stack headroom

The pattern-entry renderer uses pre-rendered sprite assets in place of live path geometry. On the connected Waveshare S3, a capture after unlocking measured **17,216 bytes free of the 32,768-byte UI task stack** at its high-water mark. The fix does not increase task-stack sizes or global allocations. This measurement covers the observed unlock workload; it is not a worst-case guarantee for every screen or concurrent radio and update activity.

## Measured BLE telemetry throughput

On 7 November 2026 (local time), the Waveshare 2.06 original ESP32-S3 display ran six completed 12-second read-only stages against an actual wheel-e BLE module. The normal Ride UI remained active, the phone was disconnected, and the panel ran at 80 MHz. This build used ESP-IDF 6.1 and Slint `mcu-v1.19.5`. Standard command 47 replies contained 70 payload bytes; wheel-e command 36/101/33 replies contained 28. ATT MTU was 23 (20 usable bytes). The negotiated connection interval was 31.25 ms, latency zero and supervision timeout 4 seconds, after an initial 50 ms / 2.56-second configuration.

| Offered rate per family | Aggregate offered requests/s | Received replies/s | Mean RTT (ms) | p95 RTT bucket upper bound (ms) |
| --- | --- | --- | --- | --- |
| 5 Hz | 10 | 9.74 | 83.1 | 160 |
| 10 Hz | 20 | 16.42 | 81.3 | 130 |
| 15 Hz | 30 | 18.67 | 80.0 | 130 |
| 20 Hz | 40 | 19.91 | 78.1 | 130 |
| 30 Hz | 60 | 23.91 | 67.8 | 110 |
| 40 Hz | 80 | 28.82 | 60.3 | 100 |

All six completed stages recorded zero queue failures and zero timeouts. There was at most one pending request per family, without queue flooding. Zero to two requests still in flight at a stage boundary are not packet-loss counts. The peak received application payload was 1,419 bytes/s. Reply counts describe valid framed VESC payload messages, not BLE fragments or decoder-accepted telemetry. An earlier profile aborted on a mode change, followed by a phone-session command-47 timeout; those incomplete stages are excluded from the table.

The shared policy in `bike/polling.hpp` now targets 10 Hz per telemetry family: up to 20 setup-plus-integration requests/s, plus the initial Ready BMS probe and 1 Hz BMS polling while Stats is open. Per-family credits bound outstanding work, and due requests retry until accepted. The target leaves headroom below this link's observed peak of about 29 aggregate replies/s; it does not guarantee 20 delivered replies/s. This is one BLE module and connection configuration, not a universal capacity figure. WFB, real BMS hardware and concurrent phone-proxy throughput have not been validated by this measurement.

The capture also exposed a reported battery charge of 411.2%. The display now preserves nonnegative controller-reported percentages, including values above 100%, on Ride and Stats. The Ride battery fill stops at full while its numeric label retains the reported value. Negative battery estimates and negative optional Wh values remain unavailable without discarding the whole reply.

## Locked pattern diagnostic memory

The later locked-pattern diagnostic capture reached UI readiness at 11,469 ms with **44,423 bytes internal free**, a **31,744-byte largest internal block** and **17,328 bytes free of the 32 KiB UI stack**. This sample preceded local unlock at 161,832 ms; it is not an after-unlock measurement. The earlier locked capture retained only 1,447 bytes internal free with a 448-byte largest block.

The fix replaces dynamic pattern item trees with nine static nodes and five segments, caches selection state and drops covered page children while the screen is locked and the credential dialog is open. Twelve rendered fixtures remained pixel-identical for the pattern rewrite. The 32 KiB UI task stack and 512-byte malloc threshold are unchanged. These are measured locked diagnostic idle captures, not a continuous radio/TLS soak. Local evidence is `.cache/ble-benchmark-fair-device.log` and `.cache/ble-benchmark-shared-device.log`; neither is a distributed artifact.

The user confirmed that other wheel-e Stats values remain visible after the per-field handling of the reported 411.2% battery value. The earlier handling hid that percentage; the current behavior displays it as reported. Hardware validation of that updated numeric display and phone interoperability remain outstanding.

## Relay buffering and handoff checks

The relay now uses three 16 KiB PSRAM byte rings rather than per-fragment relay queues. Complete phone frames are CRC-validated before forwarding, then coalesced into up to 512-byte batches with reply-key and firmware-transfer conflict guards. The phone frame cap is 16 KiB including framing. Nonblocking notification service separates phone-output pressure from phone-to-bike progress.

The ten passing host suites include a 12,000-byte startup burst (600 writes of 20 bytes), coalesced into 24 batches, complete-frame validation and resource/ownership handoff checks. Normal phone departure finishes admitted valid frames and resumes after backend drain and quiet receive, retaining the bike BLE session and route. These tests establish software behavior, not successful phone-app interoperability on hardware; that verification remains pending.

## GUITION Wi-Fi configuration and DMA headroom

A 2026-11-13 capture on the production P4 v3.2 with C6 companion 2.3.2 traced "Wi-Fi configuration rejected" to host DMA exhaustion rather than authentication. The configuration RPC failed before transmission with `ESP_FAIL` while the radio reported transmit-ready, with only 287 bytes DMA-capable memory free and a 12-byte largest block. Bluetooth writes failed in the same interval. The profile still preferred internal memory for allocations up to 32 bytes, allowing keyboard bindings to consume the packet-buffer headroom.

The P4 now uses `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0`, matching the S3's PSRAM-first policy for ordinary allocations. Explicit internal driver buffers and task stacks keep their existing policy. The updated device reached UI readiness with 193,511 bytes internal free and a 151,552-byte largest block, compared with 93,651 and 39,936 bytes in the preceding diagnostic capture. The next Wi-Fi attempt obtained a DHCP address, and the user confirmed connection success. This validates that connection flow with the existing C6 firmware; it is not an OTA, phone-interoperability or long-duration radio-load acceptance test. SDK errors now log transmit readiness and DMA headroom without credentials. Redacted local evidence is in `.cache/wifi-dma-exhaustion-before.log` and `.cache/guition-wifi-errors.log`.
