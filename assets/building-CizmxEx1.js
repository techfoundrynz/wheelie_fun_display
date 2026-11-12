var e={source:`docs/building.md`,route:`building`,title:`Build & install`,group:`For developers`,description:`Set up PlatformIO, install firmware and run the shared simulators.`,body:`<p>The firmware uses ESP-IDF through PlatformIO and a pinned Slint compiler/runtime pair. Firmware builds use matching prebuilt MCU compiler/runtime assets. Desktop/browser simulators need Rust. Both supported targets share the pinned platform and SDK in the common PlatformIO configuration.</p>
<h2 id="supported-hardware">Supported hardware<a class="heading-anchor" href="#supported-hardware" aria-label="Link to Supported hardware" title="Link to Supported hardware"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Environment</th>
<th>Native resolution</th>
<th>Hardware</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>waveshare_esp32s3_206</code></td>
<td>410 × 502</td>
<td>Waveshare ESP32-S3 Touch AMOLED 2.06, CO5300 / FT3168, 32 MB flash</td>
</tr>
<tr>
<td><code>guition_jc4880p443c</code></td>
<td>480 × 800</td>
<td>GUITION JC4880P443C, ST7701S / GT911, 16 MB flash, P4 + C6</td>
</tr>
</tbody>
</table></div>
<p>See the <a href="../hardware/waveshare-206/">Waveshare S3 2.06-inch setup guide</a> for its board wiring and USB recovery. The <a href="../hardware/guition-43/">GUITION P4 4.3-inch target</a> requires production ESP32-P4 revision 3.1 or newer and compatible C6 companion firmware. Both targets are included in <code>default_envs</code>.</p>
<p><a href="https://github.com/techfoundrynz/wheelie_fun_display/blob/main/platformio.ini">PlatformIO configuration</a> is the source of truth. Do not flash a different Waveshare size or revision with this target merely because it also uses an ESP32-S3.</p>
<h2 id="prepare-the-tools">Prepare the tools<a class="heading-anchor" href="#prepare-the-tools" aria-label="Link to Prepare the tools" title="Link to Prepare the tools"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>Install Python and PlatformIO Core, then use a terminal at the repository root. CI pins PlatformIO Core 6.2.0.</p>
<pre><code class="language-sh">python -m pip install platformio==6.2.0
pio run
python scripts/check_sdkconfig.py --all
</code></pre>
<p><code>pio run</code> builds all supported <code>default_envs</code>. The project pins Espressif platform 8.0.0 / ESP-IDF 6.1 in the common environment and a matching pinned Slint compiler/runtime pair. The Slint compiler and runtime are downloaded together with checksum verification.</p>
<p>Slint generation uses a content fingerprint of the UI/resources, compiler, font-preparation script and font plan/options. Unchanged inputs reuse the matching generated header/source pair: glyph ordering can vary between compiler runs, so unnecessary regeneration otherwise causes spurious changes and recompilation. Missing outputs or changed inputs regenerate the pair; the fingerprint is committed only after generation and font preparation succeed. Build checks cover preservation of both output hashes for unchanged inputs.</p>
<p>Each application image is <code>.pio/build/&lt;environment&gt;/firmware.bin</code>. The ESP project identity is derived as <code>wfd_&lt;hardware-id&gt;</code>; release filenames use the PlatformIO environment name.</p>
<p>Build firmware environments sequentially: IDF shares managed components between targets. P4 builds apply <code>-mno-relax</code> to the generated Slint source to avoid slow linking; see <a href="../performance/#p4-link-time-on-windows">performance notes</a>.</p>
<h2 id="first-usb-installation">First USB installation<a class="heading-anchor" href="#first-usb-installation" aria-label="Link to First USB installation" title="Link to First USB installation"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>For a published release, open the <a href="../install/">browser installer</a>, choose the exact display and connect with a USB data cable. The installer checks the chip family and flashes a complete matching image. It can reset saved settings; use OTA for normal updates. The P4 installer does not provision the C6 companion.</p>
<p>When uploading a local build with PlatformIO, discover the port rather than copying someone else's COM number:</p>
<pre><code class="language-sh">pio device list
pio run -e waveshare_esp32s3_206 -t upload --upload-port YOUR_PORT
pio device monitor --port YOUR_PORT
</code></pre>
<p>Replace <code>YOUR_PORT</code> with the detected port, such as <code>COM9</code> on Windows or <code>/dev/ttyACM0</code> on Linux. If the board is not detected, use a known USB data cable and enter the board's download mode with BOOT/RESET as described by its manufacturer.</p>
<p>Do not combine <code>-t nobuild</code> with <code>-t upload</code>. That PlatformIO path bypasses ESP-IDF's partition-derived upload settings and can write the application at the default address instead of this project's OTA slot. Use the normal target upload command above, even when an image was already built.</p>
<p>The first installation must include the matching bootloader, partition table, OTA initialization data and application. PlatformIO upload uses the configured target layout. Do not upload only an application image to a factory partition layout. After installation, verify the dashboard, touch, all four orientations and the boot log before connecting a bike.</p>
<h2 id="configure-an-optional-wired-interface">Configure an optional wired interface<a class="heading-anchor" href="#configure-an-optional-wired-interface" aria-label="Link to Configure an optional wired interface" title="Link to Configure an optional wired interface"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>BLE is the default. CAN and UART are available only when their pin pairs are defined in the target's <code>build_flags</code>. Choose unused, exposed GPIOs from the actual board schematic; the repository does not guess wiring.</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Build flag</th>
<th>Default / requirement</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>CAN_TX</code>, <code>CAN_RX</code></td>
<td>Both required to enable CAN. Connect through an external transceiver.</td>
</tr>
<tr>
<td><code>CAN_BITRATE</code></td>
<td><code>500000</code>; supported values 125000, 250000, 500000 and 1000000.</td>
</tr>
<tr>
<td><code>CAN_LOCAL_ID</code></td>
<td><code>254</code>; must be unique on the vehicle CAN bus.</td>
</tr>
<tr>
<td><code>UART_TX</code>, <code>UART_RX</code></td>
<td>Both required to enable UART.</td>
</tr>
<tr>
<td><code>UART_BAUD</code></td>
<td><code>115200</code>; must match the vehicle.</td>
</tr>
<tr>
<td><code>BIKE_UART_PORT</code></td>
<td><code>1</code>; ports 1 and 2 are supported, not the console port 0.</td>
</tr>
</tbody>
</table></div>
<p>Add the chosen <code>-D</code> definitions to the hardware profile and rebuild. The interfaces then appear in Settings. If a saved interface is unavailable in the current build, the display falls back to BLE. The main motor controller is found through discovery rather than a target CAN-ID build flag.</p>
<h2 id="editor-and-live-preview">Editor and live preview<a class="heading-anchor" href="#editor-and-live-preview" aria-label="Link to Editor and live preview" title="Link to Editor and live preview"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<pre><code class="language-sh">python scripts/sync_slint_lsp.py
</code></pre>
<p>This downloads the pinned language server/live preview and writes the machine-specific path to ignored VS Code settings. Reload the editor after initial setup. The tracked shared settings remain portable; do not commit generated absolute tool paths.</p>
<h2 id="desktop-simulator">Desktop simulator<a class="heading-anchor" href="#desktop-simulator" aria-label="Link to Desktop simulator" title="Link to Desktop simulator"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>Install a Rust toolchain, then run:</p>
<pre><code class="language-sh">cargo run --locked --manifest-path simulator/Cargo.toml
cargo run --locked --manifest-path simulator/Cargo.toml -- 502 410
cargo run --locked --manifest-path simulator/Cargo.toml -- --wired
cargo run --locked --manifest-path simulator/Cargo.toml -- --smoke-test
</code></pre>
<p>The <code>--wired</code> demo exposes simulated CAN/UART settings. Simulators use fake bike/Wi-Fi data and never drive hardware or download firmware. The <a href="../simulator/">browser simulator</a> runs the actual shared Slint UI and software renderer in WebAssembly. Application services remain browser-only mocks: connections, Wi-Fi, OTA, PIN validation and pairing do not reach hardware. Demo PINs/preferences reset on reload; the demo upstream pairing PIN is <code>123456</code>. Serve the generated site over HTTP rather than opening it with <code>file://</code>.</p>
<h2 id="host-checks">Host checks<a class="heading-anchor" href="#host-checks" aria-label="Link to Host checks" title="Link to Host checks"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>On Linux, or in WSL with a C++ compiler, CMake and Ninja:</p>
<pre><code class="language-sh">cmake -S tests -B .cache/host-tests -G Ninja -DBIKE_SANITIZERS=ON
cmake --build .cache/host-tests
ctest --test-dir .cache/host-tests --output-on-failure
</code></pre>
<p>On Windows, use an installed compiler generator and omit sanitizer flags if unsupported. Headless UI checks use the shared Slint screens:</p>
<pre><code class="language-sh">cargo run --locked --manifest-path simulator/Cargo.toml --bin transition-checks -- 410 502
cargo run --locked --manifest-path simulator/Cargo.toml --bin transition-checks -- 502 410
</code></pre>
<p>See <a href="../contributing/">contributing</a> for choosing checks appropriate to a change and <a href="../site-maintenance/">release maintenance</a> for release packaging.</p>
`,headings:[{id:`supported-hardware`,title:`Supported hardware`,level:2},{id:`prepare-the-tools`,title:`Prepare the tools`,level:2},{id:`first-usb-installation`,title:`First USB installation`,level:2},{id:`configure-an-optional-wired-interface`,title:`Configure an optional wired interface`,level:2},{id:`editor-and-live-preview`,title:`Editor and live preview`,level:2},{id:`desktop-simulator`,title:`Desktop simulator`,level:2},{id:`host-checks`,title:`Host checks`,level:2}],search:`The firmware uses ESP-IDF through PlatformIO and a pinned Slint compiler/runtime pair. Firmware builds use matching prebuilt MCU compiler/runtime assets. Desktop/browser simulators need Rust. Both supported targets share the pinned platform and SDK in the common PlatformIO configuration. Supported hardware Environment Native resolution Hardware waveshare_esp32s3_206 410 × 502 Waveshare ESP32-S3 Touch AMOLED 2.06, CO5300 / FT3168, 32 MB flash guition_jc4880p443c 480 × 800 GUITION JC4880P443C, ST7701S / GT911, 16 MB flash, P4 + C6 See the Waveshare S3 2.06-inch setup guide for its board wiring and USB recovery. The GUITION P4 4.3-inch target requires production ESP32-P4 revision 3.1 or newer and compatible C6 companion firmware. Both targets are included in default_envs. PlatformIO configuration is the source of truth. Do not flash a different Waveshare size or revision with this target merely because it also uses an ESP32-S3. Prepare the tools Install Python and PlatformIO Core, then use a terminal at the repository root. CI pins PlatformIO Core 6.2.0. python -m pip install platformio==6.2.0 pio run python scripts/check_sdkconfig.py --all pio run builds all supported default_envs. The project pins Espressif platform 8.0.0 / ESP-IDF 6.1 in the common environment and a matching pinned Slint compiler/runtime pair. The Slint compiler and runtime are downloaded together with checksum verification. Slint generation uses a content fingerprint of the UI/resources, compiler, font-preparation script and font plan/options. Unchanged inputs reuse the matching generated header/source pair: glyph ordering can vary between compiler runs, so unnecessary regeneration otherwise causes spurious changes and recompilation. Missing outputs or changed inputs regenerate the pair; the fingerprint is committed only after generation and font preparation succeed. Build checks cover preservation of both output hashes for unchanged inputs. Each application image is .pio/build/<environment>/firmware.bin. The ESP project identity is derived as wfd_<hardware-id>; release filenames use the PlatformIO environment name. Build firmware environments sequentially: IDF shares managed components between targets. P4 builds apply -mno-relax to the generated Slint source to avoid slow linking; see performance notes. First USB installation For a published release, open the browser installer, choose the exact display and connect with a USB data cable. The installer checks the chip family and flashes a complete matching image. It can reset saved settings; use OTA for normal updates. The P4 installer does not provision the C6 companion. When uploading a local build with PlatformIO, discover the port rather than copying someone else's COM number: pio device list pio run -e waveshare_esp32s3_206 -t upload --upload-port YOUR_PORT pio device monitor --port YOUR_PORT Replace YOUR_PORT with the detected port, such as COM9 on Windows or /dev/ttyACM0 on Linux. If the board is not detected, use a known USB data cable and enter the board's download mode with BOOT/RESET as described by its manufacturer. Do not combine -t nobuild with -t upload. That PlatformIO path bypasses ESP-IDF's partition-derived upload settings and can write the application at the default address instead of this project's OTA slot. Use the normal target upload command above, even when an image was already built. The first installation must include the matching bootloader, partition table, OTA initialization data and application. PlatformIO upload uses the configured target layout. Do not upload only an application image to a factory partition layout. After installation, verify the dashboard, touch, all four orientations and the boot log before connecting a bike. Configure an optional wired interface BLE is the default. CAN and UART are available only when their pin pairs are defined in the target's build_flags. Choose unused, exposed GPIOs from the actual board schematic; the repository does not guess wiring. Build flag Default / requirement CAN_TX, CAN_RX Both required to enable CAN. Connect through an external transceiver. CAN_BITRATE 500000; supported values 125000, 250000, 500000 and 1000000. CAN_LOCAL_ID 254; must be unique on the vehicle CAN bus. UART_TX, UART_RX Both required to enable UART. UART_BAUD 115200; must match the vehicle. BIKE_UART_PORT 1; ports 1 and 2 are supported, not the console port 0. Add the chosen -D definitions to the hardware profile and rebuild. The interfaces then appear in Settings. If a saved interface is unavailable in the current build, the display falls back to BLE. The main motor controller is found through discovery rather than a target CAN-ID build flag. Editor and live preview python scripts/sync_slint_lsp.py This downloads the pinned language server/live preview and writes the machine-specific path to ignored VS Code settings. Reload the editor after initial setup. The tracked shared settings remain portable; do not commit generated absolute tool paths. Desktop simulator Install a Rust toolchain, then run: cargo run --locked --manifest-path simulator/Cargo.toml cargo run --locked --manifest-path simulator/Cargo.toml -- 502 410 cargo run --locked --manifest-path simulator/Cargo.toml -- --wired cargo run --locked --manifest-path simulator/Cargo.toml -- --smoke-test The --wired demo exposes simulated CAN/UART settings. Simulators use fake bike/Wi-Fi data and never drive hardware or download firmware. The browser simulator runs the actual shared Slint UI and software renderer in WebAssembly. Application services remain browser-only mocks: connections, Wi-Fi, OTA, PIN validation and pairing do not reach hardware. Demo PINs/preferences reset on reload; the demo upstream pairing PIN is 123456. Serve the generated site over HTTP rather than opening it with file://. Host checks On Linux, or in WSL with a C++ compiler, CMake and Ninja: cmake -S tests -B .cache/host-tests -G Ninja -DBIKE_SANITIZERS=ON cmake --build .cache/host-tests ctest --test-dir .cache/host-tests --output-on-failure On Windows, use an installed compiler generator and omit sanitizer flags if unsupported. Headless UI checks use the shared Slint screens: cargo run --locked --manifest-path simulator/Cargo.toml --bin transition-checks -- 410 502 cargo run --locked --manifest-path simulator/Cargo.toml --bin transition-checks -- 502 410 See contributing for choosing checks appropriate to a change and release maintenance for release packaging.`};export{e as default};