var e={source:`docs/connections.md`,route:`connections`,title:`Connections & proxy`,group:`For riders`,description:`BLE, CAN, UART and your phone's transparent VESC connection.`,body:`<p>The display needs one connection to the bike. You can also connect a phone app through the display by enabling its <strong>BLE proxy</strong>: the display acts as a bridge between your phone and the bike.</p>
<h2 id="choose-a-connection">Choose a connection</h2>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Connection</th>
<th>What you need</th>
</tr>
</thead>
<tbody>
<tr>
<td>Bluetooth Low Energy (BLE)</td>
<td>A compatible bike Bluetooth interface. Available by default.</td>
</tr>
<tr>
<td>CAN</td>
<td>A configured firmware build, an external CAN transceiver and matching bus wiring.</td>
</tr>
<tr>
<td>UART</td>
<td>A configured firmware build and compatible serial wiring.</td>
</tr>
</tbody>
</table></div>
<p>Choose the interface under <strong>Settings &gt; Vehicle</strong>. The vehicle integration (Auto, WFB, wheel-e or VESC) is a separate choice in that section. Leave integration at <strong>Auto</strong> for automatic detection.</p>
<h2 id="bluetooth-to-the-bike">Bluetooth to the bike</h2>
<ol>
<li>Power the bike and make its Bluetooth interface available.</li>
<li>Open <strong>Settings &gt; Vehicle</strong> and select the device. Recognizable bike names appear first; other compatible VESC Bluetooth devices are still listed.</li>
<li>Complete any pairing prompt with the PIN configured on the bike's Bluetooth device.</li>
<li>Wait for controller discovery and live readings before using bike controls.</li>
</ol>
<p><img src="../assets/images/ble-pairing.png" alt="Bike Bluetooth pairing prompt"></p>
<p>The display remembers the verified bike and integration. If the bike's Bluetooth PIN changes, pair again using its new PIN. Your display's screen-lock code is separate.</p>
<h2 id="finding-the-main-controller">Finding the main controller</h2>
<p>The display searches the directly connected controller and controllers reachable over CAN. It selects the first compatible motor controller; a BMS is not selected as the motor controller.</p>
<p>With <strong>Auto</strong>, it checks for WFB and wheel-e packages before falling back to standard VESC readings. Selecting WFB or wheel-e explicitly requires that package to respond. A device name does not enable package-specific features.</p>
<p>Check discovery status under <strong>Settings &gt; Vehicle</strong>. Use <strong>Find main controller again</strong> to repeat discovery when the link is available. If a phone connected before discovery finished, disconnect the phone first.</p>
<h2 id="native-can">Native CAN</h2>
<p>CAN appears only when the firmware has both CAN pins configured. The default bus speed is <strong>500 kbit/s</strong> and the display's default node ID is <strong>254</strong>. The bus speed must match the bike, and the display ID must be unique on that bus.</p>
<p>Use a CAN transceiver and the wiring for your exact board. The motor-controller ID is discovered automatically. For the GUITION breakout, see <a href="../hardware/guition-43/can-breakout/">connector pinouts and setup</a>. For custom wiring, see <a href="../building/#configure-an-optional-wired-interface">wired-interface build configuration</a>.</p>
<h2 id="uart">UART</h2>
<p>UART appears only when both UART pins are configured in the firmware. The default baud rate is <strong>115200</strong> and must match the bike.</p>
<p>Use compatible logic levels, connect TX to RX and RX to TX as required, and share a signal ground. Do not guess GPIOs from the screen model. Vehicle UART normally uses port 1; port 0 is reserved for the console. See <a href="../building/#configure-an-optional-wired-interface">wired-interface build configuration</a>.</p>
<h2 id="connect-a-phone-through-the-display">Connect a phone through the display</h2>
<ol>
<li>Unlock the display and enable <strong>Settings &gt; Phone connection &gt; BLE proxy</strong>. It is off by default.</li>
<li>In your phone app, find the display using the name below.</li>
<li>If a screen lock is configured, open <strong>Settings &gt; Phone BLE code</strong> after unlocking and enter that code in the phone's pairing prompt. A pattern uses its six dot numbers. See <a href="../pins/">PINs, patterns and pairing</a>.</li>
<li>Run the phone app's normal VESC discovery and select the controller you want to use.</li>
<li>Disconnect the phone before reading or saving tuning, requesting a bike lock, or updating the display.</li>
</ol>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Active integration</th>
<th>Display name shown to the phone</th>
</tr>
</thead>
<tbody>
<tr>
<td>WFB</td>
<td><code>WFB</code>; the name editor is hidden.</td>
</tr>
<tr>
<td>wheel-e or VESC</td>
<td>Your saved BLE name, initially <code>WB Disp</code>. Edit it under <strong>Settings &gt; Phone connection &gt; BLE name</strong>.</td>
</tr>
</tbody>
</table></div>
<p>Leaving WFB restores your saved name. The display's name does not copy the bike's Bluetooth name.</p>
<h2 id="while-the-phone-is-connected">While the phone is connected</h2>
<p>The green phone icon beside the cog indicates an authorized phone connection. Read-only ride data normally continues. Busy phone traffic or a phone firmware transfer can temporarily pause display readings. Tuning and display firmware updates wait until the phone disconnects.</p>
<p>The phone selects its own CAN controller; it does not automatically use the display's selected controller. Readings from another controller do not replace the display's bike readings. The bridge works with BLE, UART or CAN on the bike side.</p>
<p>The proxy pauses while the screen is locked. With a configured lock, unlock locally after each display restart before connecting the phone. Without a screen lock, the enabled proxy has no PIN requirement.</p>
<h2 id="pairing-and-changed-pins">Pairing and changed PINs</h2>
<p>Changing or removing the screen-lock credential disconnects phone sessions and clears remembered phone pairings on the display. If the phone still remembers the old pairing, forget the display in the phone's Bluetooth settings and pair again.</p>
<p>Changing only the entry mode between PIN and pattern preserves pairings for the same credential. The bike's own Bluetooth pairing remains separate.</p>
<p>Phone pairing and wired operation still need validation with your exact hardware and app. <a href="../troubleshooting/">Troubleshooting</a> covers common connection problems; <a href="../bluetooth/">Bluetooth internals</a> and <a href="../transports/">transport internals</a> explain implementation details.</p>
`,headings:[{id:`choose-a-connection`,title:`Choose a connection`,level:2},{id:`bluetooth-to-the-bike`,title:`Bluetooth to the bike`,level:2},{id:`finding-the-main-controller`,title:`Finding the main controller`,level:2},{id:`native-can`,title:`Native CAN`,level:2},{id:`uart`,title:`UART`,level:2},{id:`connect-a-phone-through-the-display`,title:`Connect a phone through the display`,level:2},{id:`while-the-phone-is-connected`,title:`While the phone is connected`,level:2},{id:`pairing-and-changed-pins`,title:`Pairing and changed PINs`,level:2}],search:`The display needs one connection to the bike. You can also connect a phone app through the display by enabling its BLE proxy: the display acts as a bridge between your phone and the bike. Choose a connection Connection What you need Bluetooth Low Energy (BLE) A compatible bike Bluetooth interface. Available by default. CAN A configured firmware build, an external CAN transceiver and matching bus wiring. UART A configured firmware build and compatible serial wiring. Choose the interface under Settings > Vehicle. The vehicle integration (Auto, WFB, wheel-e or VESC) is a separate choice in that section. Leave integration at Auto for automatic detection. Bluetooth to the bike Power the bike and make its Bluetooth interface available. Open Settings > Vehicle and select the device. Recognizable bike names appear first; other compatible VESC Bluetooth devices are still listed. Complete any pairing prompt with the PIN configured on the bike's Bluetooth device. Wait for controller discovery and live readings before using bike controls. The display remembers the verified bike and integration. If the bike's Bluetooth PIN changes, pair again using its new PIN. Your display's screen-lock code is separate. Finding the main controller The display searches the directly connected controller and controllers reachable over CAN. It selects the first compatible motor controller; a BMS is not selected as the motor controller. With Auto, it checks for WFB and wheel-e packages before falling back to standard VESC readings. Selecting WFB or wheel-e explicitly requires that package to respond. A device name does not enable package-specific features. Check discovery status under Settings > Vehicle. Use Find main controller again to repeat discovery when the link is available. If a phone connected before discovery finished, disconnect the phone first. Native CAN CAN appears only when the firmware has both CAN pins configured. The default bus speed is 500 kbit/s and the display's default node ID is 254. The bus speed must match the bike, and the display ID must be unique on that bus. Use a CAN transceiver and the wiring for your exact board. The motor-controller ID is discovered automatically. For the GUITION breakout, see connector pinouts and setup. For custom wiring, see wired-interface build configuration. UART UART appears only when both UART pins are configured in the firmware. The default baud rate is 115200 and must match the bike. Use compatible logic levels, connect TX to RX and RX to TX as required, and share a signal ground. Do not guess GPIOs from the screen model. Vehicle UART normally uses port 1; port 0 is reserved for the console. See wired-interface build configuration. Connect a phone through the display Unlock the display and enable Settings > Phone connection > BLE proxy. It is off by default. In your phone app, find the display using the name below. If a screen lock is configured, open Settings > Phone BLE code after unlocking and enter that code in the phone's pairing prompt. A pattern uses its six dot numbers. See PINs, patterns and pairing. Run the phone app's normal VESC discovery and select the controller you want to use. Disconnect the phone before reading or saving tuning, requesting a bike lock, or updating the display. Active integration Display name shown to the phone WFB WFB; the name editor is hidden. wheel-e or VESC Your saved BLE name, initially WB Disp. Edit it under Settings > Phone connection > BLE name. Leaving WFB restores your saved name. The display's name does not copy the bike's Bluetooth name. While the phone is connected The green phone icon beside the cog indicates an authorized phone connection. Read-only ride data normally continues. Busy phone traffic or a phone firmware transfer can temporarily pause display readings. Tuning and display firmware updates wait until the phone disconnects. The phone selects its own CAN controller; it does not automatically use the display's selected controller. Readings from another controller do not replace the display's bike readings. The bridge works with BLE, UART or CAN on the bike side. The proxy pauses while the screen is locked. With a configured lock, unlock locally after each display restart before connecting the phone. Without a screen lock, the enabled proxy has no PIN requirement. Pairing and changed PINs Changing or removing the screen-lock credential disconnects phone sessions and clears remembered phone pairings on the display. If the phone still remembers the old pairing, forget the display in the phone's Bluetooth settings and pair again. Changing only the entry mode between PIN and pattern preserves pairings for the same credential. The bike's own Bluetooth pairing remains separate. Phone pairing and wired operation still need validation with your exact hardware and app. Troubleshooting covers common connection problems; Bluetooth internals and transport internals explain implementation details.`};export{e as default};