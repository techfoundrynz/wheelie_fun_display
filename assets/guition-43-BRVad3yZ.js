var e={source:`docs/hardware-guition-43.md`,route:`hardware/guition-43`,title:`GUITION P4 4.3-inch`,group:`Hardware`,description:`P4/C6 board setup, companion firmware and hardware requirements.`,body:`<p>The <code>guition_jc4880p443c</code> target is for the GUITION <strong>JC4880P443C_I_W</strong> / enclosed <code>_Y</code> board, described as JC4880P433 in the original article. It has a 480 × 800 ST7701S MIPI-DSI panel, GT911 touch, 16 MB flash, 32 MB PSRAM and an ESP32-C6 radio companion.</p>
<p>The supplied profile selects <strong>ESP32-P4 revisions below 3.0</strong>, including the original v1.3 engineering-sample board. ESP-IDF treats those chips and production v3.x chips as incompatible targets. Do not use this profile on a v3.x board; check the boot log or vendor revision before installation.</p>
<h2 id="install-the-firmware">Install the firmware<a class="heading-anchor" href="#install-the-firmware" aria-label="Link to Install the firmware" title="Link to Install the firmware"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<ol>
<li>Confirm your P4 revision is <strong>below 3.0</strong> using the boot log or vendor information. This profile cannot be used on a v3.x board.</li>
<li>Check that the C6 has compatible radio firmware; see <a href="#esp32-c6-radio-firmware">ESP32-C6 radio firmware</a> below.</li>
<li>Open the <a href="../../install/">browser installer</a>, choose <strong>GUITION JC4880P443C</strong> and connect to the P4 with a USB data cable.</li>
<li>Complete installation, then check the screen and touch before <a href="../../getting-started/">connecting your bike</a>.</li>
</ol>
<p>The installer flashes the P4 application and its flash layout. It does not provision or replace the C6 firmware. A full USB installation can reset saved settings; use <a href="../../maintenance/">on-device updates</a> for routine maintenance.</p>
<p>The supplied profile starts in 90° landscape (800 × 480). Tap <strong>Settings &gt; Orientation</strong> to cycle through all four angles; the choice is saved and touch rotates with the screen.</p>
<h2 id="large-screen-layout">Large-screen layout<a class="heading-anchor" href="#large-screen-layout" aria-label="Link to Large-screen layout" title="Link to Large-screen layout"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>Landscape Ride uses a full-width battery gauge at the top, then a larger speed dial on the left and trip and odometer above the four telemetry cards on the right. In portrait, telemetry cards and distance readings are larger. Stats groups its overview into three columns when the screen is wide; controller and BMS details remain scrollable.</p>
<p><img src="../../assets/images/ride-guition-landscape.png" alt="GUITION landscape Ride layout"></p>
<p><img src="../../assets/images/ride-guition-portrait.png" alt="GUITION portrait Ride layout"></p>
<p><img src="../../assets/images/stats-guition-landscape.png" alt="GUITION landscape Stats overview"></p>
<p>These previews are generated from the current shared UI with illustrative connected values during the Pages build. In the <a href="../../simulator/">browser simulator</a>, select <strong>GUITION 4.3-inch</strong> under <strong>Display model</strong>, then use Rotate to explore all four orientations.</p>
<h2 id="case-and-handlebar-mount">Case and handlebar mount<a class="heading-anchor" href="#case-and-handlebar-mount" aria-label="Link to Case and handlebar mount" title="Link to Case and handlebar mount"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p><img src="../../assets/images/guition-43-mounted.png" alt="GUITION case and handlebar clamp"></p>
<p>The <a href="case-mount/">GUITION case and mount guide</a> includes the shared editable Fusion archive, five STEP downloads, shaded views and an assembly animation. It covers the rear housing, aluminium clamp, split handlebar liner, cable grommet and additional display mounting screws. Check the real module against the CAD's explicitly unverified display-depth reference before fabrication.</p>
<h2 id="can-breakout-board">CAN breakout board<a class="heading-anchor" href="#can-breakout-board" aria-label="Link to CAN breakout board" title="Link to CAN breakout board"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>The <a href="can-breakout/">GUITION CAN breakout guide</a> includes the editable EasyEDA Pro project, labelled board views, schematic, connector pinouts and optional CAN firmware pin configuration. The board fits the display's expansion header and provides a CAN transceiver and six-pin vehicle connector.</p>
<h2 id="esp32-c6-radio-firmware">ESP32-C6 radio firmware<a class="heading-anchor" href="#esp32-c6-radio-firmware" aria-label="Link to ESP32-C6 radio firmware" title="Link to ESP32-C6 radio firmware"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>The P4 relies on its separate C6 chip for both Wi-Fi and Bluetooth. A working vendor Wi-Fi demo alone does not establish that the companion supports this application.</p>
<p>Before replacing C6 firmware, check whether the existing image supports <strong>ESP-Hosted over SDIO, Wi-Fi and Bluetooth controller HCI</strong>. A compatible existing image may work. The profile pins ESP-Hosted 3.1.0 and esp_wifi_remote 1.6.5; a matching 3.1.0 companion is recommended.</p>
<p>If provisioning is needed:</p>
<ol>
<li>Use Espressif’s matching <a href="https://github.com/espressif/esp-hosted-mcu/tree/v3.1.0/examples/bluetooth/esp_hosted_nimble/bleprph_wifi_coex/cp">ESP-Hosted coexistence companion project</a>.</li>
<li>Build for <code>esp32c6</code> with ESP-IDF 6.1. Verify SDIO and the board’s C6 wiring in menuconfig. Hosted 3.x uses the example’s <code>cp/</code> project in place of the old <code>slave/</code> project.</li>
<li>Follow the board vendor’s C6 boot-mode and programming-connector instructions. Flash through the <strong>C6 programming connection</strong>, not the P4 application port.</li>
<li>Test a bike BLE connection, encrypted pairing, phone connection, Wi-Fi and OTA together. Check responsiveness and the serial log for DSI underruns.</li>
</ol>
<p>These checks require the physical board and compatible C6 firmware. See the <a href="https://github.com/espressif/esp-hosted-mcu/blob/v3.1.0/docs/migration.md">Hosted compatibility notes</a> before changing an existing installation.</p>
<h2 id="scope">Scope<a class="heading-anchor" href="#scope" aria-label="Link to Scope" title="Link to Scope"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>Camera, audio, microSD, battery telemetry and RS-485 are not initialized by this application. Optional vehicle CAN/UART pins can be configured in the hardware profile after wiring the required interface; they are not guessed from expansion-header availability. The radio SDIO bus must remain reserved for the C6.</p>
<h2 id="sources-and-notices">Sources and notices<a class="heading-anchor" href="#sources-and-notices" aria-label="Link to Sources and notices" title="Link to Sources and notices"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<ul>
<li><a href="https://www.guition.com/icms/upload/fb081940d6fc11f09850077a33e1404f/file/productmanager-productfile/5dfbe9a7c0fc44869270528bd2411b1e/Directory/JC4880P443C_I_W%20Specifications-EN-V1.0_1776241869964.pdf">GUITION product specifications</a>.</li>
<li><a href="https://github.com/espressif/esp-idf/blob/v6.1/components/esp_hw_support/port/esp32p4/Kconfig.hw_support">ESP-IDF P4 revision selection</a>.</li>
<li><a href="https://github.com/espressif/esp-hosted-mcu/tree/v3.1.0/examples/bluetooth/esp_hosted_nimble/bleprph_wifi_coex">Espressif hosted NimBLE/VHCI example</a>.</li>
<li><a href="https://github.com/espressif/esp-hosted-mcu/blob/v3.1.0/docs/migration.md">Espressif Hosted migration and companion compatibility</a>.</li>
<li><a href="https://github.com/espressif/esp-hosted-mcu/tree/v3.1.0/examples/bluetooth/esp_hosted_nimble/bleprph_wifi_coex/cp">Espressif hosted companion project</a>.</li>
<li><a href="https://github.com/ultramcu/guition-jc4880p4-bsp/tree/324970bade0d1f4e52880fe8016580368bc1e06e">MIT-licensed board initialization reference</a>. Its panel register table is retained with the original copyright and MIT notice in <code>firmware/main/boards/</code>; the display lifecycle and rendering integration are implemented in this project.</li>
</ul>
<h2 id="build-reference">Build reference<a class="heading-anchor" href="#build-reference" aria-label="Link to Build reference" title="Link to Build reference"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>This board is included in the supported <code>default_envs</code>. To build it on its own:</p>
<pre><code class="language-sh">pio run -e guition_jc4880p443c
</code></pre>
<p>The pinned Slint release supplies matching compiler and MCU runtime assets for ESP32-S3 and ESP32-P4. Firmware builds download and verify those assets; a local Rust runtime bootstrap is not required.</p>
<p>Both profiles use the common PlatformIO Espressif32 8.0.0 and ESP-IDF 6.1 pins. The P4 profile selects pre-v3 silicon and its qualified 360 MHz CPU setting. Shared settings live in <code>sdkconfig.defaults</code>; architecture-specific values live in <code>config/</code>. Managed component versions are pinned once in <code>firmware/main/idf_component.yml</code>; each chip has its own generated dependency lock.</p>
<p>For a published release, the <a href="../../install/">browser installer</a> flashes the complete P4 image. It does not replace the C6 companion firmware; prepare that separately as described in <a href="#esp32-c6-radio-firmware">ESP32-C6 radio firmware</a>.</p>
<h2 id="display-and-touch-reference">Display and touch reference<a class="heading-anchor" href="#display-and-touch-reference" aria-label="Link to Display and touch reference" title="Link to Display and touch reference"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Signal</th>
<th>Configuration</th>
</tr>
</thead>
<tbody>
<tr>
<td>Native panel</td>
<td>480 × 800, RGB565, two DSI lanes</td>
</tr>
<tr>
<td>DSI PHY</td>
<td>LDO channel 3 at 2.5 V; 500 Mbit/s per lane</td>
</tr>
<tr>
<td>DPI pixel clock</td>
<td>34 MHz</td>
</tr>
<tr>
<td>Horizontal timing</td>
<td>sync 12, back porch 42, front porch 42</td>
</tr>
<tr>
<td>Vertical timing</td>
<td>sync 2, back porch 8, front porch 166</td>
</tr>
<tr>
<td>Panel reset</td>
<td>GPIO5</td>
</tr>
<tr>
<td>Backlight</td>
<td>GPIO23, LEDC brightness control</td>
</tr>
<tr>
<td>Touch I2C</td>
<td>SDA GPIO7, SCL GPIO8, 400 kHz</td>
</tr>
<tr>
<td>Touch reset</td>
<td>GPIO3</td>
</tr>
<tr>
<td>Touch addressing</td>
<td>Probe 0x5D, then 0x14; polling without an interrupt pin</td>
</tr>
</tbody>
</table></div>
<p>Slint renders into a persistent, full-screen RGB565 canvas in PSRAM. The P4 pixel processing accelerator (PPA) rotates each completed frame into the inactive native DSI framebuffer; two native framebuffers keep scanout separate from rendering. Presentation waits for refresh completion before reusing the old front buffer. This uses three full-screen allocations (2.30 MB total), with no internal-RAM scanline buffers. All four display orientations use PPA rotation and the matching native-coordinate touch transforms. PSRAM runs at 200 MHz. The supplied profile starts at 90° (800 × 480 landscape). <code>DISPLAY_ROTATION</code> sets the initial angle in the hardware profile; runtime selection is saved in NVS.</p>
<h3 id="p4-to-c6-wiring">P4-to-C6 wiring</h3>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Signal</th>
<th>P4 GPIO / configuration</th>
</tr>
</thead>
<tbody>
<tr>
<td>SDIO</td>
<td>Slot 1, four-bit bus, 25 MHz</td>
</tr>
<tr>
<td>CLK / CMD</td>
<td>18 / 19</td>
</tr>
<tr>
<td>D0 / D1 / D2 / D3</td>
<td>14 / 15 / 16 / 17</td>
</tr>
<tr>
<td>Companion reset</td>
<td>54</td>
</tr>
</tbody>
</table></div>
<p>Wi-Fi uses esp_wifi_remote, and the P4 NimBLE host reaches the C6 controller through the ESP-Hosted HCI tunnel. Keep this SDIO bus reserved for the radio companion.</p>
`,headings:[{id:`install-the-firmware`,title:`Install the firmware`,level:2},{id:`large-screen-layout`,title:`Large-screen layout`,level:2},{id:`case-and-handlebar-mount`,title:`Case and handlebar mount`,level:2},{id:`can-breakout-board`,title:`CAN breakout board`,level:2},{id:`esp32-c6-radio-firmware`,title:`ESP32-C6 radio firmware`,level:2},{id:`scope`,title:`Scope`,level:2},{id:`sources-and-notices`,title:`Sources and notices`,level:2},{id:`build-reference`,title:`Build reference`,level:2},{id:`display-and-touch-reference`,title:`Display and touch reference`,level:2},{id:`p4-to-c6-wiring`,title:`P4-to-C6 wiring`,level:3}],search:`The guition_jc4880p443c target is for the GUITION JC4880P443C_I_W / enclosed _Y board, described as JC4880P433 in the original article. It has a 480 × 800 ST7701S MIPI-DSI panel, GT911 touch, 16 MB flash, 32 MB PSRAM and an ESP32-C6 radio companion. The supplied profile selects ESP32-P4 revisions below 3.0, including the original v1.3 engineering-sample board. ESP-IDF treats those chips and production v3.x chips as incompatible targets. Do not use this profile on a v3.x board; check the boot log or vendor revision before installation. Install the firmware Confirm your P4 revision is below 3.0 using the boot log or vendor information. This profile cannot be used on a v3.x board. Check that the C6 has compatible radio firmware; see ESP32-C6 radio firmware below. Open the browser installer, choose GUITION JC4880P443C and connect to the P4 with a USB data cable. Complete installation, then check the screen and touch before connecting your bike. The installer flashes the P4 application and its flash layout. It does not provision or replace the C6 firmware. A full USB installation can reset saved settings; use on-device updates for routine maintenance. The supplied profile starts in 90° landscape (800 × 480). Tap Settings > Orientation to cycle through all four angles; the choice is saved and touch rotates with the screen. Large-screen layout Landscape Ride uses a full-width battery gauge at the top, then a larger speed dial on the left and trip and odometer above the four telemetry cards on the right. In portrait, telemetry cards and distance readings are larger. Stats groups its overview into three columns when the screen is wide; controller and BMS details remain scrollable. These previews are generated from the current shared UI with illustrative connected values during the Pages build. In the browser simulator, select GUITION 4.3-inch under Display model, then use Rotate to explore all four orientations. Case and handlebar mount The GUITION case and mount guide includes the shared editable Fusion archive, five STEP downloads, shaded views and an assembly animation. It covers the rear housing, aluminium clamp, split handlebar liner, cable grommet and additional display mounting screws. Check the real module against the CAD's explicitly unverified display-depth reference before fabrication. CAN breakout board The GUITION CAN breakout guide includes the editable EasyEDA Pro project, labelled board views, schematic, connector pinouts and optional CAN firmware pin configuration. The board fits the display's expansion header and provides a CAN transceiver and six-pin vehicle connector. ESP32-C6 radio firmware The P4 relies on its separate C6 chip for both Wi-Fi and Bluetooth. A working vendor Wi-Fi demo alone does not establish that the companion supports this application. Before replacing C6 firmware, check whether the existing image supports ESP-Hosted over SDIO, Wi-Fi and Bluetooth controller HCI. A compatible existing image may work. The profile pins ESP-Hosted 3.1.0 and esp_wifi_remote 1.6.5; a matching 3.1.0 companion is recommended. If provisioning is needed: Use Espressif’s matching ESP-Hosted coexistence companion project. Build for esp32c6 with ESP-IDF 6.1. Verify SDIO and the board’s C6 wiring in menuconfig. Hosted 3.x uses the example’s cp/ project in place of the old slave/ project. Follow the board vendor’s C6 boot-mode and programming-connector instructions. Flash through the C6 programming connection, not the P4 application port. Test a bike BLE connection, encrypted pairing, phone connection, Wi-Fi and OTA together. Check responsiveness and the serial log for DSI underruns. These checks require the physical board and compatible C6 firmware. See the Hosted compatibility notes before changing an existing installation. Scope Camera, audio, microSD, battery telemetry and RS-485 are not initialized by this application. Optional vehicle CAN/UART pins can be configured in the hardware profile after wiring the required interface; they are not guessed from expansion-header availability. The radio SDIO bus must remain reserved for the C6. Sources and notices GUITION product specifications. ESP-IDF P4 revision selection. Espressif hosted NimBLE/VHCI example. Espressif Hosted migration and companion compatibility. Espressif hosted companion project. MIT-licensed board initialization reference. Its panel register table is retained with the original copyright and MIT notice in firmware/main/boards/; the display lifecycle and rendering integration are implemented in this project. Build reference This board is included in the supported default_envs. To build it on its own: pio run -e guition_jc4880p443c The pinned Slint release supplies matching compiler and MCU runtime assets for ESP32-S3 and ESP32-P4. Firmware builds download and verify those assets; a local Rust runtime bootstrap is not required. Both profiles use the common PlatformIO Espressif32 8.0.0 and ESP-IDF 6.1 pins. The P4 profile selects pre-v3 silicon and its qualified 360 MHz CPU setting. Shared settings live in sdkconfig.defaults; architecture-specific values live in config/. Managed component versions are pinned once in firmware/main/idf_component.yml; each chip has its own generated dependency lock. For a published release, the browser installer flashes the complete P4 image. It does not replace the C6 companion firmware; prepare that separately as described in ESP32-C6 radio firmware. Display and touch reference Signal Configuration Native panel 480 × 800, RGB565, two DSI lanes DSI PHY LDO channel 3 at 2.5 V; 500 Mbit/s per lane DPI pixel clock 34 MHz Horizontal timing sync 12, back porch 42, front porch 42 Vertical timing sync 2, back porch 8, front porch 166 Panel reset GPIO5 Backlight GPIO23, LEDC brightness control Touch I2C SDA GPIO7, SCL GPIO8, 400 kHz Touch reset GPIO3 Touch addressing Probe 0x5D, then 0x14; polling without an interrupt pin Slint renders into a persistent, full-screen RGB565 canvas in PSRAM. The P4 pixel processing accelerator (PPA) rotates each completed frame into the inactive native DSI framebuffer; two native framebuffers keep scanout separate from rendering. Presentation waits for refresh completion before reusing the old front buffer. This uses three full-screen allocations (2.30 MB total), with no internal-RAM scanline buffers. All four display orientations use PPA rotation and the matching native-coordinate touch transforms. PSRAM runs at 200 MHz. The supplied profile starts at 90° (800 × 480 landscape). DISPLAY_ROTATION sets the initial angle in the hardware profile; runtime selection is saved in NVS. P4-to-C6 wiring Signal P4 GPIO / configuration SDIO Slot 1, four-bit bus, 25 MHz CLK / CMD 18 / 19 D0 / D1 / D2 / D3 14 / 15 / 16 / 17 Companion reset 54 Wi-Fi uses esp_wifi_remote, and the P4 NimBLE host reaches the C6 controller through the ESP-Hosted HCI tunnel. Keep this SDIO bus reserved for the radio companion.`};export{e as default};