var e={source:`docs/performance.md`,route:`performance`,title:`Performance`,group:`For developers`,description:`Configuration decisions and the limits of measured results.`,body:`<p>This records the selected configuration and local measurements for the 2.06-inch display. Startup work timings and memory samples do not establish a sustained frame rate.</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Setting</th>
<th>Selected value</th>
<th>Reason</th>
</tr>
</thead>
<tbody>
<tr>
<td>CPU</td>
<td>240 MHz</td>
<td>ESP32-S3 performance clock</td>
</tr>
<tr>
<td>UI task stack</td>
<td>32 KiB internal SRAM</td>
<td>Keep layout/rendering headroom and permit flash/NVS operations with caches disabled</td>
</tr>
<tr>
<td>C/C++ optimization</td>
<td>Performance (<code>-O2</code>)</td>
<td>Retain assertions and checks</td>
</tr>
<tr>
<td>Flash</td>
<td>QIO, 80 MHz, 32 MB</td>
<td>Supported target configuration; verify cold boot on this board</td>
</tr>
<tr>
<td>PSRAM</td>
<td>Octal, 80 MHz</td>
<td>Board has 8 MB; avoid experimental clock settings</td>
</tr>
<tr>
<td>Instruction fetch from PSRAM</td>
<td>Enabled</td>
<td>Execute application instructions from external RAM</td>
</tr>
<tr>
<td>Read-only data / full XiP helper</td>
<td>Disabled</td>
<td>Preserve external heap instead of duplicating read-only data</td>
</tr>
<tr>
<td>Instruction cache</td>
<td>32 KB, 32-byte lines</td>
<td>Supported maximum; 64-byte instruction lines do not exist here</td>
</tr>
<tr>
<td>Data cache</td>
<td>32 KB, 64-byte lines</td>
<td>Retain internal heap rather than enlarging cache to 64 KB</td>
</tr>
<tr>
<td>Always-internal allocation threshold</td>
<td>512 bytes</td>
<td>Larger ordinary allocations prefer PSRAM</td>
</tr>
<tr>
<td>Internal reserve</td>
<td>32 KB</td>
<td>Leave internal-only allocations available</td>
</tr>
<tr>
<td>NimBLE host allocations</td>
<td>External PSRAM</td>
<td>Preserve internal RAM for controller, driver and task requirements</td>
</tr>
<tr>
<td>NVS page/key cache</td>
<td>PSRAM</td>
<td>Persistent bond/settings lookups must not consume DMA/stack headroom</td>
</tr>
<tr>
<td>Large transport state</td>
<td>Explicit PSRAM allocations</td>
<td>Keep reassembly, decoded-packet rings and controller discovery outside internal heap</td>
</tr>
<tr>
<td>Wi-Fi/LwIP allocation preference</td>
<td>PSRAM preferred</td>
<td>CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP</td>
</tr>
<tr>
<td>Wi-Fi DMA/cache pools</td>
<td>RX 4, TX 8, TX cache 16</td>
<td>Explicit limits; PSRAM mode otherwise defaults to 16 RX + 16 TX DMA buffers</td>
</tr>
<tr>
<td>Wi-Fi RX block-ack window</td>
<td>4</td>
<td>Keep the window within the four static RX buffers, per IDF Kconfig guidance</td>
</tr>
<tr>
<td>TLS heap</td>
<td>External PSRAM</td>
<td>TLS buffers must not exhaust internal DMA/task memory</td>
</tr>
<tr>
<td>PSRAM test, assertions, watchdogs</td>
<td>Retained</td>
<td>No check is disabled for speed</td>
</tr>
<tr>
<td>Slint</td>
<td><code>mcu-v1.19.5</code></td>
<td>Hardened bitmap/geometry caches and direct scanline rendering</td>
</tr>
<tr>
<td>Panel QSPI</td>
<td>80 MHz</td>
<td>Hardware-profile clock supplied by DISP_QSPI_HZ</td>
</tr>
</tbody>
</table></div>
<p><code>scripts/check_sdkconfig.py</code> checks the generated configuration so silent Kconfig fallback is detected. ESP-IDF's derived image/header flash mode can say <code>dio</code> while the bootloader's selected <code>CONFIG_ESPTOOLPY_FLASHMODE_QIO=y</code> enables QIO after initial boot; audit the choice symbol as well as the runtime boot log.</p>
<p>The UI embeds bitmap Roboto fonts, limiting large-font glyphs to numeric characters. The header displays connection status beside the Settings cog. On ESP32-S3, dirty regions and three internal scanline buffers avoid full-framebuffer rasterization into PSRAM.</p>
<p>The backend now incorporates the applicable transfer changes described below:</p>
<ul>
<li>Each dirty rectangle accumulates independently, avoiding a flush for every interleaved scanline.</li>
<li>Narrow dirty spans use the full existing byte allocation, with an even row capacity calculated from span width. Full-width transfers retain ten rows; no additional DMA memory is allocated.</li>
<li>A submitted color transfer overlaps rasterization into another buffer. Buffer reuse, address-window commands, brightness, rotation and frame completion wait for DMA ownership to return, with a bounded timeout. An uncompleted transfer is a fatal error rather than permission to overwrite its buffer.</li>
<li>Rendering and transfer waits count towards the event-loop budget. Active touch/animation targets approximately 60 Hz; idle input polling retains a 10 ms budget. Every iteration still yields at least one FreeRTOS tick, preserving idle-task watchdog service. Telemetry now targets 10 Hz per family (up to 20 setup-plus-integration requests/s), subject to reply credits and phone arbitration; UI data projection targets 10 Hz.</li>
<li>Held contacts dispatch pointer moves only when coordinates change.</li>
</ul>
<p>About &gt; Diagnostics exposes rendered frame count, exponentially averaged frame work and DMA wait, maximum frame work, and average panel transactions. These are work timings, not a delivered FPS measurement. Measure delivered frames over wall-clock time during the same scrolling workload.</p>
<p>The portable scanline test exercises the production accumulator with 2,000 randomized interleaved-region cases and native-size cases, checking exact pixel coverage, even windows, allocation canaries, delayed-DMA buffer immutability and batching. AddressSanitizer and UndefinedBehaviorSanitizer are enabled in the Linux host run.</p>
<p>The hardware profile supplies the 80 MHz panel clock. This halves theoretical full-screen wire time versus the driver's 40 MHz reference default (about 10.3 ms versus 20.6 ms for 410x502 RGB565, excluding command overhead). It does not by itself double delivered FPS. Address-window caching is not implemented. Existing QIO80 flash, octal80 PSRAM, instruction fetch from PSRAM and pinned renderer caches are already enabled; read-only data and full XiP remain disabled.</p>
<p>The connected board has passed cold-boot memory initialization, instruction copying and the PSRAM test, and the user confirmed its dashboard and touch. Radio-load scrolling, corner alignment, extended heap/stack minima and a matched before/after delivered-FPS benchmark still require hardware validation.</p>
<p>Large task-only BLE packet queue payloads are explicitly allocated in PSRAM with internal FreeRTOS control blocks. Dynamic FreeRTOS queue creation otherwise consumes internal RAM despite the ordinary malloc threshold; with both BLE roles enabled, that exhausted space for the 12 KiB maintenance-task stack. DMA buffers and all task stacks retain internal allocations.</p>
<p>A quiet landscape boot with Wi-Fi PSRAM enabled but unrestricted IDF buffer defaults retained only about 1.1 KiB internal free. A 128-byte malloc-threshold trial did not improve that earlier baseline. The cause was IDF raising the static Wi-Fi pools to 16 RX and 16 TX buffers when PSRAM mode was enabled. Explicit RX4/TX8/cache16 limits avoid that hidden increase. TLS heap allocation also explicitly uses PSRAM.</p>
<p>With RX4/TX8/cache16, TLS/NimBLE external heaps and the original 512-byte threshold, the final quiet boot retained 30,203 bytes internal free and a 15,872-byte largest block, compared with 1,139 bytes free in the earlier unrestricted-pool quiet boot. Both captures used the saved landscape layout. This establishes startup headroom, not an extended radio/TLS soak or FPS comparison.</p>
<p>With persistent SMP bonds enabled, the first integrated transport build had only 6,383 bytes internal free at UI acceptance. Explicit PSRAM state allocation plus <code>CONFIG_NVS_ALLOCATE_CACHE_IN_SPIRAM=y</code> raised this to 19,147 bytes (largest block 7,424 bytes) in the final 24-second startup capture. That capture used the earlier 512-byte allocation threshold, internal task stacks, three DMA scanline buffers and 80 MHz panel clock. That earlier 16 KiB UI stack retained only 1,640 bytes at its startup high-water mark. The current UI task uses a 32 KiB internal stack; this capacity is audited in the generated SDK configuration. Startup and all-screen high-water measurements must be repeated after layout changes. NVS operations are low-frequency and trade some metadata-access speed for internal driver/stack headroom. Sustained radio/load and all-screen minima still need hardware measurement.</p>
<h2 id="immutable-font-metadata">Immutable font metadata<a class="heading-anchor" href="#immutable-font-metadata" aria-label="Link to Immutable font metadata" title="Link to Immutable font metadata"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>Generated font tables need constant initialization to remain in flash-mapped read-only memory. The pinned compiler emits slice helper calls that otherwise put immutable glyph metadata in internal DRAM. After Slint code generation, <code>scripts/prepare_slint_fonts.py</code> rewrites only recognized static <code>BitmapGlyph</code>, <code>BitmapGlyphs</code> and <code>BitmapFont</code> resources with constexpr slices and <code>constinit const</code>. Unknown initializer forms fail preparation or compilation. Runtime UI expressions and image resources are untouched.</p>
<p>In the measured four-weight baseline, ELF inspection confirmed all 48 font resources (79,072 bytes) reside in flash-mapped memory, with none of this font metadata in internal RAM. Static RAM fell from 128,832 to 49,760 bytes in that baseline. The current font-size set also includes 120 px digits; every build audits the actual linked font resources rather than assuming this historical count or byte total. Font pixels, glyph coverage, metrics and weights are unchanged; this is a placement improvement, not an FPS claim. The UI task keeps its 32 KiB internal stack so flash/NVS operations can safely run when caches are disabled.</p>
<p>A 24-second device startup capture of the optimized build reached UI acceptance without panic or reset: internal free 51,447 bytes, largest internal block 31,744 bytes and UI stack high-water free 17,864 bytes. The panel remained at 80 MHz. These are startup headroom measurements; exercise all pages, active communications and updates when measuring worst-case runtime minima.</p>
<h2 id="esp32-p4-rendering">ESP32-P4 rendering<a class="heading-anchor" href="#esp32-p4-rendering" aria-label="Link to ESP32-P4 rendering" title="Link to ESP32-P4 rendering"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>The GUITION target uses a persistent logical RGB565 canvas and two full-screen DSI scanout buffers in 200 MHz PSRAM. At 480 × 800, those three buffers occupy 2,304,000 bytes. Slint renders into the logical canvas, then PPA rotates/copies it into the inactive native scanout buffer. Cache maintenance and display refresh completion protect buffer ownership before reuse. The PPA path handles all four rotations without changing the ESP32-S3 scanline implementation.</p>
<p>The P4 profile uses the supported 360 MHz CPU setting for the selected pre-v3 silicon revision. These are configured capabilities and build checks; physical P4 timing, cache coherence, tearing, touch alignment and concurrent C6 radio operation still need acceptance on the board.</p>
<h2 id="pin-verification-memory-headroom">PIN verification memory headroom<a class="heading-anchor" href="#pin-verification-memory-headroom" aria-label="Link to PIN verification memory headroom" title="Link to PIN verification memory headroom"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>A boot-PIN crash was traced against the matching ELF to lazy allocation of the hardware SHA mutex (<code>lock_init_generic</code> -&gt; <code>esp_crypto_sha_aes_lock_acquire</code> -&gt; <code>psa_hash_compute</code>). The affected UI retained only 1,291 bytes of internal heap, with a 640-byte largest block after its first frame. Small UI allocations had consumed the remaining internal memory.</p>
<p>The common malloc threshold remains 512 bytes, preserving the fast path for small UI allocations. The battery text halo now uses fixed glyph layers rather than allocating 32 dynamic repeater item trees for its two labels; its offsets, opacity and soft appearance are unchanged. Security initialization exercises SHA before the first UI frame, allocating its hardware lock early. Recoverable crypto errors leave the PIN screen locked and report a retry message without recording an incorrect attempt or changing the stored PIN. The stored hash format is unchanged. Task stacks, FreeRTOS control blocks and explicitly internal DMA buffers retain their allocation policy. On the connected S3, the PIN-screen startup sample retained 2,743 bytes internal free (largest block 1,408 bytes), compared with 1,291 bytes (largest 640 bytes) in the crashing image. The flashed image logged a successful screen unlock, and the user confirmed that pressing OK returned to the dashboard without a reboot. Internal headroom on this screen remains tight; this capture establishes the unlock fix, not sustained radio/TLS memory margins or a rendering-speed comparison.</p>
<p>The subsequent PIN-screen redesign retains the 512-byte threshold and uses fixed keypad rows instead of nested repeaters. A startup sample of that flashed S3 image retained 41,711 bytes of internal heap, with a 10,752-byte largest block and 17,632 bytes of free UI stack at its high-water mark. The redesigned screen shows visible digits and automatically submits the sixth accepted digit. The user confirmed the layout and automatic unlock on the connected display, matching the successful unlock in the serial capture. These startup samples compare different PIN layouts and do not establish rendering-speed or sustained radio-load performance.</p>
<p>The final backspace/Clear icon build retained 14,151 bytes internal free (largest block 7,680 bytes) at its first-frame startup sample, with 17,920 bytes of free UI stack at the high-water mark. Its serial capture also recorded a successful automatic unlock. The earlier 41,711-byte sample followed 13 rendered frames, so these samples are not equivalent first-frame measurements.</p>
<h2 id="pattern-rendering-stack-headroom">Pattern rendering stack headroom<a class="heading-anchor" href="#pattern-rendering-stack-headroom" aria-label="Link to Pattern rendering stack headroom" title="Link to Pattern rendering stack headroom"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>The pattern-entry renderer uses pre-rendered sprite assets in place of live path geometry. On the connected Waveshare S3, a capture after unlocking measured <strong>17,216 bytes free of the 32,768-byte UI task stack</strong> at its high-water mark. The fix does not increase task-stack sizes or global allocations. This measurement covers the observed unlock workload; it is not a worst-case guarantee for every screen or concurrent radio and update activity.</p>
<h2 id="measured-ble-telemetry-throughput">Measured BLE telemetry throughput<a class="heading-anchor" href="#measured-ble-telemetry-throughput" aria-label="Link to Measured BLE telemetry throughput" title="Link to Measured BLE telemetry throughput"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>On 7 November 2026 (local time), the Waveshare 2.06 original ESP32-S3 display ran six completed 12-second read-only stages against an actual wheel-e BLE module. The normal Ride UI remained active, the phone was disconnected, and the panel ran at 80 MHz. This build used ESP-IDF 6.1 and Slint <code>mcu-v1.19.5</code>. Standard command 47 replies contained 70 payload bytes; wheel-e command 36/101/33 replies contained 28. ATT MTU was 23 (20 usable bytes). The negotiated connection interval was 31.25 ms, latency zero and supervision timeout 4 seconds, after an initial 50 ms / 2.56-second configuration.</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Offered rate per family</th>
<th>Aggregate offered requests/s</th>
<th>Received replies/s</th>
<th>Mean RTT (ms)</th>
<th>p95 RTT bucket upper bound (ms)</th>
</tr>
</thead>
<tbody>
<tr>
<td>5 Hz</td>
<td>10</td>
<td>9.74</td>
<td>83.1</td>
<td>160</td>
</tr>
<tr>
<td>10 Hz</td>
<td>20</td>
<td>16.42</td>
<td>81.3</td>
<td>130</td>
</tr>
<tr>
<td>15 Hz</td>
<td>30</td>
<td>18.67</td>
<td>80.0</td>
<td>130</td>
</tr>
<tr>
<td>20 Hz</td>
<td>40</td>
<td>19.91</td>
<td>78.1</td>
<td>130</td>
</tr>
<tr>
<td>30 Hz</td>
<td>60</td>
<td>23.91</td>
<td>67.8</td>
<td>110</td>
</tr>
<tr>
<td>40 Hz</td>
<td>80</td>
<td>28.82</td>
<td>60.3</td>
<td>100</td>
</tr>
</tbody>
</table></div>
<p>All six completed stages recorded zero queue failures and zero timeouts. There was at most one pending request per family, without queue flooding. Zero to two requests still in flight at a stage boundary are not packet-loss counts. The peak received application payload was 1,419 bytes/s. Reply counts describe valid framed VESC payload messages, not BLE fragments or decoder-accepted telemetry. An earlier profile aborted on a mode change, followed by a phone-session command-47 timeout; those incomplete stages are excluded from the table.</p>
<p>The shared policy in <code>bike/polling.hpp</code> now targets 10 Hz per telemetry family: up to 20 setup-plus-integration requests/s, plus the initial Ready BMS probe and 1 Hz BMS polling while Stats is open. Per-family credits bound outstanding work, and due requests retry until accepted. The target leaves headroom below this link's observed peak of about 29 aggregate replies/s; it does not guarantee 20 delivered replies/s. This is one BLE module and connection configuration, not a universal capacity figure. WFB, real BMS hardware and concurrent phone-proxy throughput have not been validated by this measurement.</p>
<p>The capture also exposed a reported battery charge of 411.2%. Decoding now preserves the other valid setup fields while marking battery charge invalid (<code>battery_valid=false</code>), so the dashboard shows <code>--</code>. Invalid negative optional Wh values are likewise unavailable without discarding the whole reply.</p>
<h3 id="connection-interval-audit">Connection interval audit</h3>
<p>The central currently passes default connection parameters to NimBLE. In the installed ESP-IDF 6.1 host, <code>BLE_GAP_INITIAL_CONN_ITVL_MIN/MAX</code> are 30/50 ms; the measured initial 50 ms interval therefore follows those defaults. The later 31.25 ms peer update is accepted without an application override.</p>
<p>A best-effort 7.5–15 ms request is technically possible with <code>ble_gap_update_params</code>: interval units are 1.25 ms (values 6–12), with zero latency and an unchanged valid supervision timeout. Whether this module accepts it, and whether delivered throughput improves with both BLE roles active, require separate hardware measurement. The current firmware retains its existing negotiation and 10 Hz telemetry target while the byte pipeline and handoff are validated at the measured 31.25 ms interval.</p>
<p><a href="https://github.com/vedderb/vesc_tool/blob/master/bleuart.cpp#L324-L335">VESC Tool's 7.5 ms update example</a> is commented out; it is not an active recommendation. <a href="https://github.com/vedderb/vesc_express/blob/main/main/comm_ble.c#L109-L127">Current VESC Express</a> sets advertising intervals there, not connected intervals, and does not request a connection update in that file. Any later faster-interval experiment should make one request per physical session, log the actual result, tolerate refusal and accept later peer overrides without a negotiation loop.</p>
<h3 id="reproduce-the-diagnostic">Reproduce the diagnostic</h3>
<p>The CMake option <code>WFD_BLE_BENCHMARK</code> is off in the common PlatformIO configuration. For a local diagnostic build only, change that option to <code>ON</code>, keeping all other common CMake arguments, then build and upload the exact target:</p>
<pre><code class="language-sh">pio run -e waveshare_esp32s3_206 -t upload
pio device monitor -e waveshare_esp32s3_206
</code></pre>
<p>Connect the vehicle, leave the phone disconnected for the solo profile and capture the six completed <code>ble_benchmark: RESULT</code> records. Record the module, MTU, connection interval, firmware and active UI workload. Share redacted summaries without raw BLE addresses or PINs. Restore <code>WFD_BLE_BENCHMARK=OFF</code>, rebuild and reflash normal firmware after testing; do not publish diagnostic images. The local evidence file is <code>.cache/ble-benchmark-shared-device.log</code>, which is not a distributed artifact.</p>
<h2 id="locked-pattern-diagnostic-memory">Locked pattern diagnostic memory<a class="heading-anchor" href="#locked-pattern-diagnostic-memory" aria-label="Link to Locked pattern diagnostic memory" title="Link to Locked pattern diagnostic memory"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>The later locked-pattern diagnostic capture reached UI readiness at 11,469 ms with <strong>44,423 bytes internal free</strong>, a <strong>31,744-byte largest internal block</strong> and <strong>17,328 bytes free of the 32 KiB UI stack</strong>. This sample preceded local unlock at 161,832 ms; it is not an after-unlock measurement. The earlier locked capture retained only 1,447 bytes internal free with a 448-byte largest block.</p>
<p>The fix replaces dynamic pattern item trees with nine static nodes and five segments, caches selection state and drops covered page children while the screen is locked and the credential dialog is open. Twelve rendered fixtures remained pixel-identical for the pattern rewrite. The 32 KiB UI task stack and 512-byte malloc threshold are unchanged. These are measured locked diagnostic idle captures, not a continuous radio/TLS soak. Local evidence is <code>.cache/ble-benchmark-fair-device.log</code> and <code>.cache/ble-benchmark-shared-device.log</code>; neither is a distributed artifact.</p>
<p>The user confirmed that other wheel-e Stats values remain visible after the per-field handling of the reported 411.2% battery value. This establishes that UI behavior on the connected module; phone interoperability is still awaiting validation.</p>
<h2 id="relay-buffering-and-handoff-checks">Relay buffering and handoff checks<a class="heading-anchor" href="#relay-buffering-and-handoff-checks" aria-label="Link to Relay buffering and handoff checks" title="Link to Relay buffering and handoff checks"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>The relay now uses three 16 KiB PSRAM byte rings rather than per-fragment relay queues. Complete phone frames are CRC-validated before forwarding, then coalesced into up to 512-byte batches with reply-key and firmware-transfer conflict guards. The phone frame cap is 16 KiB including framing. Nonblocking notification service separates phone-output pressure from phone-to-bike progress.</p>
<p>The ten passing host suites include a 12,000-byte startup burst (600 writes of 20 bytes), coalesced into 24 batches, complete-frame validation and resource/ownership handoff checks. Normal phone departure finishes admitted valid frames and resumes after backend drain and quiet receive, retaining the bike BLE session and route. These tests establish software behavior, not successful phone-app interoperability on hardware; that verification remains pending.</p>
`,headings:[{id:`immutable-font-metadata`,title:`Immutable font metadata`,level:2},{id:`esp32-p4-rendering`,title:`ESP32-P4 rendering`,level:2},{id:`pin-verification-memory-headroom`,title:`PIN verification memory headroom`,level:2},{id:`pattern-rendering-stack-headroom`,title:`Pattern rendering stack headroom`,level:2},{id:`measured-ble-telemetry-throughput`,title:`Measured BLE telemetry throughput`,level:2},{id:`connection-interval-audit`,title:`Connection interval audit`,level:3},{id:`reproduce-the-diagnostic`,title:`Reproduce the diagnostic`,level:3},{id:`locked-pattern-diagnostic-memory`,title:`Locked pattern diagnostic memory`,level:2},{id:`relay-buffering-and-handoff-checks`,title:`Relay buffering and handoff checks`,level:2}],search:`This records the selected configuration and local measurements for the 2.06-inch display. Startup work timings and memory samples do not establish a sustained frame rate. Setting Selected value Reason CPU 240 MHz ESP32-S3 performance clock UI task stack 32 KiB internal SRAM Keep layout/rendering headroom and permit flash/NVS operations with caches disabled C/C++ optimization Performance (-O2) Retain assertions and checks Flash QIO, 80 MHz, 32 MB Supported target configuration; verify cold boot on this board PSRAM Octal, 80 MHz Board has 8 MB; avoid experimental clock settings Instruction fetch from PSRAM Enabled Execute application instructions from external RAM Read-only data / full XiP helper Disabled Preserve external heap instead of duplicating read-only data Instruction cache 32 KB, 32-byte lines Supported maximum; 64-byte instruction lines do not exist here Data cache 32 KB, 64-byte lines Retain internal heap rather than enlarging cache to 64 KB Always-internal allocation threshold 512 bytes Larger ordinary allocations prefer PSRAM Internal reserve 32 KB Leave internal-only allocations available NimBLE host allocations External PSRAM Preserve internal RAM for controller, driver and task requirements NVS page/key cache PSRAM Persistent bond/settings lookups must not consume DMA/stack headroom Large transport state Explicit PSRAM allocations Keep reassembly, decoded-packet rings and controller discovery outside internal heap Wi-Fi/LwIP allocation preference PSRAM preferred CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP Wi-Fi DMA/cache pools RX 4, TX 8, TX cache 16 Explicit limits; PSRAM mode otherwise defaults to 16 RX + 16 TX DMA buffers Wi-Fi RX block-ack window 4 Keep the window within the four static RX buffers, per IDF Kconfig guidance TLS heap External PSRAM TLS buffers must not exhaust internal DMA/task memory PSRAM test, assertions, watchdogs Retained No check is disabled for speed Slint mcu-v1.19.5 Hardened bitmap/geometry caches and direct scanline rendering Panel QSPI 80 MHz Hardware-profile clock supplied by DISP_QSPI_HZ scripts/check_sdkconfig.py checks the generated configuration so silent Kconfig fallback is detected. ESP-IDF's derived image/header flash mode can say dio while the bootloader's selected CONFIG_ESPTOOLPY_FLASHMODE_QIO=y enables QIO after initial boot; audit the choice symbol as well as the runtime boot log. The UI embeds bitmap Roboto fonts, limiting large-font glyphs to numeric characters. The header displays connection status beside the Settings cog. On ESP32-S3, dirty regions and three internal scanline buffers avoid full-framebuffer rasterization into PSRAM. The backend now incorporates the applicable transfer changes described below: Each dirty rectangle accumulates independently, avoiding a flush for every interleaved scanline. Narrow dirty spans use the full existing byte allocation, with an even row capacity calculated from span width. Full-width transfers retain ten rows; no additional DMA memory is allocated. A submitted color transfer overlaps rasterization into another buffer. Buffer reuse, address-window commands, brightness, rotation and frame completion wait for DMA ownership to return, with a bounded timeout. An uncompleted transfer is a fatal error rather than permission to overwrite its buffer. Rendering and transfer waits count towards the event-loop budget. Active touch/animation targets approximately 60 Hz; idle input polling retains a 10 ms budget. Every iteration still yields at least one FreeRTOS tick, preserving idle-task watchdog service. Telemetry now targets 10 Hz per family (up to 20 setup-plus-integration requests/s), subject to reply credits and phone arbitration; UI data projection targets 10 Hz. Held contacts dispatch pointer moves only when coordinates change. About > Diagnostics exposes rendered frame count, exponentially averaged frame work and DMA wait, maximum frame work, and average panel transactions. These are work timings, not a delivered FPS measurement. Measure delivered frames over wall-clock time during the same scrolling workload. The portable scanline test exercises the production accumulator with 2,000 randomized interleaved-region cases and native-size cases, checking exact pixel coverage, even windows, allocation canaries, delayed-DMA buffer immutability and batching. AddressSanitizer and UndefinedBehaviorSanitizer are enabled in the Linux host run. The hardware profile supplies the 80 MHz panel clock. This halves theoretical full-screen wire time versus the driver's 40 MHz reference default (about 10.3 ms versus 20.6 ms for 410x502 RGB565, excluding command overhead). It does not by itself double delivered FPS. Address-window caching is not implemented. Existing QIO80 flash, octal80 PSRAM, instruction fetch from PSRAM and pinned renderer caches are already enabled; read-only data and full XiP remain disabled. The connected board has passed cold-boot memory initialization, instruction copying and the PSRAM test, and the user confirmed its dashboard and touch. Radio-load scrolling, corner alignment, extended heap/stack minima and a matched before/after delivered-FPS benchmark still require hardware validation. Large task-only BLE packet queue payloads are explicitly allocated in PSRAM with internal FreeRTOS control blocks. Dynamic FreeRTOS queue creation otherwise consumes internal RAM despite the ordinary malloc threshold; with both BLE roles enabled, that exhausted space for the 12 KiB maintenance-task stack. DMA buffers and all task stacks retain internal allocations. A quiet landscape boot with Wi-Fi PSRAM enabled but unrestricted IDF buffer defaults retained only about 1.1 KiB internal free. A 128-byte malloc-threshold trial did not improve that earlier baseline. The cause was IDF raising the static Wi-Fi pools to 16 RX and 16 TX buffers when PSRAM mode was enabled. Explicit RX4/TX8/cache16 limits avoid that hidden increase. TLS heap allocation also explicitly uses PSRAM. With RX4/TX8/cache16, TLS/NimBLE external heaps and the original 512-byte threshold, the final quiet boot retained 30,203 bytes internal free and a 15,872-byte largest block, compared with 1,139 bytes free in the earlier unrestricted-pool quiet boot. Both captures used the saved landscape layout. This establishes startup headroom, not an extended radio/TLS soak or FPS comparison. With persistent SMP bonds enabled, the first integrated transport build had only 6,383 bytes internal free at UI acceptance. Explicit PSRAM state allocation plus CONFIG_NVS_ALLOCATE_CACHE_IN_SPIRAM=y raised this to 19,147 bytes (largest block 7,424 bytes) in the final 24-second startup capture. That capture used the earlier 512-byte allocation threshold, internal task stacks, three DMA scanline buffers and 80 MHz panel clock. That earlier 16 KiB UI stack retained only 1,640 bytes at its startup high-water mark. The current UI task uses a 32 KiB internal stack; this capacity is audited in the generated SDK configuration. Startup and all-screen high`};export{e as default};