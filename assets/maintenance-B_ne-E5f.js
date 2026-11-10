var e={source:`docs/maintenance.md`,route:`maintenance`,title:`Updates & maintenance`,group:`For riders`,description:`Update your display over Wi-Fi, USB or VESC Tool and diagnose problems.`,body:`<p>Use the display's Wi-Fi update menu for routine updates. Use the browser installer for a first installation or complete USB recovery.</p>
<h2 id="choose-an-update-method">Choose an update method<a class="heading-anchor" href="#choose-an-update-method" aria-label="Link to Choose an update method" title="Link to Choose an update method"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Method</th>
<th>Use it for</th>
<th>What to expect</th>
</tr>
</thead>
<tbody>
<tr>
<td>Display Wi-Fi update</td>
<td>Routine updates</td>
<td>Downloads an image for your board; restart when offered.</td>
</tr>
<tr>
<td>Browser USB installer</td>
<td>First installation or recovery</td>
<td>Installs the full flash layout and can reset saved settings.</td>
</tr>
<tr>
<td>VESC Tool</td>
<td>Application updates over BLE or CAN</td>
<td>Requires the exact board's application image; restarts automatically after verification.</td>
</tr>
</tbody>
</table></div>
<h2 id="update-from-the-display">Update from the display<a class="heading-anchor" href="#update-from-the-display" aria-label="Link to Update from the display" title="Link to Update from the display"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<ol>
<li>Stop the bike, unlock the display and disconnect any phone using its proxy.</li>
<li>Open <strong>Settings &gt; Wi-Fi</strong> and connect to a network. You can enter a hidden network name if needed.</li>
<li>Open <strong>Settings &gt; Firmware updates</strong> and check for a release.</li>
<li>Review the version, confirm installation and keep power connected.</li>
<li>Wait for verification, then select <strong>Restart into update</strong> when offered.</li>
</ol>
<p>Vehicle communications pause during installation. If no public release is available, an image is incompatible or validation fails, the display reports the problem and keeps the installed firmware selected.</p>
<p>Updates come from the project's <a href="https://github.com/techfoundrynz/wheelie_fun_display/releases">GitHub releases</a>. The display checks the download and board identity before activating it. An update that fails early during startup can roll back to the previous image.</p>
<h2 id="update-progress-on-the-display">Update progress on the display<a class="heading-anchor" href="#update-progress-on-the-display" aria-label="Link to Update progress on the display" title="Link to Update progress on the display"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>Wi-Fi downloads and VESC Tool uploads open a progress panel showing the source, percentage and status. Navigation is blocked during an update.</p>
<ul>
<li><strong>Cancel</strong> is available during transfer, then disabled while verification and activation finish.</li>
<li>A stopped or rejected update leaves its status visible until you close it.</li>
<li>A verified Wi-Fi update offers <strong>Restart into update</strong>.</li>
<li>A verified VESC Tool upload restarts automatically. Cancellation after activation cannot undo the restart.</li>
</ul>
<p>Screen-lock and Bluetooth pairing prompts take priority over the progress panel.</p>
<h2 id="usb-installation-or-recovery">USB installation or recovery<a class="heading-anchor" href="#usb-installation-or-recovery" aria-label="Link to USB installation or recovery" title="Link to USB installation or recovery"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<ol>
<li>Read the setup guide for your exact <a href="../hardware/waveshare-206/">Waveshare</a> or <a href="../hardware/guition-43/">GUITION</a> board.</li>
<li>Open the <a href="../install/">browser installer</a> using a supported desktop browser.</li>
<li>Choose the exact model, connect a USB data cable and follow the prompts.</li>
<li>After startup, check touch, orientation and saved settings before reconnecting the bike.</li>
</ol>
<p>The complete USB image includes the bootloader and partition layout. It is different from an application-only update image. GUITION USB installation does not update its C6 radio companion.</p>
<h2 id="vesc-tool-updates">VESC Tool updates<a class="heading-anchor" href="#vesc-tool-updates" aria-label="Link to VESC Tool updates" title="Link to VESC Tool updates"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>Stop the bike and unlock the display. Download the <strong>application</strong> <code>.bin</code> for your exact display target from the release. Do not select a <code>-factory.bin</code>, bootloader, partition table or GUITION C6 image.</p>
<p>Choose one connection route:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Route</th>
<th>Steps</th>
</tr>
</thead>
<tbody>
<tr>
<td>Display BLE with CAN selected</td>
<td>Enable Phone connection, connect VESC Tool to the display, and use its local Custom Module identity. Selecting another CAN controller targets that controller instead.</td>
</tr>
<tr>
<td>Through another VESC on CAN</td>
<td>Connect VESC Tool to that VESC, scan the bus and select the display's CAN ID (default 254). The display's CAN transport must be active.</td>
</tr>
<tr>
<td>Update-only BLE, without a bike or Wi-Fi</td>
<td>Disconnect an existing phone session. Open <strong>Settings &gt; Firmware updates &gt; Start BLE update mode</strong>, then connect VESC Tool to <strong>Wheelie Fun Display OTA</strong>.</td>
</tr>
</tbody>
</table></div>
<p>In VESC Tool, open <strong>Firmware &gt; Custom File</strong>, select the matching application image and start the upload. Keep power and the connection available until verification finishes and the display restarts.</p>
<p>Update-only BLE uses the usual phone pairing and screen-lock code. Exiting that mode or restarting restores normal operation without changing the saved phone-connection preference or advertised name.</p>
<p>During upload, bike polling and tuning pause. A BLE disconnect, CAN session change, screen lock, cancellation or 60 seconds of inactivity aborts the transfer. A remote CAN sender disappearing is detected by the inactivity timeout. CAN has no Bluetooth authentication; physical bus access must be trusted.</p>
<p>The updater checks size, checksum, image validity and board identity before selecting the new image. Failed transfers keep the current firmware. Compressed uploads, bootloader writes and broadcast/all-controller flashing are unavailable. Physical BLE and CAN uploads still require hardware acceptance testing.</p>
<h2 id="diagnose-before-resetting">Diagnose before resetting<a class="heading-anchor" href="#diagnose-before-resetting" aria-label="Link to Diagnose before resetting" title="Link to Diagnose before resetting"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>Open <strong>Settings &gt; About</strong> and inspect Diagnostics. Record the firmware version, board identity, uptime and connection information. For a repeatable problem, note the exact steps and save a serial log if available. Never include PINs or Wi-Fi passwords in a report.</p>
<p>Avoid erasing saved settings as a routine fix: this removes preferences, saved bikes, Bluetooth pairings, Wi-Fi details and screen-lock state. Start with <a href="../troubleshooting/">troubleshooting</a>.</p>
<h2 id="contributor-information">Contributor information<a class="heading-anchor" href="#contributor-information" aria-label="Link to Contributor information" title="Link to Contributor information"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>Release publishing, hardware acceptance checks and help-site build instructions are in <a href="../site-maintenance/">releases, validation and Pages</a>. Download validation uses HTTPS and hashes; separate signed firmware and secure boot are not implemented.</p>
`,headings:[{id:`choose-an-update-method`,title:`Choose an update method`,level:2},{id:`update-from-the-display`,title:`Update from the display`,level:2},{id:`update-progress-on-the-display`,title:`Update progress on the display`,level:2},{id:`usb-installation-or-recovery`,title:`USB installation or recovery`,level:2},{id:`vesc-tool-updates`,title:`VESC Tool updates`,level:2},{id:`diagnose-before-resetting`,title:`Diagnose before resetting`,level:2},{id:`contributor-information`,title:`Contributor information`,level:2}],search:`Use the display's Wi-Fi update menu for routine updates. Use the browser installer for a first installation or complete USB recovery. Choose an update method Method Use it for What to expect Display Wi-Fi update Routine updates Downloads an image for your board; restart when offered. Browser USB installer First installation or recovery Installs the full flash layout and can reset saved settings. VESC Tool Application updates over BLE or CAN Requires the exact board's application image; restarts automatically after verification. Update from the display Stop the bike, unlock the display and disconnect any phone using its proxy. Open Settings > Wi-Fi and connect to a network. You can enter a hidden network name if needed. Open Settings > Firmware updates and check for a release. Review the version, confirm installation and keep power connected. Wait for verification, then select Restart into update when offered. Vehicle communications pause during installation. If no public release is available, an image is incompatible or validation fails, the display reports the problem and keeps the installed firmware selected. Updates come from the project's GitHub releases. The display checks the download and board identity before activating it. An update that fails early during startup can roll back to the previous image. Update progress on the display Wi-Fi downloads and VESC Tool uploads open a progress panel showing the source, percentage and status. Navigation is blocked during an update. Cancel is available during transfer, then disabled while verification and activation finish. A stopped or rejected update leaves its status visible until you close it. A verified Wi-Fi update offers Restart into update. A verified VESC Tool upload restarts automatically. Cancellation after activation cannot undo the restart. Screen-lock and Bluetooth pairing prompts take priority over the progress panel. USB installation or recovery Read the setup guide for your exact Waveshare or GUITION board. Open the browser installer using a supported desktop browser. Choose the exact model, connect a USB data cable and follow the prompts. After startup, check touch, orientation and saved settings before reconnecting the bike. The complete USB image includes the bootloader and partition layout. It is different from an application-only update image. GUITION USB installation does not update its C6 radio companion. VESC Tool updates Stop the bike and unlock the display. Download the application .bin for your exact display target from the release. Do not select a -factory.bin, bootloader, partition table or GUITION C6 image. Choose one connection route: Route Steps Display BLE with CAN selected Enable Phone connection, connect VESC Tool to the display, and use its local Custom Module identity. Selecting another CAN controller targets that controller instead. Through another VESC on CAN Connect VESC Tool to that VESC, scan the bus and select the display's CAN ID (default 254). The display's CAN transport must be active. Update-only BLE, without a bike or Wi-Fi Disconnect an existing phone session. Open Settings > Firmware updates > Start BLE update mode, then connect VESC Tool to Wheelie Fun Display OTA. In VESC Tool, open Firmware > Custom File, select the matching application image and start the upload. Keep power and the connection available until verification finishes and the display restarts. Update-only BLE uses the usual phone pairing and screen-lock code. Exiting that mode or restarting restores normal operation without changing the saved phone-connection preference or advertised name. During upload, bike polling and tuning pause. A BLE disconnect, CAN session change, screen lock, cancellation or 60 seconds of inactivity aborts the transfer. A remote CAN sender disappearing is detected by the inactivity timeout. CAN has no Bluetooth authentication; physical bus access must be trusted. The updater checks size, checksum, image validity and board identity before selecting the new image. Failed transfers keep the current firmware. Compressed uploads, bootloader writes and broadcast/all-controller flashing are unavailable. Physical BLE and CAN uploads still require hardware acceptance testing. Diagnose before resetting Open Settings > About and inspect Diagnostics. Record the firmware version, board identity, uptime and connection information. For a repeatable problem, note the exact steps and save a serial log if available. Never include PINs or Wi-Fi passwords in a report. Avoid erasing saved settings as a routine fix: this removes preferences, saved bikes, Bluetooth pairings, Wi-Fi details and screen-lock state. Start with troubleshooting. Contributor information Release publishing, hardware acceptance checks and help-site build instructions are in releases, validation and Pages. Download validation uses HTTPS and hashes; separate signed firmware and secure boot are not implemented.`};export{e as default};