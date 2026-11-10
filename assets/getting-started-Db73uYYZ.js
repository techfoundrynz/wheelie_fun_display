var e={source:`GETTING-STARTED.MD`,route:`getting-started`,title:`Getting started`,group:`Start here`,description:`Power up, choose an integration and connect your first bike.`,body:`<p>Install the firmware for your display, connect your bike, then choose your everyday settings. You can try the <a href="../simulator/">browser simulator</a> first to learn the screens without hardware.</p>
<h2 id="what-you-need">What you need</h2>
<ul>
<li>A supported <strong>Waveshare ESP32-S3 Touch AMOLED 2.06-inch</strong> or <strong>GUITION JC4880P443C 4.3-inch</strong> display.</li>
<li>A USB data cable for the first installation.</li>
<li>A VESC-based bike with Wheelie Fun Bike, wheel-e or a compatible generic VESC setup.</li>
</ul>
<p>Check your board before installing: <a href="../hardware/waveshare-206/">Waveshare setup</a> or <a href="../hardware/guition-43/">GUITION setup</a>. The GUITION profile requires an ESP32-P4 below revision 3.0 and compatible C6 radio firmware. Images for other boards or revisions are not interchangeable.</p>
<h2 id="1-install-and-power-up">1. Install and power up</h2>
<ol>
<li>Open the <a href="../install/">browser installer</a> and choose your exact display model.</li>
<li>Connect the display with a USB data cable and follow the installation prompts. Keep it powered until installation finishes.</li>
<li>Let the display start. The Ride dashboard should appear. Unlock it if a screen lock was already configured.</li>
</ol>
<p>A complete USB installation can reset saved settings. For later updates, use <a href="../maintenance/">the display's update menu</a>.</p>
<p><strong>Disconnected</strong> is normal before you select a bike. <strong>Reconnecting</strong> means the display is trying a saved connection.</p>
<p><img src="../assets/images/ride-portrait.png" alt="Ride dashboard in portrait"></p>
<p>Screenshots throughout these guides use simulated values. The simulator does not connect to or control a real bike.</p>
<h2 id="2-choose-your-bike-type">2. Choose your bike type</h2>
<p>Tap the cog in the upper-right corner, then open <strong>Settings &gt; Vehicle &gt; Vehicle integration</strong> before connecting. Leave it at <strong>Auto</strong> unless you need a specific integration.</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Integration</th>
<th>Available features</th>
</tr>
</thead>
<tbody>
<tr>
<td>Auto</td>
<td>Detects WFB or wheel-e; falls back to standard VESC telemetry if neither package is found.</td>
</tr>
<tr>
<td>Wheelie Fun Bike</td>
<td>Ride data, supported tuning controls and bike locking.</td>
</tr>
<tr>
<td>wheel-e</td>
<td>Ride data and supported bike locking. Use VESC Tool for wheel-e tuning.</td>
</tr>
<tr>
<td>VESC</td>
<td>Standard ride data and local display settings.</td>
</tr>
</tbody>
</table></div>
<p>The display checks the controller's replies before enabling package features. A Bluetooth device name alone does not identify its installed package.</p>
<h2 id="3-connect-your-bike">3. Connect your bike</h2>
<ol>
<li>Power the bike and make its Bluetooth interface available. Disconnect another phone or app if it occupies the bike's only Bluetooth connection.</li>
<li>Open <strong>Settings &gt; Vehicle</strong> and select the bike. Bluetooth Low Energy (BLE) is the default connection.</li>
<li>If prompted, enter the pairing PIN configured on the bike's Bluetooth device.</li>
<li>Wait for controller discovery to finish and live readings to appear.</li>
</ol>
<p>The display finds a compatible motor controller automatically, including one reached through the bike's CAN bus. <strong>Auto</strong> works with renamed devices too.</p>
<p>A reading of <code>--</code> means the value is missing or out of date; it does not mean zero. If readings do not appear, follow <a href="../troubleshooting/#the-link-connects-but-values-stay-unavailable">troubleshooting</a>. For wired connections or a phone app, see <a href="../connections/">connections</a>.</p>
<h2 id="4-set-up-the-screen">4. Set up the screen</h2>
<p>Open Settings with the cog. Choose brightness, speed units and temperature units. Speed and temperature units are independent.</p>
<p>Tap <strong>Settings &gt; Orientation</strong> to cycle through 0°, 90°, 180° and 270°. Touch controls rotate with the screen. These preferences are saved across restarts.</p>
<p>Use the bottom tabs or swipe horizontally:</p>
<ul>
<li><strong>Ride:</strong> speed, battery, power, pitch and connection status.</li>
<li><strong>Stats:</strong> detailed readings; scroll vertically for more.</li>
<li><strong>Tune:</strong> supported bike settings. Changes remain drafts until you save and confirm them.</li>
</ul>
<p>See <a href="../guide/">using the display</a> for tuning steps and screen details.</p>
<h2 id="5-add-a-screen-lock-optional">5. Add a screen lock (optional)</h2>
<ol>
<li>Open <strong>Settings &gt; Screen lock &gt; Set screen lock</strong>.</li>
<li>Choose <strong>Use PIN</strong> or <strong>Use pattern</strong>.</li>
<li>Enter the six-digit PIN or six-dot pattern twice.</li>
</ol>
<p>There is no default credential or recovery-code screen. Keep a record of your chosen credential.</p>
<p><strong>Lock screen</strong> protects the display controls. <strong>Lock bike and screen</strong>, when supported, also asks the bike to lock and waits for confirmation. An unconfirmed request does not establish that the bike is locked.</p>
<p>When the locked screen goes black, tap once to wake it, lift your finger, then unlock. See <a href="../pins/">PINs, patterns and pairing</a> for phone pairing and credential changes.</p>
<h2 id="6-check-for-updates">6. Check for updates</h2>
<p>Connect under <strong>Settings &gt; Wi-Fi</strong>, then open <strong>Settings &gt; Firmware updates</strong>. Review the version and confirm installation. Keep power connected and restart when offered.</p>
<p>See <a href="../maintenance/">updates and maintenance</a> for update methods and recovery checks.</p>
<h2 id="where-next">Where next</h2>
<p><a href="../guide/">Using the display</a> · <a href="../connections/#connect-a-phone-through-the-display">Connect a phone</a> · <a href="../troubleshooting/">Troubleshooting</a></p>
`,headings:[{id:`what-you-need`,title:`What you need`,level:2},{id:`1-install-and-power-up`,title:`1. Install and power up`,level:2},{id:`2-choose-your-bike-type`,title:`2. Choose your bike type`,level:2},{id:`3-connect-your-bike`,title:`3. Connect your bike`,level:2},{id:`4-set-up-the-screen`,title:`4. Set up the screen`,level:2},{id:`5-add-a-screen-lock-optional`,title:`5. Add a screen lock (optional)`,level:2},{id:`6-check-for-updates`,title:`6. Check for updates`,level:2},{id:`where-next`,title:`Where next`,level:2}],search:`Install the firmware for your display, connect your bike, then choose your everyday settings. You can try the browser simulator first to learn the screens without hardware. What you need A supported Waveshare ESP32-S3 Touch AMOLED 2.06-inch or GUITION JC4880P443C 4.3-inch display. A USB data cable for the first installation. A VESC-based bike with Wheelie Fun Bike, wheel-e or a compatible generic VESC setup. Check your board before installing: Waveshare setup or GUITION setup. The GUITION profile requires an ESP32-P4 below revision 3.0 and compatible C6 radio firmware. Images for other boards or revisions are not interchangeable. 1. Install and power up Open the browser installer and choose your exact display model. Connect the display with a USB data cable and follow the installation prompts. Keep it powered until installation finishes. Let the display start. The Ride dashboard should appear. Unlock it if a screen lock was already configured. A complete USB installation can reset saved settings. For later updates, use the display's update menu. Disconnected is normal before you select a bike. Reconnecting means the display is trying a saved connection. Screenshots throughout these guides use simulated values. The simulator does not connect to or control a real bike. 2. Choose your bike type Tap the cog in the upper-right corner, then open Settings > Vehicle > Vehicle integration before connecting. Leave it at Auto unless you need a specific integration. Integration Available features Auto Detects WFB or wheel-e; falls back to standard VESC telemetry if neither package is found. Wheelie Fun Bike Ride data, supported tuning controls and bike locking. wheel-e Ride data and supported bike locking. Use VESC Tool for wheel-e tuning. VESC Standard ride data and local display settings. The display checks the controller's replies before enabling package features. A Bluetooth device name alone does not identify its installed package. 3. Connect your bike Power the bike and make its Bluetooth interface available. Disconnect another phone or app if it occupies the bike's only Bluetooth connection. Open Settings > Vehicle and select the bike. Bluetooth Low Energy (BLE) is the default connection. If prompted, enter the pairing PIN configured on the bike's Bluetooth device. Wait for controller discovery to finish and live readings to appear. The display finds a compatible motor controller automatically, including one reached through the bike's CAN bus. Auto works with renamed devices too. A reading of -- means the value is missing or out of date; it does not mean zero. If readings do not appear, follow troubleshooting. For wired connections or a phone app, see connections. 4. Set up the screen Open Settings with the cog. Choose brightness, speed units and temperature units. Speed and temperature units are independent. Tap Settings > Orientation to cycle through 0°, 90°, 180° and 270°. Touch controls rotate with the screen. These preferences are saved across restarts. Use the bottom tabs or swipe horizontally: Ride: speed, battery, power, pitch and connection status. Stats: detailed readings; scroll vertically for more. Tune: supported bike settings. Changes remain drafts until you save and confirm them. See using the display for tuning steps and screen details. 5. Add a screen lock (optional) Open Settings > Screen lock > Set screen lock. Choose Use PIN or Use pattern. Enter the six-digit PIN or six-dot pattern twice. There is no default credential or recovery-code screen. Keep a record of your chosen credential. Lock screen protects the display controls. Lock bike and screen, when supported, also asks the bike to lock and waits for confirmation. An unconfirmed request does not establish that the bike is locked. When the locked screen goes black, tap once to wake it, lift your finger, then unlock. See PINs, patterns and pairing for phone pairing and credential changes. 6. Check for updates Connect under Settings > Wi-Fi, then open Settings > Firmware updates. Review the version and confirm installation. Keep power connected and restart when offered. See updates and maintenance for update methods and recovery checks. Where next Using the display · Connect a phone · Troubleshooting`};export{e as default};