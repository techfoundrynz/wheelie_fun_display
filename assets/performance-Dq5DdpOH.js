var e={source:`docs/performance.md`,route:`performance`,title:`Performance`,group:`For developers`,description:`Configuration decisions and the limits of measured results.`,body:`<p>This records board-specific configuration and local measurements. Startup work timings and memory samples do not establish a sustained frame rate.</p>
<h2 id="p4-link-time-on-windows">P4 link time on Windows<a class="heading-anchor" href="#p4-link-time-on-windows" aria-label="Link to P4 link time on Windows" title="Link to P4 link time on Windows"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>The pinned GCC 15.2 RISC-V linker stalls in relaxation on the generated Slint object's roughly 2.1 million symbols (143 MB in the measured build). Replaying the link with relaxation disabled completed in 2.28 seconds; the normal relink was still running after 500 seconds when its inputs changed. This identifies the bottleneck, rather than an end-to-end build speedup.</p>
<p>Only the generated P4 <code>dashboard.cpp</code> uses <code>-mno-relax</code>. Debug information is retained, and the SDK, renderer and runtime keep normal relaxation. A global disable increased executable text by about 167 KiB and is not used.</p>
<p>SDK changes invalidate cached linker scripts and bootloader configuration. Post-link checks enforce the P4 XiP protection boundaries: a stale template previously caused an early instruction-access fault.</p>
<h2 id="s3-runtime-configuration">S3 runtime configuration<a class="heading-anchor" href="#s3-runtime-configuration" aria-label="Link to S3 runtime configuration" title="Link to S3 runtime configuration"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
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
<td>Enabled</td>
<td>Keep application instructions and read-only resources in PSRAM</td>
</tr>
<tr>
<td>Instruction cache</td>
<td>32 KB, 32-byte lines</td>
<td>Supported maximum; 64-byte instruction lines do not exist here</td>
</tr>
<tr>
<td>Data cache</td>
<td>64 KB, 64-byte lines</td>
<td>Modest measured throughput gain; PSRAM allocation policy preserves internal headroom</td>
</tr>
<tr>
<td>Always-internal allocation threshold</td>
<td>0 bytes</td>
<td>Ordinary allocations prefer PSRAM; DMA buffers, task stacks and internal-only driver allocations remain internal</td>
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
<p>The UI embeds bitmap Roboto fonts, limiting large-font glyphs to numeric characters. The header displays connection status beside the Settings cog. On ESP32-S3, ordinary scrolling renders dirty spans directly through three internal scanline buffers. Page transitions use the streamed page compositor described below.</p>
<p>The backend now incorporates the applicable transfer changes described below:</p>
<ul>
<li>Each dirty rectangle accumulates independently, avoiding a flush for every interleaved scanline.</li>
<li>Narrow dirty spans use the full existing byte allocation, with an even row capacity calculated from span width. Each of the three buffers holds twenty native-width rows: 49,200 bytes total.</li>
<li>A submitted color transfer overlaps rasterization into another buffer. Buffer reuse, address-window commands, brightness, rotation and frame completion wait for DMA ownership to return, with a bounded timeout. An uncompleted transfer is a fatal error rather than permission to overwrite its buffer.</li>
<li>Rendering and transfer waits count towards the event-loop budget. Active touch/animation targets approximately 60 Hz; idle input polling retains a 10 ms budget. Every iteration still yields at least one FreeRTOS tick, preserving idle-task watchdog service. Telemetry now targets 10 Hz per family (up to 20 setup-plus-integration requests/s), subject to reply credits and phone arbitration; UI data projection targets 10 Hz.</li>
<li>Held contacts dispatch pointer moves only when coordinates change.</li>
</ul>
<p>About &gt; Diagnostics exposes rendered frame count, exponentially averaged frame work and DMA wait, maximum frame work, and average panel transactions. These are work timings, not a delivered FPS measurement. Measure delivered frames over wall-clock time during the same scrolling workload.</p>
<p>The portable scanline test exercises the production accumulator with 2,000 randomized interleaved-region cases and native-size cases, checking exact pixel coverage, even windows, allocation canaries, delayed-DMA buffer immutability and batching. AddressSanitizer and UndefinedBehaviorSanitizer are enabled in the Linux host run.</p>
<p>The hardware profile supplies the 80 MHz panel clock. This halves theoretical full-screen wire time versus the driver's 40 MHz reference default (about 10.3 ms versus 20.6 ms for 410x502 RGB565, excluding command overhead). It does not by itself double delivered FPS. Address-window caching is not implemented. QIO80 flash, octal80 PSRAM, instruction/read-only XiP from PSRAM and pinned renderer caches are enabled. The clock settings are unchanged by the streamed-renderer optimization.</p>
<p>The connected board has passed memory initialization, instruction/read-only copying and the PSRAM test. The user confirmed dashboard, touch, rotations and transition edges. Matched physical FPS measurements are recorded below; sustained radio-load scrolling and extended heap/stack minima still require hardware validation. The following earlier memory investigations used the configurations stated with each capture, rather than today's allocation threshold.</p>
<p>Large task-only BLE packet queue payloads are explicitly allocated in PSRAM with internal FreeRTOS control blocks. Dynamic FreeRTOS queue creation otherwise consumes internal RAM despite the ordinary malloc threshold; with both BLE roles enabled, that exhausted space for the 12 KiB maintenance-task stack. DMA buffers and all task stacks retain internal allocations.</p>
<p>A quiet landscape boot with Wi-Fi PSRAM enabled but unrestricted IDF buffer defaults retained only about 1.1 KiB internal free. A 128-byte malloc-threshold trial did not improve that earlier baseline. The cause was IDF raising the static Wi-Fi pools to 16 RX and 16 TX buffers when PSRAM mode was enabled. Explicit RX4/TX8/cache16 limits avoid that hidden increase. TLS heap allocation also explicitly uses PSRAM.</p>
<p>With RX4/TX8/cache16, TLS/NimBLE external heaps and the original 512-byte threshold, the final quiet boot retained 30,203 bytes internal free and a 15,872-byte largest block, compared with 1,139 bytes free in the earlier unrestricted-pool quiet boot. Both captures used the saved landscape layout. This establishes startup headroom, not an extended radio/TLS soak or FPS comparison.</p>
<p>With persistent SMP bonds enabled, the first integrated transport build had only 6,383 bytes internal free at UI acceptance. Explicit PSRAM state allocation plus <code>CONFIG_NVS_ALLOCATE_CACHE_IN_SPIRAM=y</code> raised this to 19,147 bytes (largest block 7,424 bytes) in the final 24-second startup capture. That capture used the earlier 512-byte allocation threshold, internal task stacks, three DMA scanline buffers and 80 MHz panel clock. That earlier 16 KiB UI stack retained only 1,640 bytes at its startup high-water mark. The current UI task uses a 32 KiB internal stack; this capacity is audited in the generated SDK configuration. Startup and all-screen high-water measurements must be repeated after layout changes. NVS operations are low-frequency and trade some metadata-access speed for internal driver/stack headroom. Sustained radio/load and all-screen minima still need hardware measurement.</p>
<h2 id="immutable-font-metadata">Immutable font metadata<a class="heading-anchor" href="#immutable-font-metadata" aria-label="Link to Immutable font metadata" title="Link to Immutable font metadata"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>Generated font tables need constant initialization to remain in flash-mapped read-only memory. The pinned compiler emits slice helper calls that otherwise put immutable glyph metadata in internal DRAM. After Slint code generation, <code>scripts/prepare_slint_fonts.py</code> rewrites only recognized static <code>BitmapGlyph</code>, <code>BitmapGlyphs</code> and <code>BitmapFont</code> resources with constexpr slices and <code>constinit const</code>. Unknown initializer forms fail preparation or compilation. Runtime UI expressions and image resources are untouched.</p>
<p>In the measured four-weight baseline, ELF inspection confirmed all 48 font resources (79,072 bytes) reside in flash-mapped memory, with none of this font metadata in internal RAM. Static RAM fell from 128,832 to 49,760 bytes in that baseline. The current font-size set also includes 120 px digits; every build audits the actual linked font resources rather than assuming this historical count or byte total. Font pixels, glyph coverage, metrics and weights are unchanged; this is a placement improvement, not an FPS claim. The UI task keeps its 32 KiB internal stack so flash/NVS operations can safely run when caches are disabled.</p>
<p>A 24-second device startup capture of the optimized build reached UI acceptance without panic or reset: internal free 51,447 bytes, largest internal block 31,744 bytes and UI stack high-water free 17,864 bytes. The panel remained at 80 MHz. These are startup headroom measurements; exercise all pages, active communications and updates when measuring worst-case runtime minima.</p>
<h2 id="esp32-s3-streamed-page-transitions">ESP32-S3 streamed page transitions<a class="heading-anchor" href="#esp32-s3-streamed-page-transitions" aria-label="Link to ESP32-S3 streamed page transitions" title="Link to ESP32-S3 streamed page transitions"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>The S3 renderer retains seven page trees and three native RGB565 images in PSRAM: fixed chrome and two page snapshots, totaling 1,234,920 bytes. Transitions compose rows directly into three internal twenty-row DMA buffers. Ordinary scrolling rasterizes dirty spans into those buffers. This avoids repeatedly laying out and rasterizing two moving pages.</p>
<p>Small updates refresh the stationary snapshot; scrolling invalidates it for recapture before the next slide. During slides, translation follows each rendered input frame; incoming content is rerendered no more than once every 30 ms. Telemetry projection remains 10 Hz, and lock/modal changes take effect immediately. Rotation repaints all 205,820 native pixels before incremental rendering resumes, preventing old controls from remaining in the margins.</p>
<h3 id="s3-measurements">S3 measurements</h3>
<p>Measured on 12 November 2026 with a Waveshare AMOLED 2.06, local firmware 0.1.4 and ESP-IDF 6.1. The baseline used the original scanline renderer, 32 KiB data cache, instruction-only PSRAM copying and a 512-byte malloc threshold. The selected configuration uses a 64 KiB data cache, instruction/read-only XiP and a zero-byte threshold; clock speeds are unchanged.</p>
<p>Each stage ran for 30 seconds after settling, using real UI pointer events without a connected controller or mocked telemetry. FPS counts completed QSPI/DMA frames over wall time; minimum FPS is the lowest complete, non-overlapping one-second bucket.</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Rotation</th>
<th>Slides before, average FPS</th>
<th>Slides after, average FPS</th>
<th>Minimum after</th>
<th>Settings before, average FPS</th>
<th>Settings after, average FPS</th>
<th>Minimum after</th>
</tr>
</thead>
<tbody>
<tr>
<td>0°</td>
<td>22.34</td>
<td>36.69</td>
<td>35.36</td>
<td>29.33</td>
<td>35.34</td>
<td>34.38</td>
</tr>
<tr>
<td>90°</td>
<td>25.82</td>
<td>39.43</td>
<td>37.43</td>
<td>32.85</td>
<td>35.39</td>
<td>34.34</td>
</tr>
<tr>
<td>180°</td>
<td>20.71</td>
<td>35.97</td>
<td>34.28</td>
<td>26.22</td>
<td>33.19</td>
<td>32.10</td>
</tr>
<tr>
<td>270°</td>
<td>25.01</td>
<td>39.49</td>
<td>37.25</td>
<td>32.23</td>
<td>35.48</td>
<td>34.38</td>
</tr>
</tbody>
</table></div>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Rotation</th>
<th>Drags before, average FPS</th>
<th>Drags after, average FPS</th>
<th>Minimum after</th>
<th>Stats before, average FPS</th>
<th>Stats after, average FPS</th>
<th>Minimum after</th>
</tr>
</thead>
<tbody>
<tr>
<td>0°</td>
<td>18.09</td>
<td>36.18</td>
<td>35.36</td>
<td>31.04</td>
<td>33.93</td>
<td>32.77</td>
</tr>
<tr>
<td>90°</td>
<td>19.62</td>
<td>38.43</td>
<td>37.58</td>
<td>31.87</td>
<td>33.71</td>
<td>31.28</td>
</tr>
<tr>
<td>180°</td>
<td>18.06</td>
<td>35.80</td>
<td>34.86</td>
<td>28.20</td>
<td>32.20</td>
<td>30.78</td>
</tr>
<tr>
<td>270°</td>
<td>19.58</td>
<td>38.39</td>
<td>37.62</td>
<td>32.10</td>
<td>33.97</td>
<td>31.65</td>
</tr>
</tbody>
</table></div>
<p>All sixteen stages completed without a reset, abort or idle-motion interval. Frame-interval p95 was 72–75 ms for slides, 52–66 ms for drags, 30–33 ms for Settings and 46–64 ms for Stats. Initial page capture still causes longer frames; these averages do not establish uniform latency or 50 FPS.</p>
<p>Stage-end internal heap stayed at 37,951 bytes free (largest block 19,456), compared with a baseline sample of 39 bytes during page cycling. DMA-capable free memory was 30,163 bytes, PSRAM free was 1.99–2.02 MB, and UI stack high-water free was 12,692 bytes. The boot-inclusive internal-heap minimum was 18,132 bytes. Occasional FT3168 I²C errors persist; measured touch polls stayed below 1 ms. Transactions have a 20 ms timeout.</p>
<p>The display, touch, rotations and transition edges passed physical checks. Normal firmware with profiling disabled passed startup and five controlled USB-serial-JTAG restarts, each retaining 39,103 bytes internal free and 12,948 bytes free UI stack. These checks do not qualify controller/radio load, OTA, temperature or power-cycle reliability.</p>
<p>Local logs: <code>.cache/s3-fps-baseline-validation.log</code>, <code>.cache/s3-fps-final-validation.log</code> and <code>.cache/s3-production-boot.log</code>.</p>
<h2 id="esp32-p4-rendering">ESP32-P4 rendering<a class="heading-anchor" href="#esp32-p4-rendering" aria-label="Link to ESP32-P4 rendering" title="Link to ESP32-P4 rendering"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>The GUITION renderer uses two native RGB565 DSI scanout buffers and two page snapshots in 250 MHz PSRAM (3,072,000 bytes total). Slint renders rotated dirty spans through two internal 32-row tiles sharing 61,440 bytes. PPA copies overlap rasterization; completion signals prevent tile reuse before DMA finishes.</p>
<p>Slint paints each frame's damage once. Before reusing a retired scanout buffer, PPA restores unchanged regions from the latest front buffer. Both buffers are restored after a slide to prevent stale edges on subsequent scrolling. The incoming page stays stationary and live while PPA translates the snapshots; reversal and cancellation preserve the outgoing image.</p>
<p>Both targets retain seven page trees and prepare layouts offscreen after startup, rotation or credential screens. Pointer input releases the stationary capture before hit testing. Stats and Tune provide bounded scroll momentum, stopped by a new contact or cancellation. Page slides include the side margins while header/navigation stay fixed; GUITION corner-safe insets are zero.</p>
<p>The P4 uses 400 MHz CPU, 256 KiB L2 cache with 128-byte lines, instruction/read-only XiP and a 32-byte malloc threshold. Task stacks and DMA buffers remain internal. Watchdogs, assertions and PSRAM testing remain enabled. See Espressif's <a href="https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/kconfig-reference.html#config-spiram-speed">configuration reference</a> and <a href="https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-guides/performance/speed.html">speed guide</a>.</p>
<p>The pinned P4 toolchain needs <code>_GLIBCXX_HAVE_ATOMIC_LOCK_POLICY=1</code> to avoid leaking lazy pthread mutexes through mutex-based shared ownership. The S3 toolchain already uses atomics. Staged Slint headers also prune expired model listeners, and About diagnostics update only changed rows. The P4 SDIO memory pool reserves receive-buffer headroom during page-allocation churn.</p>
<h3 id="p4-measurements">P4 measurements</h3>
<p>Measured on 12 November 2026 with a GUITION P4 v3.2, local firmware 0.1.4 and 250 MHz PSRAM. Each stage ran for 30 seconds after settling, without a connected controller or mocked telemetry. FPS counts completed framebuffer submissions over wall time; minimum FPS is the lowest complete, non-overlapping one-second bucket.</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Rotation</th>
<th>Automatic slides, average FPS</th>
<th>Minimum FPS</th>
<th>Settings scroll, average FPS</th>
<th>Minimum FPS</th>
</tr>
</thead>
<tbody>
<tr>
<td>0°</td>
<td>55.02</td>
<td>53.43</td>
<td>56.20</td>
<td>55.63</td>
</tr>
<tr>
<td>90°</td>
<td>54.27</td>
<td>52.03</td>
<td>55.81</td>
<td>54.65</td>
</tr>
<tr>
<td>180°</td>
<td>54.50</td>
<td>52.07</td>
<td>51.43</td>
<td>50.71</td>
</tr>
<tr>
<td>270°</td>
<td>54.30</td>
<td>52.04</td>
<td>56.93</td>
<td>55.84</td>
</tr>
</tbody>
</table></div>
<p>Frame-interval p95 was 38–42 ms for slides and 24–28 ms for Settings, including initial page capture. The averages do not mean every frame finishes within 20 ms.</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Rotation</th>
<th>Horizontal drags, average FPS</th>
<th>Minimum FPS</th>
<th>Stats flicks, average FPS</th>
<th>Minimum FPS</th>
</tr>
</thead>
<tbody>
<tr>
<td>0°</td>
<td>58.32</td>
<td>57.13</td>
<td>60.38</td>
<td>58.18</td>
</tr>
<tr>
<td>90°</td>
<td>58.53</td>
<td>57.87</td>
<td>58.12</td>
<td>55.39</td>
</tr>
<tr>
<td>180°</td>
<td>57.46</td>
<td>56.48</td>
<td>56.92</td>
<td>54.45</td>
</tr>
<tr>
<td>270°</td>
<td>58.51</td>
<td>57.30</td>
<td>58.07</td>
<td>55.10</td>
</tr>
</tbody>
</table></div>
<p>Stats tests alternate 400 ms drags over 20% of viewport height and 20 ms of release momentum, away from scroll bounds. All orientations recorded zero idle-motion time. Repeated stages used the same renderer and SDK configuration; only diagnostic positioning/logging changed.</p>
<p>Transition/Settings stage-end samples retained 72,403–83,219 bytes internal free (largest block 31,744). The final Stats repeat retained 76,539 bytes internal free and a 25,600-byte largest DMA block. These are samples, not continuous minima.</p>
<p>Physical checks covered display/touch at all four angles, stale-edge removal, reversals, cancellation and flick momentum. Normal firmware passed startup and five controlled USB-serial-JTAG restarts, retaining 94,619–94,739 bytes internal free and 11,400 bytes free UI stack. An earlier startup panic during a rapid flash/reset sequence did not recur; its cause remains unconfirmed. Controller/radio load, OTA, temperature and power-cycle reliability remain unqualified. P4 was build-tested, but not physically reprofiled, during the later S3 changes.</p>
<p>Local logs: <code>.cache/p4-fps-final-full-validation.log</code>, <code>.cache/p4-fps-final-repeat-validation.log</code>, <code>.cache/p4-fps-stats-positioned-validation.log</code> and <code>.cache/p4-production-boot.log</code>.</p>
<h2 id="reproduce-fps-measurements">Reproduce FPS measurements<a class="heading-anchor" href="#reproduce-fps-measurements" aria-label="Link to Reproduce FPS measurements" title="Link to Reproduce FPS measurements"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<ol>
<li>Set <code>WFD_RENDER_PROFILE=1</code> and <code>WFD_RENDER_PROFILE_SECONDS=30</code>, then build and upload the matching board environment. Profiling is disabled by default.</li>
<li>Check the display during the 60-second manual window, then capture all sixteen <code>render_profile: RESULT</code> records. The diagnostic cycles all seven pages using UI pointer events, owns input during measurement and restores preferences without NVS writes. A modal or locked screen aborts it.</li>
<li>To resume, set <code>WFD_RENDER_PROFILE_START_STAGE</code> to the zero-based stage index: 0–7 alternate slides/Settings at 90°, 0°, 270°, 180°; 8–11 test horizontal drags in that order; 12–15 test Stats flicks.</li>
<li>Unset all three variables, rebuild and flash normal firmware. Do not distribute diagnostic images.</li>
</ol>
<p>Results include wall-clock FPS, minimum one-second FPS, frame-interval p95, render/copy/wait timings and memory samples. Counters cannot detect panel artifacts, so inspect the display too. Both targets passed builds and SDK audits; all 22 UI scenarios passed with snapshots enabled and disabled, and all 14 sanitizer-backed host suites passed.</p>
<h2 id="pin-verification-memory-headroom">PIN verification memory headroom<a class="heading-anchor" href="#pin-verification-memory-headroom" aria-label="Link to PIN verification memory headroom" title="Link to PIN verification memory headroom"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>A boot-PIN crash was traced against the matching ELF to lazy allocation of the hardware SHA mutex (<code>lock_init_generic</code> -&gt; <code>esp_crypto_sha_aes_lock_acquire</code> -&gt; <code>psa_hash_compute</code>). The affected UI retained only 1,291 bytes of internal heap, with a 640-byte largest block after its first frame. Small UI allocations had consumed the remaining internal memory.</p>
<p>The malloc threshold in this earlier capture was 512 bytes, preserving the fast path for small UI allocations. The battery text halo now uses fixed glyph layers rather than allocating 32 dynamic repeater item trees for its two labels; its offsets, opacity and soft appearance are unchanged. Security initialization exercises SHA before the first UI frame, allocating its hardware lock early. Recoverable crypto errors leave the PIN screen locked and report a retry message without recording an incorrect attempt or changing the stored PIN. The stored hash format is unchanged. Task stacks, FreeRTOS control blocks and explicitly internal DMA buffers retain their allocation policy. On the connected S3, the PIN-screen startup sample retained 2,743 bytes internal free (largest block 1,408 bytes), compared with 1,291 bytes (largest 640 bytes) in the crashing image. The flashed image logged a successful screen unlock, and the user confirmed that pressing OK returned to the dashboard without a reboot. Internal headroom on this screen remains tight; this capture establishes the unlock fix, not sustained radio/TLS memory margins or a rendering-speed comparison.</p>
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
<p>The capture also exposed a reported battery charge of 411.2%. The display now preserves nonnegative controller-reported percentages, including values above 100%, on Ride and Stats. The Ride battery fill stops at full while its numeric label retains the reported value. Negative battery estimates and negative optional Wh values remain unavailable without discarding the whole reply.</p>
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
<p>The user confirmed that other wheel-e Stats values remain visible after the per-field handling of the reported 411.2% battery value. The earlier handling hid that percentage; the current behavior displays it as reported. Hardware validation of that updated numeric display and phone interoperability remain outstanding.</p>
<h2 id="relay-buffering-and-handoff-checks">Relay buffering and handoff checks<a class="heading-anchor" href="#relay-buffering-and-handoff-checks" aria-label="Link to Relay buffering and handoff checks" title="Link to Relay buffering and handoff checks"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>The relay now uses three 16 KiB PSRAM byte rings rather than per-fragment relay queues. Complete phone frames are CRC-validated before forwarding, then coalesced into up to 512-byte batches with reply-key and firmware-transfer conflict guards. The phone frame cap is 16 KiB including framing. Nonblocking notification service separates phone-output pressure from phone-to-bike progress.</p>
<p>The ten passing host suites include a 12,000-byte startup burst (600 writes of 20 bytes), coalesced into 24 batches, complete-frame validation and resource/ownership handoff checks. Normal phone departure finishes admitted valid frames and resumes after backend drain and quiet receive, retaining the bike BLE session and route. These tests establish software behavior, not successful phone-app interoperability on hardware; that verification remains pending.</p>
<h2 id="guition-wi-fi-configuration-and-dma-headroom">GUITION Wi-Fi configuration and DMA headroom<a class="heading-anchor" href="#guition-wi-fi-configuration-and-dma-headroom" aria-label="Link to GUITION Wi-Fi configuration and DMA headroom" title="Link to GUITION Wi-Fi configuration and DMA headroom"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>A 2026-11-13 capture on the production P4 v3.2 with C6 companion 2.3.2 traced "Wi-Fi configuration rejected" to host DMA exhaustion rather than authentication. The configuration RPC failed before transmission with <code>ESP_FAIL</code> while the radio reported transmit-ready, with only 287 bytes DMA-capable memory free and a 12-byte largest block. Bluetooth writes failed in the same interval. The profile still preferred internal memory for allocations up to 32 bytes, allowing keyboard bindings to consume the packet-buffer headroom.</p>
<p>The P4 now uses <code>CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0</code>, matching the S3's PSRAM-first policy for ordinary allocations. Explicit internal driver buffers and task stacks keep their existing policy. The updated device reached UI readiness with 193,511 bytes internal free and a 151,552-byte largest block, compared with 93,651 and 39,936 bytes in the preceding diagnostic capture. The next Wi-Fi attempt obtained a DHCP address, and the user confirmed connection success. This validates that connection flow with the existing C6 firmware; it is not an OTA, phone-interoperability or long-duration radio-load acceptance test. SDK errors now log transmit readiness and DMA headroom without credentials. Redacted local evidence is in <code>.cache/wifi-dma-exhaustion-before.log</code> and <code>.cache/guition-wifi-errors.log</code>.</p>
`,headings:[{id:`p4-link-time-on-windows`,title:`P4 link time on Windows`,level:2},{id:`s3-runtime-configuration`,title:`S3 runtime configuration`,level:2},{id:`immutable-font-metadata`,title:`Immutable font metadata`,level:2},{id:`esp32-s3-streamed-page-transitions`,title:`ESP32-S3 streamed page transitions`,level:2},{id:`s3-measurements`,title:`S3 measurements`,level:3},{id:`esp32-p4-rendering`,title:`ESP32-P4 rendering`,level:2},{id:`p4-measurements`,title:`P4 measurements`,level:3},{id:`reproduce-fps-measurements`,title:`Reproduce FPS measurements`,level:2},{id:`pin-verification-memory-headroom`,title:`PIN verification memory headroom`,level:2},{id:`pattern-rendering-stack-headroom`,title:`Pattern rendering stack headroom`,level:2},{id:`measured-ble-telemetry-throughput`,title:`Measured BLE telemetry throughput`,level:2},{id:`connection-interval-audit`,title:`Connection interval audit`,level:3},{id:`reproduce-the-diagnostic`,title:`Reproduce the diagnostic`,level:3},{id:`locked-pattern-diagnostic-memory`,title:`Locked pattern diagnostic memory`,level:2},{id:`relay-buffering-and-handoff-checks`,title:`Relay buffering and handoff checks`,level:2},{id:`guition-wi-fi-configuration-and-dma-headroom`,title:`GUITION Wi-Fi configuration and DMA headroom`,level:2}],search:`This records board-specific configuration and local measurements. Startup work timings and memory samples do not establish a sustained frame rate. P4 link time on Windows The pinned GCC 15.2 RISC-V linker stalls in relaxation on the generated Slint object's roughly 2.1 million symbols (143 MB in the measured build). Replaying the link with relaxation disabled completed in 2.28 seconds; the normal relink was still running after 500 seconds when its inputs changed. This identifies the bottleneck, rather than an end-to-end build speedup. Only the generated P4 dashboard.cpp uses -mno-relax. Debug information is retained, and the SDK, renderer and runtime keep normal relaxation. A global disable increased executable text by about 167 KiB and is not used. SDK changes invalidate cached linker scripts and bootloader configuration. Post-link checks enforce the P4 XiP protection boundaries: a stale template previously caused an early instruction-access fault. S3 runtime configuration Setting Selected value Reason CPU 240 MHz ESP32-S3 performance clock UI task stack 32 KiB internal SRAM Keep layout/rendering headroom and permit flash/NVS operations with caches disabled C/C++ optimization Performance (-O2) Retain assertions and checks Flash QIO, 80 MHz, 32 MB Supported target configuration; verify cold boot on this board PSRAM Octal, 80 MHz Board has 8 MB; avoid experimental clock settings Instruction fetch from PSRAM Enabled Execute application instructions from external RAM Read-only data / full XiP helper Enabled Keep application instructions and read-only resources in PSRAM Instruction cache 32 KB, 32-byte lines Supported maximum; 64-byte instruction lines do not exist here Data cache 64 KB, 64-byte lines Modest measured throughput gain; PSRAM allocation policy preserves internal headroom Always-internal allocation threshold 0 bytes Ordinary allocations prefer PSRAM; DMA buffers, task stacks and internal-only driver allocations remain internal Internal reserve 32 KB Leave internal-only allocations available NimBLE host allocations External PSRAM Preserve internal RAM for controller, driver and task requirements NVS page/key cache PSRAM Persistent bond/settings lookups must not consume DMA/stack headroom Large transport state Explicit PSRAM allocations Keep reassembly, decoded-packet rings and controller discovery outside internal heap Wi-Fi/LwIP allocation preference PSRAM preferred CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP Wi-Fi DMA/cache pools RX 4, TX 8, TX cache 16 Explicit limits; PSRAM mode otherwise defaults to 16 RX + 16 TX DMA buffers Wi-Fi RX block-ack window 4 Keep the window within the four static RX buffers, per IDF Kconfig guidance TLS heap External PSRAM TLS buffers must not exhaust internal DMA/task memory PSRAM test, assertions, watchdogs Retained No check is disabled for speed Slint mcu-v1.19.5 Hardened bitmap/geometry caches and direct scanline rendering Panel QSPI 80 MHz Hardware-profile clock supplied by DISP_QSPI_HZ scripts/check_sdkconfig.py checks the generated configuration so silent Kconfig fallback is detected. ESP-IDF's derived image/header flash mode can say dio while the bootloader's selected CONFIG_ESPTOOLPY_FLASHMODE_QIO=y enables QIO after initial boot; audit the choice symbol as well as the runtime boot log. The UI embeds bitmap Roboto fonts, limiting large-font glyphs to numeric characters. The header displays connection status beside the Settings cog. On ESP32-S3, ordinary scrolling renders dirty spans directly through three internal scanline buffers. Page transitions use the streamed page compositor described below. The backend now incorporates the applicable transfer changes described below: Each dirty rectangle accumulates independently, avoiding a flush for every interleaved scanline. Narrow dirty spans use the full existing byte allocation, with an even row capacity calculated from span width. Each of the three buffers holds twenty native-width rows: 49,200 bytes total. A submitted color transfer overlaps rasterization into another buffer. Buffer reuse, address-window commands, brightness, rotation and frame completion wait for DMA ownership to return, with a bounded timeout. An uncompleted transfer is a fatal error rather than permission to overwrite its buffer. Rendering and transfer waits count towards the event-loop budget. Active touch/animation targets approximately 60 Hz; idle input polling retains a 10 ms budget. Every iteration still yields at least one FreeRTOS tick, preserving idle-task watchdog service. Telemetry now targets 10 Hz per family (up to 20 setup-plus-integration requests/s), subject to reply credits and phone arbitration; UI data projection targets 10 Hz. Held contacts dispatch pointer moves only when coordinates change. About > Diagnostics exposes rendered frame count, exponentially averaged frame work and DMA wait, maximum frame work, and average panel transactions. These are work timings, not a delivered FPS measurement. Measure delivered frames over wall-clock time during the same scrolling workload. The portable scanline test exercises the production accumulator with 2,000 randomized interleaved-region cases and native-size cases, checking exact pixel coverage, even windows, allocation canaries, delayed-DMA buffer immutability and batching. AddressSanitizer and UndefinedBehaviorSanitizer are enabled in the Linux host run. The hardware profile supplies the 80 MHz panel clock. This halves theoretical full-screen wire time versus the driver's 40 MHz reference default (about 10.3 ms versus 20.6 ms for 410x502 RGB565, excluding command overhead). It does not by itself double delivered FPS. Address-window caching is not implemented. QIO80 flash, octal80 PSRAM, instruction/read-only XiP from PSRAM and pinned renderer caches are enabled. The clock settings are unchanged by the streamed-renderer optimization. The connected board has passed memory initialization, instruction/read-only copying and the PSRAM test. The user confirmed dashboard, touch, rotations and transition edges. Matched physical FPS measurements are recorded below; sustained radio-load scrolling and extended heap/stack minima still require hardware validation. The following earlier memory investigations used the configurations stated with each capture, rather than today's allocation threshold. Large task-only BLE packet queue payloads are explicitly allocated in PSRAM with internal FreeRTOS control blocks. Dynamic FreeRTOS queue creation otherwise consumes internal RAM despite the ordinary malloc threshold; with both BLE roles enabled, that exhausted space for the 12 KiB maintenance-task stack. DMA buffers and all task stacks retain internal allocations. A quiet landscape boot with Wi-Fi PSRAM enabled but unrestricted IDF buffer defaults retained only about 1.1 KiB internal free. A 128-byte malloc-threshold trial did not improve that earlier baseline. The cause was IDF raising the static Wi-Fi pools to 16 RX and 16 TX buffers when PSRAM mode was enabled.`};export{e as default};