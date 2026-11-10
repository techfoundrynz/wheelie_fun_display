var e={source:`docs/wheel-e-protocol.md`,route:`reference/wheel-e-protocol`,title:`wheel-e protocol notes`,group:`Reference`,description:`Package identity, modern telemetry, locking and compatibility for wheel-e.`,body:`<p>This document describes the wheel-e contract implemented by Wheelie Fun Display. The reference is <a href="https://github.com/buestad/wheel-e/tree/69382e619bb696ffe54ef899df90fd2115039ba2">buestad/wheel-e commit <code>69382e6</code></a>, whose package name is <code>Wheel-E</code>. Protocol compatibility is established with replies from the connected controller, independently of its Bluetooth name.</p>
<h2 id="supported-contract">Supported contract</h2>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Operation</th>
<th>VESC/application command</th>
<th>Display behavior</th>
</tr>
</thead>
<tbody>
<tr>
<td>Motor-controller identification</td>
<td><code>COMM_FW_VERSION</code> (<code>0</code>)</td>
<td>Requires a valid controller firmware identity; cached routes must also match the saved UUID.</td>
</tr>
<tr>
<td>Package identification</td>
<td><code>COMM_CUSTOM_APP_DATA</code> (<code>36</code>), interface <code>101</code>, <code>INFO</code> (<code>0</code>), version <code>2</code></td>
<td>Requires a complete reply identifying <code>Wheel-E</code>.</td>
</tr>
<tr>
<td>Standard motor telemetry</td>
<td><code>COMM_GET_VALUES_SETUP</code> (<code>47</code>)</td>
<td>Supplies speed, battery, voltage, currents, temperatures and controller fault.</td>
</tr>
<tr>
<td>Package telemetry</td>
<td>Interface <code>101</code>, <code>REALTIME_DATA</code> (<code>33</code>)</td>
<td>Requires the exact float32/state/pitch/roll request described below.</td>
</tr>
<tr>
<td>Bike lock/unlock</td>
<td>Interface <code>101</code>, <code>LOCK</code> (<code>12</code>)</td>
<td>Sends an explicit boolean and confirms the resulting state through fresh telemetry.</td>
</tr>
<tr>
<td>Tuning, configuration, lights and remote throttle</td>
<td>Other wheel-e commands</td>
<td>Not exposed or transmitted by this display. Use VESC Tool for those features.</td>
</tr>
</tbody>
</table></div>
<p>The implementation lives in <a href="https://github.com/techfoundrynz/wheelie_fun_display/blob/main/firmware/components/bike/integration.cpp">the integration</a>, <a href="https://github.com/techfoundrynz/wheelie_fun_display/blob/main/firmware/components/bike/include/bike/controller_discovery.hpp">controller discovery</a> and <a href="https://github.com/techfoundrynz/wheelie_fun_display/blob/main/firmware/components/bike/include/bike/lock_policy.hpp">lock policy</a>. These notes distinguish the upstream protocol from the display's deliberately smaller feature set.</p>
<h2 id="transport-and-envelope">Transport and envelope</h2>
<p>The byte sequences below are complete <strong>VESC payloads</strong>, before serial length/CRC framing or native CAN fragmentation. They can travel through the shared BLE, UART or CAN transport. See <a href="../../transports/">transport internals</a> for framing, CAN forwarding and controller routing.</p>
<p>A wheel-e application payload begins:</p>
<pre><code class="language-text">24 65 CC ...
│  │  └─ wheel-e command, hexadecimal
│  └──── interface 101 (0x65)
└─────── COMM_CUSTOM_APP_DATA, 36 (0x24)
</code></pre>
<p>The upstream command pages omit this three-byte prefix when describing command-specific offsets. Every offset in this document includes it. Multi-byte integers use big-endian byte order. Float32 telemetry uses the upstream <code>buffer_append_float32_auto</code> format: IEEE 754 binary32 in big-endian order, with subnormal values flushed to zero by the encoder. <a href="https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/doc/commands/index.md">Command envelope</a>, <a href="https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/src/conf/buffer.c">numeric encoders</a>.</p>
<p>Interface <code>101</code> is shared with Float/Refloat-family packages. Its presence, or a compatible telemetry response alone, is not a unique wheel-e identity. The display therefore requires the package-name check below before enabling wheel-e branding and features.</p>
<h2 id="discovery-and-package-identity">Discovery and package identity</h2>
<p><strong>Auto</strong> checks supported packages on discovered motor controllers. A manual <strong>wheel-e</strong> selection still performs verification. The relevant sequence is:</p>
<ol>
<li>Obtain a valid motor-controller firmware response, excluding non-motor adapters.</li>
<li>Request <code>INFO</code> version 2 and verify the package name.</li>
<li>Request modern <code>REALTIME_DATA</code> and validate the exact response format.</li>
<li>Enable the wheel-e integration only after both package checks succeed.</li>
</ol>
<p>The standard firmware request <code>[0]</code> and the wheel-e application request <code>[36, 101, 0, 2, 0]</code> are different messages. The first identifies VESC firmware; the second identifies the installed package.</p>
<p>The display stores bounded NVS route hints to accelerate later connections. A hint changes query order only: firmware UUID, package identity and telemetry are checked again. A missing controller, mismatched identity, removed package or failed probe returns to full discovery. See <a href="../../integrations/">vehicle integrations</a> for Auto and manual selection behavior.</p>
<h3 id="info-version-2">INFO version 2</h3>
<p>Request:</p>
<pre><code class="language-text">24 65 00 02 00
         │  └─ flags = 0
         └──── INFO response version requested = 2
</code></pre>
<p>The referenced handler returns this <strong>61-byte</strong> VESC payload:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Offset</th>
<th>Bytes</th>
<th>Field</th>
</tr>
</thead>
<tbody>
<tr>
<td>0</td>
<td>1</td>
<td><code>36</code>: VESC custom application data</td>
</tr>
<tr>
<td>1</td>
<td>1</td>
<td><code>101</code>: package interface</td>
</tr>
<tr>
<td>2</td>
<td>1</td>
<td><code>0</code>: INFO command</td>
</tr>
<tr>
<td>3</td>
<td>1</td>
<td><code>2</code>: actual INFO response version</td>
</tr>
<tr>
<td>4</td>
<td>1</td>
<td>Request flags echoed; <code>0</code> for the display's request</td>
</tr>
<tr>
<td>5</td>
<td>20</td>
<td>Package name, zero-padded fixed-width string</td>
</tr>
<tr>
<td>25</td>
<td>3</td>
<td>Package major, minor and patch version, one byte each</td>
</tr>
<tr>
<td>28</td>
<td>20</td>
<td>Version suffix, zero-padded fixed-width string</td>
</tr>
<tr>
<td>48</td>
<td>4</td>
<td>Build Git hash prefix</td>
</tr>
<tr>
<td>52</td>
<td>4</td>
<td>System tick rate in Hz</td>
</tr>
<tr>
<td>56</td>
<td>4</td>
<td>Package capability flags</td>
</tr>
<tr>
<td>60</td>
<td>1</td>
<td>Extra flags</td>
</tr>
</tbody>
</table></div>
<p>The display accepts a case-insensitive exact <code>Wheel-E</code> name with zero padding, version <code>2</code>, echoed flags <code>0</code>, a nonzero tick rate and the complete layout. <code>Float</code>, <code>Refloat</code>, longer names with a <code>Wheel-E</code> prefix, legacy version-1 replies and truncated replies do not establish wheel-e identity. INFO capability bits are not used to expose additional controls.</p>
<p>The reference build's tick rate is 10,000 Hz. Package version strings are diagnostic information; successful version-2 identity and modern telemetry replies are the compatibility requirements. <a href="https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/doc/commands/INFO.md">INFO specification</a>, <a href="https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/src/main.c#L2652">handler</a>, <a href="https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/package_name">package name</a>.</p>
<h2 id="realtime_data-command-33">REALTIME_DATA, command 33</h2>
<p>The display requests float32 encoding and three fields from mask1: state flags at bit 1, pitch at bit 17 and roll at bit 19. Their combined mask is <code>0x000A0002</code>. Mask2 is zero.</p>
<p>Request, <strong>12 bytes</strong>:</p>
<pre><code class="language-text">24 65 21 01 00 0A 00 02 00 00 00 00
         │  └─────────┘ └─────────┘
         │     mask1       mask2
         └─ control_flags = 1: float32
</code></pre>
<p>Response, <strong>28 bytes</strong>:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Offset</th>
<th>Bytes</th>
<th>Field</th>
<th>Interpretation</th>
</tr>
</thead>
<tbody>
<tr>
<td>0</td>
<td>3</td>
<td>Command prefix</td>
<td><code>[36, 101, 33]</code></td>
</tr>
<tr>
<td>3</td>
<td>1</td>
<td>Control flags</td>
<td>Exactly <code>1</code></td>
</tr>
<tr>
<td>4</td>
<td>4</td>
<td>Mask1</td>
<td>Exactly <code>0x000A0002</code></td>
</tr>
<tr>
<td>8</td>
<td>4</td>
<td>Mask2</td>
<td>Exactly <code>0</code></td>
</tr>
<tr>
<td>12</td>
<td>4</td>
<td>Package time</td>
<td>Unsigned ticks; divide by INFO tick rate for seconds</td>
</tr>
<tr>
<td>16</td>
<td>4</td>
<td>State flags</td>
<td>Packed unsigned integer described below</td>
</tr>
<tr>
<td>20</td>
<td>4</td>
<td>Pitch</td>
<td>Float32 degrees</td>
</tr>
<tr>
<td>24</td>
<td>4</td>
<td>Roll</td>
<td>Float32 degrees</td>
</tr>
</tbody>
</table></div>
<p>Selected fields follow mask-bit order. No float16 variant, additional mask fields or legacy compact telemetry layout is accepted by this decoder. The display rejects incorrect headers, flags or masks, incomplete/extended bodies, unknown run states, non-finite angles and angles outside ±180°. Rejected packets leave the last valid sample unchanged. <a href="https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/doc/commands/REALTIME_DATA.md">REALTIME_DATA specification</a>, <a href="https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/src/main.c#L2404">response encoder</a>.</p>
<h3 id="state-flags">State flags</h3>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Bits</th>
<th>Field</th>
<th>Display use</th>
</tr>
</thead>
<tbody>
<tr>
<td>29–28</td>
<td>Package mode</td>
<td>Not currently projected into the UI</td>
</tr>
<tr>
<td>26–24</td>
<td>Run state</td>
<td>Normalized using the table below</td>
</tr>
<tr>
<td>23–22</td>
<td>Footpad state</td>
<td>Not currently projected into the UI</td>
</tr>
<tr>
<td>21</td>
<td>Charging</td>
<td>Overrides the normalized state to <code>14</code>; blocks locking</td>
</tr>
<tr>
<td>20</td>
<td>Fatal error</td>
<td>Overrides the normalized state to <code>255</code>; blocks locking</td>
</tr>
<tr>
<td>17</td>
<td>Darkride</td>
<td>Not currently projected into the UI</td>
</tr>
<tr>
<td>16</td>
<td>Wheelslip</td>
<td>Not currently projected into the UI</td>
</tr>
<tr>
<td>15–12</td>
<td>Setpoint adjustment type</td>
<td>Not currently projected into the UI</td>
</tr>
<tr>
<td>11–8</td>
<td>Stop condition</td>
<td>Not currently projected into the UI</td>
</tr>
<tr>
<td>7–0</td>
<td>Beep reason</td>
<td>Not currently projected into the UI</td>
</tr>
</tbody>
</table></div>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Raw run state</th>
<th>Upstream name</th>
<th>Display's internal state</th>
</tr>
</thead>
<tbody>
<tr>
<td>0</td>
<td>DISABLED</td>
<td>15</td>
</tr>
<tr>
<td>1</td>
<td>STARTUP</td>
<td>11</td>
</tr>
<tr>
<td>2</td>
<td>READY</td>
<td>0</td>
</tr>
<tr>
<td>3</td>
<td>RUNNING</td>
<td>1</td>
</tr>
<tr>
<td>4</td>
<td>THROTTLE</td>
<td>16</td>
</tr>
<tr>
<td>5</td>
<td>CRUISE</td>
<td>17</td>
</tr>
</tbody>
</table></div>
<p>These are modern wheel-e states, not the four-bit legacy Float compatibility states. Keeping THROTTLE and CRUISE distinct allows the lock policy to reject cruise while considering stationary throttle mode. If charging and fatal error are both set, fatal error takes precedence. <a href="https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/src/state.h">State definitions</a>, <a href="https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/src/main.c#L2278">flag encoder</a>.</p>
<h2 id="standard-telemetry-and-freshness">Standard telemetry and freshness</h2>
<p>After verification, the display polls standard VESC setup telemetry (<code>47</code>) together with wheel-e realtime data (<code>33</code>) at a target of 10 Hz per family, subject to outstanding-request credits and phone arbitration. Standard telemetry supplies signed speed in m/s, voltage, battery percentage, motor/battery current, Celsius temperatures and controller fault. The UI converts speed and temperature into the independently selected display units.</p>
<p>Package data supplies pitch, roll and run state. Standard telemetry is not a substitute for a missing package-identity or package-telemetry reply. Auto may instead select generic VESC when no supported package verifies; that integration has standard read-only telemetry and no wheel-e lock or tuning features.</p>
<p>Setup and package freshness expire independently after 1,500 ms, measured from local receive time. The package timestamp is not used to schedule polling or establish freshness. A connection/route change clears prior telemetry and pending commands. Details are in <a href="https://github.com/techfoundrynz/wheelie_fun_display/blob/main/firmware/components/bike/telemetry.cpp">telemetry parsing</a> and <a href="../../architecture/">architecture</a>.</p>
<h2 id="lock-command-12">LOCK, command 12</h2>
<p>The request contains one boolean byte, with no PIN or acknowledgement field:</p>
<pre><code class="language-text">24 65 0C 01    lock:   disabled = true
24 65 0C 00    unlock: disabled = false
</code></pre>
<p>The upstream handler acts only when the run state is not RUNNING. It reloads the persisted configuration, updates its disabled flag, changes the package state and writes configuration back to EEPROM. Unlocking from DISABLED enters STARTUP. The handler does not send a dedicated success response. This persistence also means temporary controller-side tuning is not retained by the lock handler. <a href="https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/src/main.c#L1754">LOCK handler</a>, <a href="https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/src/state.c#L63">state transition</a>.</p>
<p>The display applies stricter write guards: current-session setup and package telemetry must both be fresh, speed magnitude below 0.2 m/s, motor-current magnitude below 2 A and controller fault zero. RUNNING, CRUISE, charging and fatal-error states are rejected. Stationary THROTTLE can pass these guards.</p>
<p>A queued write is not confirmation. The display requires fresh telemetry from the same session, received more than 400 ms after the request: DISABLED confirms locking; a permitted stopped non-disabled state confirms unlocking. A five-second timeout, disconnect or stale data leaves the operation unconfirmed. Commands are not automatically replayed after reconnect. The six-digit <a href="../../pins/">display PIN</a> is local access control, separate from this wheel-e boolean command.</p>
<h2 id="compatibility-limits">Compatibility limits</h2>
<p>The supported contract requires INFO version 2 identifying <code>Wheel-E</code> and modern REALTIME_DATA command 33 with the exact format above. Older wheel-e releases without this identity response remain unsupported by the wheel-e integration. Other interface-101 packages remain eligible for generic read-only VESC telemetry through Auto, but do not receive wheel-e branding or write capabilities.</p>
<p>Upstream marks <code>GET_RTDATA</code> (<code>1</code>) and <code>GET_ALLDATA</code> (<code>10</code>) deprecated. Internal realtime commands <code>31</code>/<code>32</code> do not provide the compatibility contract used here. This display does not send WFB-specific ride commands <code>100</code>/<code>103</code> to interface 101, and does not implement wheel-e configuration or throttle commands. <a href="https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/doc/commands/index.md">Upstream command index</a>, <a href="https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/src/main.c#L1581">command IDs and dispatch</a>.</p>
<p>Portable tests cover identity validation, probe order, cached-hint re-verification, response sizes and masks, state normalization and lock confirmation. Actual controller firmware, phone interoperability and physical lock behavior still need hardware acceptance; source compatibility and simulator checks do not establish those results. See <a href="../../todo/">planned work</a>.</p>
`,headings:[{id:`supported-contract`,title:`Supported contract`,level:2},{id:`transport-and-envelope`,title:`Transport and envelope`,level:2},{id:`discovery-and-package-identity`,title:`Discovery and package identity`,level:2},{id:`info-version-2`,title:`INFO version 2`,level:3},{id:`realtime_data-command-33`,title:`REALTIME_DATA, command 33`,level:2},{id:`state-flags`,title:`State flags`,level:3},{id:`standard-telemetry-and-freshness`,title:`Standard telemetry and freshness`,level:2},{id:`lock-command-12`,title:`LOCK, command 12`,level:2},{id:`compatibility-limits`,title:`Compatibility limits`,level:2}],search:`This document describes the wheel-e contract implemented by Wheelie Fun Display. The reference is buestad/wheel-e commit 69382e6, whose package name is Wheel-E. Protocol compatibility is established with replies from the connected controller, independently of its Bluetooth name. Supported contract Operation VESC/application command Display behavior Motor-controller identification COMM_FW_VERSION (0) Requires a valid controller firmware identity; cached routes must also match the saved UUID. Package identification COMM_CUSTOM_APP_DATA (36), interface 101, INFO (0), version 2 Requires a complete reply identifying Wheel-E. Standard motor telemetry COMM_GET_VALUES_SETUP (47) Supplies speed, battery, voltage, currents, temperatures and controller fault. Package telemetry Interface 101, REALTIME_DATA (33) Requires the exact float32/state/pitch/roll request described below. Bike lock/unlock Interface 101, LOCK (12) Sends an explicit boolean and confirms the resulting state through fresh telemetry. Tuning, configuration, lights and remote throttle Other wheel-e commands Not exposed or transmitted by this display. Use VESC Tool for those features. The implementation lives in the integration, controller discovery and lock policy. These notes distinguish the upstream protocol from the display's deliberately smaller feature set. Transport and envelope The byte sequences below are complete VESC payloads, before serial length/CRC framing or native CAN fragmentation. They can travel through the shared BLE, UART or CAN transport. See transport internals for framing, CAN forwarding and controller routing. A wheel-e application payload begins: 24 65 CC ... │ │ └─ wheel-e command, hexadecimal │ └──── interface 101 (0x65) └─────── COMM_CUSTOM_APP_DATA, 36 (0x24) The upstream command pages omit this three-byte prefix when describing command-specific offsets. Every offset in this document includes it. Multi-byte integers use big-endian byte order. Float32 telemetry uses the upstream buffer_append_float32_auto format: IEEE 754 binary32 in big-endian order, with subnormal values flushed to zero by the encoder. Command envelope, numeric encoders. Interface 101 is shared with Float/Refloat-family packages. Its presence, or a compatible telemetry response alone, is not a unique wheel-e identity. The display therefore requires the package-name check below before enabling wheel-e branding and features. Discovery and package identity Auto checks supported packages on discovered motor controllers. A manual wheel-e selection still performs verification. The relevant sequence is: Obtain a valid motor-controller firmware response, excluding non-motor adapters. Request INFO version 2 and verify the package name. Request modern REALTIME_DATA and validate the exact response format. Enable the wheel-e integration only after both package checks succeed. The standard firmware request [0] and the wheel-e application request [36, 101, 0, 2, 0] are different messages. The first identifies VESC firmware; the second identifies the installed package. The display stores bounded NVS route hints to accelerate later connections. A hint changes query order only: firmware UUID, package identity and telemetry are checked again. A missing controller, mismatched identity, removed package or failed probe returns to full discovery. See vehicle integrations for Auto and manual selection behavior. INFO version 2 Request: 24 65 00 02 00 │ └─ flags = 0 └──── INFO response version requested = 2 The referenced handler returns this 61-byte VESC payload: Offset Bytes Field 0 1 36: VESC custom application data 1 1 101: package interface 2 1 0: INFO command 3 1 2: actual INFO response version 4 1 Request flags echoed; 0 for the display's request 5 20 Package name, zero-padded fixed-width string 25 3 Package major, minor and patch version, one byte each 28 20 Version suffix, zero-padded fixed-width string 48 4 Build Git hash prefix 52 4 System tick rate in Hz 56 4 Package capability flags 60 1 Extra flags The display accepts a case-insensitive exact Wheel-E name with zero padding, version 2, echoed flags 0, a nonzero tick rate and the complete layout. Float, Refloat, longer names with a Wheel-E prefix, legacy version-1 replies and truncated replies do not establish wheel-e identity. INFO capability bits are not used to expose additional controls. The reference build's tick rate is 10,000 Hz. Package version strings are diagnostic information; successful version-2 identity and modern telemetry replies are the compatibility requirements. INFO specification, handler, package name. REALTIME_DATA, command 33 The display requests float32 encoding and three fields from mask1: state flags at bit 1, pitch at bit 17 and roll at bit 19. Their combined mask is 0x000A0002. Mask2 is zero. Request, 12 bytes: 24 65 21 01 00 0A 00 02 00 00 00 00 │ └─────────┘ └─────────┘ │ mask1 mask2 └─ control_flags = 1: float32 Response, 28 bytes: Offset Bytes Field Interpretation 0 3 Command prefix [36, 101, 33] 3 1 Control flags Exactly 1 4 4 Mask1 Exactly 0x000A0002 8 4 Mask2 Exactly 0 12 4 Package time Unsigned ticks; divide by INFO tick rate for seconds 16 4 State flags Packed unsigned integer described below 20 4 Pitch Float32 degrees 24 4 Roll Float32 degrees Selected fields follow mask-bit order. No float16 variant, additional mask fields or legacy compact telemetry layout is accepted by this decoder. The display rejects incorrect headers, flags or masks, incomplete/extended bodies, unknown run states, non-finite angles and angles outside ±180°. Rejected packets leave the last valid sample unchanged. REALTIME_DATA specification, response encoder. State flags Bits Field Display use 29–28 Package mode Not currently projected into the UI 26–24 Run state Normalized using the table below 23–22 Footpad state Not currently projected into the UI 21 Charging Overrides the normalized state to 14; blocks locking 20 Fatal error Overrides the normalized state to 255; blocks locking 17 Darkride Not currently projected into the UI 16 Wheelslip Not currently projected into the UI 15–12 Setpoint adjustment type Not currently projected into the UI 11–8 Stop condition Not currently projected into the UI 7–0 Beep reason Not currently projected into the UI Raw run state Upstream name Display's internal state 0 DISABLED 15 1 STARTUP 11 2 READY 0 3 RUNNING 1 4 THROTTLE 16 5 CRUISE 17 These are modern wheel-e states, not the four-bit legacy Float compatibility states. Keeping THROTTLE and CRUISE distinct allows the lock policy to reject cruise while considering stationary throttle mode. If charging and fatal error are both set, fatal error takes precedence. State definitions, flag encoder. Standard telemetry and freshness After verification, the display polls standard VESC setup telemetry (47) together with wheel-e realtime data (33) at a target of 10 Hz per family, subject to outstanding-request credits and phone arbitration. Standard telemetry supplies signed speed in m/s, volta`};export{e as default};