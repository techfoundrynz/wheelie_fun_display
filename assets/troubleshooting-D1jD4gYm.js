var e={source:`docs/troubleshooting.md`,route:`troubleshooting`,title:`Troubleshooting`,group:`For riders`,description:`Find the right next check for connection, touch or update issues.`,body:`<p>Find the symptom below and work through its checks in order. Keep the bike stopped while investigating settings or locking. A reading of <code>--</code> means unavailable or out-of-date data, not zero.</p>
<h2 id="the-screen-is-black">The screen is black</h2>
<ol>
<li>Tap once, lift your finger and wait for the unlock screen. A locked display intentionally turns the panel black after its timeout.</li>
<li>Check power and the USB cable.</li>
<li>After a first installation, confirm you selected the exact board model and completed the full USB installation.</li>
<li>If the correct image still does not start, save the boot log if available and report the problem.</li>
</ol>
<p>Only the listed Waveshare 2.06-inch and GUITION 4.3-inch targets are supported. See <a href="../hardware/waveshare-206/#usb-recovery-and-checks">Waveshare recovery</a> or <a href="../hardware/guition-43/">GUITION requirements</a>.</p>
<h2 id="the-usb-port-is-missing">The USB port is missing</h2>
<ol>
<li>Use a known USB <strong>data</strong> cable and try a direct computer port.</li>
<li>Close any serial monitor or app already using the port.</li>
<li>Follow your board's BOOT/RESET download-mode procedure, then reconnect in the installer. For Waveshare, hold BOOT, tap RESET and release BOOT.</li>
<li>Select the newly detected port; its name may have changed.</li>
</ol>
<p>If using PlatformIO, <code>pio device list</code> lists available ports. See the <a href="../install/">browser installer</a> for USB installation.</p>
<h2 id="no-bike-appears-over-ble">No bike appears over BLE</h2>
<ol>
<li>Power the bike and make its Bluetooth interface available.</li>
<li>Disconnect another phone/app if the bike's module allows only one connection.</li>
<li>Open <strong>Settings &gt; Vehicle</strong> and look through the device list. Recognizable names sort first, but other compatible devices remain selectable.</li>
<li>Leave integration at <strong>Auto</strong>, or confirm the explicitly selected WFB/wheel-e package is installed on the intended controller.</li>
</ol>
<h2 id="the-link-connects-but-values-stay-unavailable">The link connects but values stay unavailable</h2>
<ol>
<li>Check controller discovery status under <strong>Settings &gt; Vehicle</strong>. A connected link can still be waiting for valid readings.</li>
<li>Disconnect the phone proxy to let discovery and display polling proceed.</li>
<li>Use <strong>Find main controller again</strong> when the link is available.</li>
<li>Check the integration. Explicit WFB or wheel-e selection requires that package; generic VESC provides standard readings only.</li>
</ol>
<p>Busy phone traffic or firmware transfers can temporarily pause readings. BMS data also depends on controller support and may be cached. See <a href="../guide/#stats">Stats</a>.</p>
<h2 id="the-ble-pin-is-rejected">The BLE PIN is rejected</h2>
<p>Check which devices you are pairing:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Connection</th>
<th>Code to enter</th>
</tr>
</thead>
<tbody>
<tr>
<td>Display to the bike's Bluetooth device</td>
<td>The PIN configured on that bike device.</td>
</tr>
<tr>
<td>Phone to the display</td>
<td>The code shown under <strong>Settings &gt; Phone BLE code</strong> after unlocking. A pattern uses its six dot numbers.</td>
</tr>
</tbody>
</table></div>
<p>If the bike's PIN changed, retry with its new PIN. If the display's screen lock changed or was removed, forget the display's old pairing on the phone and pair again. After restarting a secured display, unlock it locally before connecting the phone.</p>
<p>See <a href="../pins/">PINs, patterns and pairing</a> for entry rules and cooldowns.</p>
<h2 id="can-or-uart-is-absent-from-settings">CAN or UART is absent from Settings</h2>
<p>These choices require firmware with both pins configured for that interface. Stock profiles leave optional wired pins unset.</p>
<p>Check your board's wiring and firmware configuration. CAN also needs a transceiver, matching bus speed and a unique display ID; UART needs matching baud and compatible signal levels. Changing GPIO configuration requires rebuilding firmware. See <a href="../building/#configure-an-optional-wired-interface">wired-interface setup</a>.</p>
<h2 id="a-tuning-or-lock-action-is-unavailable">A tuning or lock action is unavailable</h2>
<ol>
<li>Confirm the integration supports that feature. Tune currently requires a supported WFB configuration.</li>
<li>Unlock the display and stop the bike.</li>
<li>Disconnect the phone proxy.</li>
<li>Wait for fresh readings, a ready connection and no controller fault.</li>
<li>For tuning, read the bike's configuration successfully before editing.</li>
</ol>
<p>A draft is not a saved setting. After an interrupted tuning save, read the bike again before retrying. An <strong>unconfirmed</strong> lock means the required bike response has not arrived; do not assume propulsion is disabled. See <a href="../guide/#tune">tuning</a> or <a href="../pins/#lock-the-bike-and-screen">bike locking</a>.</p>
<h2 id="no-update-is-offered">No update is offered</h2>
<ol>
<li>Confirm the display is connected to Wi-Fi.</li>
<li>Disconnect the phone proxy and check again under <strong>Settings &gt; Firmware updates</strong>.</li>
<li>Check whether a public <a href="https://github.com/techfoundrynz/wheelie_fun_display/releases">release</a> exists for your exact board.</li>
<li>Record any message about a missing or invalid image.</li>
</ol>
<p>Unavailable releases or validation failures keep the current firmware selected. See <a href="../maintenance/">updates and maintenance</a>.</p>
<h2 id="report-a-reproducible-problem">Report a reproducible problem</h2>
<p>Include:</p>
<ul>
<li>Display model and firmware version from <strong>Settings &gt; About</strong>.</li>
<li>Integration and connection type (BLE, CAN or UART).</li>
<li>Controller firmware and, if relevant, phone/app versions.</li>
<li>Steps to reproduce, expected behavior, actual behavior and the last version that worked.</li>
<li>A serial log if available, with passwords and PINs removed.</li>
</ul>
<p>Avoid erasing settings before collecting this information. See <a href="../contributing/">contributing</a> for reporting details.</p>
`,headings:[{id:`the-screen-is-black`,title:`The screen is black`,level:2},{id:`the-usb-port-is-missing`,title:`The USB port is missing`,level:2},{id:`no-bike-appears-over-ble`,title:`No bike appears over BLE`,level:2},{id:`the-link-connects-but-values-stay-unavailable`,title:`The link connects but values stay unavailable`,level:2},{id:`the-ble-pin-is-rejected`,title:`The BLE PIN is rejected`,level:2},{id:`can-or-uart-is-absent-from-settings`,title:`CAN or UART is absent from Settings`,level:2},{id:`a-tuning-or-lock-action-is-unavailable`,title:`A tuning or lock action is unavailable`,level:2},{id:`no-update-is-offered`,title:`No update is offered`,level:2},{id:`report-a-reproducible-problem`,title:`Report a reproducible problem`,level:2}],search:`Find the symptom below and work through its checks in order. Keep the bike stopped while investigating settings or locking. A reading of -- means unavailable or out-of-date data, not zero. The screen is black Tap once, lift your finger and wait for the unlock screen. A locked display intentionally turns the panel black after its timeout. Check power and the USB cable. After a first installation, confirm you selected the exact board model and completed the full USB installation. If the correct image still does not start, save the boot log if available and report the problem. Only the listed Waveshare 2.06-inch and GUITION 4.3-inch targets are supported. See Waveshare recovery or GUITION requirements. The USB port is missing Use a known USB data cable and try a direct computer port. Close any serial monitor or app already using the port. Follow your board's BOOT/RESET download-mode procedure, then reconnect in the installer. For Waveshare, hold BOOT, tap RESET and release BOOT. Select the newly detected port; its name may have changed. If using PlatformIO, pio device list lists available ports. See the browser installer for USB installation. No bike appears over BLE Power the bike and make its Bluetooth interface available. Disconnect another phone/app if the bike's module allows only one connection. Open Settings > Vehicle and look through the device list. Recognizable names sort first, but other compatible devices remain selectable. Leave integration at Auto, or confirm the explicitly selected WFB/wheel-e package is installed on the intended controller. The link connects but values stay unavailable Check controller discovery status under Settings > Vehicle. A connected link can still be waiting for valid readings. Disconnect the phone proxy to let discovery and display polling proceed. Use Find main controller again when the link is available. Check the integration. Explicit WFB or wheel-e selection requires that package; generic VESC provides standard readings only. Busy phone traffic or firmware transfers can temporarily pause readings. BMS data also depends on controller support and may be cached. See Stats. The BLE PIN is rejected Check which devices you are pairing: Connection Code to enter Display to the bike's Bluetooth device The PIN configured on that bike device. Phone to the display The code shown under Settings > Phone BLE code after unlocking. A pattern uses its six dot numbers. If the bike's PIN changed, retry with its new PIN. If the display's screen lock changed or was removed, forget the display's old pairing on the phone and pair again. After restarting a secured display, unlock it locally before connecting the phone. See PINs, patterns and pairing for entry rules and cooldowns. CAN or UART is absent from Settings These choices require firmware with both pins configured for that interface. Stock profiles leave optional wired pins unset. Check your board's wiring and firmware configuration. CAN also needs a transceiver, matching bus speed and a unique display ID; UART needs matching baud and compatible signal levels. Changing GPIO configuration requires rebuilding firmware. See wired-interface setup. A tuning or lock action is unavailable Confirm the integration supports that feature. Tune currently requires a supported WFB configuration. Unlock the display and stop the bike. Disconnect the phone proxy. Wait for fresh readings, a ready connection and no controller fault. For tuning, read the bike's configuration successfully before editing. A draft is not a saved setting. After an interrupted tuning save, read the bike again before retrying. An unconfirmed lock means the required bike response has not arrived; do not assume propulsion is disabled. See tuning or bike locking. No update is offered Confirm the display is connected to Wi-Fi. Disconnect the phone proxy and check again under Settings > Firmware updates. Check whether a public release exists for your exact board. Record any message about a missing or invalid image. Unavailable releases or validation failures keep the current firmware selected. See updates and maintenance. Report a reproducible problem Include: Display model and firmware version from Settings > About. Integration and connection type (BLE, CAN or UART). Controller firmware and, if relevant, phone/app versions. Steps to reproduce, expected behavior, actual behavior and the last version that worked. A serial log if available, with passwords and PINs removed. Avoid erasing settings before collecting this information. See contributing for reporting details.`};export{e as default};