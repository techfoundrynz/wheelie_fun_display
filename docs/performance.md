# Performance configuration

This records the selected configuration and local measurements for the 2.06-inch display. Startup work timings and memory samples do not establish a sustained frame rate.

| Setting | Selected value | Reason |
|---|---|---|
| CPU | 240 MHz | ESP32-S3 performance clock |
| UI task stack | 32 KiB internal SRAM | Keep layout/rendering headroom and permit flash/NVS operations with caches disabled |
| C/C++ optimization | Performance (`-O2`) | Retain assertions and checks |
| Flash | QIO, 80 MHz, 32 MB | Supported target configuration; verify cold boot on this board |
| PSRAM | Octal, 80 MHz | Board has 8 MB; avoid experimental clock settings |
| Instruction fetch from PSRAM | Enabled | Execute application instructions from external RAM |
| Read-only data / full XiP helper | Disabled | Preserve external heap instead of duplicating read-only data |
| Instruction cache | 32 KB, 32-byte lines | Supported maximum; 64-byte instruction lines do not exist here |
| Data cache | 32 KB, 64-byte lines | Retain internal heap rather than enlarging cache to 64 KB |
| Always-internal allocation threshold | 512 bytes | Larger ordinary allocations prefer PSRAM |
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

The UI embeds bitmap Roboto fonts, limiting large-font glyphs to numeric characters. The header displays connection status beside the Settings cog. On ESP32-S3, dirty regions and three internal scanline buffers avoid full-framebuffer rasterization into PSRAM.

The backend now incorporates the applicable transfer changes described below:

- Each dirty rectangle accumulates independently, avoiding a flush for every interleaved scanline.
- Narrow dirty spans use the full existing byte allocation, with an even row capacity calculated from span width. Full-width transfers retain ten rows; no additional DMA memory is allocated.
- A submitted color transfer overlaps rasterization into another buffer. Buffer reuse, address-window commands, brightness, rotation and frame completion wait for DMA ownership to return, with a bounded timeout. An uncompleted transfer is a fatal error rather than permission to overwrite its buffer.
- Rendering and transfer waits count towards the event-loop budget. Active touch/animation targets approximately 60 Hz; idle input polling retains a 10 ms budget. Every iteration still yields at least one FreeRTOS tick, preserving idle-task watchdog service. Telemetry polling remains 5 Hz and UI data projection 10 Hz.
- Held contacts dispatch pointer moves only when coordinates change.

About > Diagnostics exposes rendered frame count, exponentially averaged frame work and DMA wait, maximum frame work, and average panel transactions. These are work timings, not a delivered FPS measurement. Measure delivered frames over wall-clock time during the same scrolling workload.

The portable scanline test exercises the production accumulator with 2,000 randomized interleaved-region cases and native-size cases, checking exact pixel coverage, even windows, allocation canaries, delayed-DMA buffer immutability and batching. AddressSanitizer and UndefinedBehaviorSanitizer are enabled in the Linux host run.

The hardware profile supplies the 80 MHz panel clock. This halves theoretical full-screen wire time versus the driver's 40 MHz reference default (about 10.3 ms versus 20.6 ms for 410x502 RGB565, excluding command overhead). It does not by itself double delivered FPS. Address-window caching is not implemented. Existing QIO80 flash, octal80 PSRAM, instruction fetch from PSRAM and pinned renderer caches are already enabled; read-only data and full XiP remain disabled.

The connected board has passed cold-boot memory initialization, instruction copying and the PSRAM test, and the user confirmed its dashboard and touch. Radio-load scrolling, corner alignment, extended heap/stack minima and a matched before/after delivered-FPS benchmark still require hardware validation.

Large task-only BLE packet queue payloads are explicitly allocated in PSRAM with internal FreeRTOS control blocks. Dynamic FreeRTOS queue creation otherwise consumes internal RAM despite the ordinary malloc threshold; with both BLE roles enabled, that exhausted space for the 12 KiB maintenance-task stack. DMA buffers and all task stacks retain internal allocations.

