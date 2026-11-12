var e={source:`docs/hardware-waveshare-206.md`,route:`hardware/waveshare-206`,title:`Waveshare S3 2.06-inch`,group:`Hardware`,description:`AMOLED setup, board wiring, orientation and USB recovery.`,body:`<p>Use the <code>waveshare_esp32s3_206</code> target for the <strong>Waveshare ESP32-S3 Touch AMOLED 2.06</strong>. Its native resolution is 410 × 502, with a CO5300 AMOLED panel, FT3168 touch, 32 MB flash and 8 MB octal PSRAM. Wi-Fi and Bluetooth are built into the ESP32-S3; no radio companion needs provisioning.</p>
<p><img src="../../assets/images/ride-portrait.png" alt="Software preview of the portrait Ride dashboard"></p>
<p>The picture is generated from the current shared UI during the documentation build. Its connected values are illustrative.</p>
<h2 id="install-the-firmware">Install the firmware<a class="heading-anchor" href="#install-the-firmware" aria-label="Link to Install the firmware" title="Link to Install the firmware"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<ol>
<li>Open the <a href="../../install/">browser firmware installer</a> in a supported desktop browser.</li>
<li>Choose <strong>Waveshare ESP32-S3 AMOLED 2.06</strong>. Other Waveshare sizes use different firmware.</li>
<li>Connect a USB data cable, select the display’s port and follow the prompts.</li>
<li>After startup, check touch and orientation, then follow <a href="../../getting-started/">getting started</a> to connect your bike.</li>
</ol>
<p>A complete USB installation can reset saved settings. For ordinary updates, use <strong>Settings &gt; Firmware updates</strong>; see <a href="../../maintenance/">updates and maintenance</a>.</p>
<h2 id="display-touch-and-orientation">Display, touch and orientation<a class="heading-anchor" href="#display-touch-and-orientation" aria-label="Link to Display, touch and orientation" title="Link to Display, touch and orientation"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>Tap <strong>Settings &gt; Orientation</strong> to cycle through 0°, 90°, 180° and 270°. The screen and touch controls rotate together, and your choice is saved.</p>
<p>The layout leaves room for the panel’s rounded corners. Check that you can reach the cog, bottom tabs and keyboard in your chosen mounting orientation.</p>
<h2 id="board-wiring">Board wiring<a class="heading-anchor" href="#board-wiring" aria-label="Link to Board wiring" title="Link to Board wiring"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>These are the shipped profile's internal connections, not suggested pins for external accessories.</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Function</th>
<th>GPIO / value</th>
</tr>
</thead>
<tbody>
<tr>
<td>I2C SDA / SCL</td>
<td>15 / 14</td>
</tr>
<tr>
<td>QSPI clock / chip select</td>
<td>11 / 12</td>
</tr>
<tr>
<td>QSPI data 0–3</td>
<td>4 / 5 / 6 / 7</td>
</tr>
<tr>
<td>Display reset / enable</td>
<td>8 / 13</td>
</tr>
<tr>
<td>Touch reset / interrupt</td>
<td>9 / 38</td>
</tr>
<tr>
<td>Panel column offset</td>
<td>22 pixels</td>
</tr>
</tbody>
</table></div>
<p>BLE is the default vehicle transport. CAN and UART appear in Settings only after their pin pairs are configured in the target profile. Use a firmware build configured for your wiring and provide an external CAN transceiver if using CAN; see <a href="../../connections/">connections</a>.</p>
<h2 id="case-and-handlebar-mount">Case and handlebar mount<a class="heading-anchor" href="#case-and-handlebar-mount" aria-label="Link to Case and handlebar mount" title="Link to Case and handlebar mount"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p><img src="../../assets/images/waveshare-206-mounted.png" alt="Portrait and landscape Waveshare 2.06 cases with handlebar clamps"></p>
<p>The <a href="case-mount/">case and mount guide</a> has part downloads, dimensions, a fastener list and assembly views for a nominal <strong>22.225 mm (7/8-inch)</strong> handlebar.</p>
<ul>
<li>Rotate the same casing on its square mounting pattern for portrait or landscape, then match <strong>Settings &gt; Orientation</strong>.</li>
<li>Fit the <a href="case-mount/#cable-grommet-installation">cable grommet</a> to the 6 mm cable jacket before attaching oversized connectors.</li>
<li>Check your real display and printed parts against the model before final assembly.</li>
</ul>
<h2 id="usb-recovery-and-checks">USB recovery and checks<a class="heading-anchor" href="#usb-recovery-and-checks" aria-label="Link to USB recovery and checks" title="Link to USB recovery and checks"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>If the USB port is missing:</p>
<ol>
<li>Use a known USB data cable.</li>
<li>Hold <strong>BOOT</strong>, tap <strong>RESET</strong>, then release <strong>BOOT</strong>.</li>
<li>Select the newly detected port in the installer. With PlatformIO, use <code>pio device list</code> to identify it.</li>
</ol>
<p>After installation, confirm the dashboard, cog, Settings scrolling and touch at all four orientations. The connected S3 board has passed SDK 6.1 startup and rendering checks; real-bike, phone-proxy and OTA acceptance remain listed in <a href="../../todo/">TODO</a>. Occasional touch I2C read errors are also tracked there; save the serial log when diagnosing a touch issue.</p>
<p>See <a href="../../troubleshooting/">troubleshooting</a>, <a href="../../performance/">performance configuration</a>, the <a href="https://www.waveshare.com/esp32-s3-touch-amoled-2.06.htm">Waveshare product page</a> and <a href="https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06">vendor examples</a>.</p>
<h2 id="build-and-driver-reference">Build and driver reference<a class="heading-anchor" href="#build-and-driver-reference" aria-label="Link to Build and driver reference" title="Link to Build and driver reference"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>To build from source, follow <a href="../../building/">building and installing</a> with environment <code>waveshare_esp32s3_206</code>. Release application images are <code>waveshare_esp32s3_206.bin</code>; full USB images are <code>waveshare_esp32s3_206-factory.bin</code>.</p>
<p>The profile uses a 240 MHz CPU, 80 MHz flash/octal PSRAM and 80 MHz panel QSPI. CO5300 uses an SH8601-compatible transport with a 22-pixel column offset; FT3168 uses the compatible FT5x06 driver. Display and touch resets use GPIOs directly. Safe insets are 38 pixels horizontally and 24 vertically. See <a href="../../performance/">performance configuration</a> for rendering details and <a href="https://github.com/techfoundrynz/wheelie_fun_display/blob/main/platformio.ini">platformio.ini</a> for the profile.</p>
<p>Transitions stream cached page images through internal DMA tiles; scrolling renders dirty spans directly. Rotation repaints the full panel to clear old controls from the margins.</p>
`,headings:[{id:`install-the-firmware`,title:`Install the firmware`,level:2},{id:`display-touch-and-orientation`,title:`Display, touch and orientation`,level:2},{id:`board-wiring`,title:`Board wiring`,level:2},{id:`case-and-handlebar-mount`,title:`Case and handlebar mount`,level:2},{id:`usb-recovery-and-checks`,title:`USB recovery and checks`,level:2},{id:`build-and-driver-reference`,title:`Build and driver reference`,level:2}],search:`Use the waveshare_esp32s3_206 target for the Waveshare ESP32-S3 Touch AMOLED 2.06. Its native resolution is 410 × 502, with a CO5300 AMOLED panel, FT3168 touch, 32 MB flash and 8 MB octal PSRAM. Wi-Fi and Bluetooth are built into the ESP32-S3; no radio companion needs provisioning. The picture is generated from the current shared UI during the documentation build. Its connected values are illustrative. Install the firmware Open the browser firmware installer in a supported desktop browser. Choose Waveshare ESP32-S3 AMOLED 2.06. Other Waveshare sizes use different firmware. Connect a USB data cable, select the display’s port and follow the prompts. After startup, check touch and orientation, then follow getting started to connect your bike. A complete USB installation can reset saved settings. For ordinary updates, use Settings > Firmware updates; see updates and maintenance. Display, touch and orientation Tap Settings > Orientation to cycle through 0°, 90°, 180° and 270°. The screen and touch controls rotate together, and your choice is saved. The layout leaves room for the panel’s rounded corners. Check that you can reach the cog, bottom tabs and keyboard in your chosen mounting orientation. Board wiring These are the shipped profile's internal connections, not suggested pins for external accessories. Function GPIO / value I2C SDA / SCL 15 / 14 QSPI clock / chip select 11 / 12 QSPI data 0–3 4 / 5 / 6 / 7 Display reset / enable 8 / 13 Touch reset / interrupt 9 / 38 Panel column offset 22 pixels BLE is the default vehicle transport. CAN and UART appear in Settings only after their pin pairs are configured in the target profile. Use a firmware build configured for your wiring and provide an external CAN transceiver if using CAN; see connections. Case and handlebar mount The case and mount guide has part downloads, dimensions, a fastener list and assembly views for a nominal 22.225 mm (7/8-inch) handlebar. Rotate the same casing on its square mounting pattern for portrait or landscape, then match Settings > Orientation. Fit the cable grommet to the 6 mm cable jacket before attaching oversized connectors. Check your real display and printed parts against the model before final assembly. USB recovery and checks If the USB port is missing: Use a known USB data cable. Hold BOOT, tap RESET, then release BOOT. Select the newly detected port in the installer. With PlatformIO, use pio device list to identify it. After installation, confirm the dashboard, cog, Settings scrolling and touch at all four orientations. The connected S3 board has passed SDK 6.1 startup and rendering checks; real-bike, phone-proxy and OTA acceptance remain listed in TODO. Occasional touch I2C read errors are also tracked there; save the serial log when diagnosing a touch issue. See troubleshooting, performance configuration, the Waveshare product page and vendor examples. Build and driver reference To build from source, follow building and installing with environment waveshare_esp32s3_206. Release application images are waveshare_esp32s3_206.bin; full USB images are waveshare_esp32s3_206-factory.bin. The profile uses a 240 MHz CPU, 80 MHz flash/octal PSRAM and 80 MHz panel QSPI. CO5300 uses an SH8601-compatible transport with a 22-pixel column offset; FT3168 uses the compatible FT5x06 driver. Display and touch resets use GPIOs directly. Safe insets are 38 pixels horizontally and 24 vertically. See performance configuration for rendering details and platformio.ini for the profile. Transitions stream cached page images through internal DMA tiles; scrolling renders dirty spans directly. Rotation repaints the full panel to clear old controls from the margins.`};export{e as default};