A quiet landscape boot with Wi-Fi PSRAM enabled but unrestricted IDF buffer defaults retained only about 1.1 KiB internal free. A 128-byte malloc-threshold trial did not improve that baseline, so the 512-byte setting is retained. The cause was IDF raising the static Wi-Fi pools to 16 RX and 16 TX buffers when PSRAM mode was enabled. Explicit RX4/TX8/cache16 limits avoid that hidden increase. TLS heap allocation also explicitly uses PSRAM.

With RX4/TX8/cache16, TLS/NimBLE external heaps and the original 512-byte threshold, the final quiet boot retained 30,203 bytes internal free and a 15,872-byte largest block, compared with 1,139 bytes free in the earlier unrestricted-pool quiet boot. Both captures used the saved landscape layout. This establishes startup headroom, not an extended radio/TLS soak or FPS comparison.


With persistent SMP bonds enabled, the first integrated transport build had only 6,383 bytes internal free at UI acceptance. Explicit PSRAM state allocation plus `CONFIG_NVS_ALLOCATE_CACHE_IN_SPIRAM=y` raised this to 19,147 bytes (largest block 7,424 bytes) in the final 24-second startup capture. The general 512-byte allocation threshold, internal task stacks, three DMA scanline buffers and 80 MHz panel clock remain unchanged. That earlier 16 KiB UI stack retained only 1,640 bytes at its startup high-water mark. The current UI task uses a 32 KiB internal stack; this capacity is audited in the generated SDK configuration. Startup and all-screen high-water measurements must be repeated after layout changes. NVS operations are low-frequency and trade some metadata-access speed for internal driver/stack headroom. Sustained radio/load and all-screen minima still need hardware measurement.

## Immutable font metadata

Generated font tables need constant initialization to remain in flash-mapped read-only memory. The pinned compiler emits slice helper calls that otherwise put immutable glyph metadata in internal DRAM. After Slint code generation, `scripts/prepare_slint_fonts.py` rewrites only recognized static `BitmapGlyph`, `BitmapGlyphs` and `BitmapFont` resources with constexpr slices and `constinit const`. Unknown initializer forms fail preparation or compilation. Runtime UI expressions and image resources are untouched.

In the measured four-weight baseline, ELF inspection confirmed all 48 font resources (79,072 bytes) reside in flash-mapped memory, with none of this font metadata in internal RAM. Static RAM fell from 128,832 to 49,760 bytes in that baseline. The current font-size set also includes 84 px digits; every build audits the actual linked font resources rather than assuming this historical count or byte total. Font pixels, glyph coverage, metrics and weights are unchanged; this is a placement improvement, not an FPS claim. The UI task keeps its 32 KiB internal stack so flash/NVS operations can safely run when caches are disabled.

A 24-second device startup capture of the optimized build reached UI acceptance without panic or reset: internal free 51,447 bytes, largest internal block 31,744 bytes and UI stack high-water free 17,864 bytes. The panel remained at 80 MHz. These are startup headroom measurements; exercise all pages, active communications and updates when measuring worst-case runtime minima.

## ESP32-P4 rendering

The GUITION target uses a persistent logical RGB565 canvas and two full-screen DSI scanout buffers in 200 MHz PSRAM. At 480 × 800, those three buffers occupy 2,304,000 bytes. Slint renders into the logical canvas, then PPA rotates/copies it into the inactive native scanout buffer. Cache maintenance and display refresh completion protect buffer ownership before reuse. The PPA path handles all four rotations without changing the ESP32-S3 scanline implementation.

The P4 profile uses the supported 360 MHz CPU setting for the selected pre-v3 silicon revision. These are configured capabilities and build checks; physical P4 timing, cache coherence, tearing, touch alignment and concurrent C6 radio operation still need acceptance on the board.
