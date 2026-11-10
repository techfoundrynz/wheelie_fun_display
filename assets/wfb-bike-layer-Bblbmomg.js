var e={source:`docs/wheelie-wfb-bike-layer.md`,route:`reference/wfb-bike-layer`,title:`WFB bike layer`,group:`Reference`,description:`WFB command layouts and configuration details.`,body:`<p>Companion to <code>wheelie-vesc-comms-api.md</code>. Everything here is the vendor-specific
layer that sits on top of <code>COMM_CUSTOM_APP_DATA</code>.</p>
<hr>
<h2 id="1-frame-shape">1. Frame shape<a class="heading-anchor" href="#1-frame-shape" aria-label="Link to 1. Frame shape" title="Link to 1. Frame shape"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>Every bike command is one VESC packet:</p>
<pre><code>0x02  len  36  102  &lt;sub&gt;  &lt;payload…&gt;  crc16  0x03
       │    │   │     │
       │    │   │     └── FloatPacketTypes sub-command
       │    │   └────────  AppPacketTypes.WFB = 102
       │    └────────────  COMM_CUSTOM_APP_DATA = 36
       └─────────────────  length byte
</code></pre>
<p><code>getVescAppData</code> reads bytes 0 and 1 of the payload and <strong>returns <code>null</code> unless
byte 0 is 102</strong>. Stock Float package boards advertise 101, so this app is a
closed loop with its own firmware package — it won't read a Float or Refloat
board even though it understands the config format.</p>
<p>Two send helpers exist:</p>
<ul>
<li><code>writeData(...bytes)</code> — fire and forget, used by the three configure commands.</li>
<li><code>getData(...bytes)</code> — writes and awaits a matching response, used for polling.</li>
</ul>
<p>Both prepend <code>[COMM_FORWARD_CAN, canId]</code> automatically when
<code>_primaryCANDeviceId</code> is set.</p>
<hr>
<h2 id="2-sub-command-100-bike_ride_configure">2. Sub-command 100 — <code>BIKE_RIDE_CONFIGURE</code><a class="heading-anchor" href="#2-sub-command-100-bike_ride_configure" aria-label="Link to 2. Sub-command 100 — BIKE_RIDE_CONFIGURE" title="Link to 2. Sub-command 100 — BIKE_RIDE_CONFIGURE"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>Sets the four motor-response parameters as a <strong>3-byte packed payload</strong>. Full
frame is 6 bytes of payload:</p>
<pre><code>36  102  100  b0  b1  b2
</code></pre>
<p>Built by <code>createRideModeCommand(maxCurrent, maxCurrentBrake, rampAlphaPos, rampAlphaNeg)</code>:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Byte</th>
<th>Content</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>b0</code> low nibble</td>
<td><code>clamp(round(maxCurrent / 10), 0, 15)</code></td>
</tr>
<tr>
<td><code>b0</code> high nibble</td>
<td><code>clamp(round(abs(maxCurrentBrake) / 10), 0, 15)</code></td>
</tr>
<tr>
<td><code>b1</code></td>
<td><code>rampAlphaToByte(rampAlphaPos)</code></td>
</tr>
<tr>
<td><code>b2</code></td>
<td><code>rampAlphaToByte(rampAlphaNeg)</code></td>
</tr>
</tbody>
</table></div>
<p>So currents are transmitted in <strong>10 A units, 4 bits each</strong>, brake current as an
absolute value in the high nibble. Range on the wire is 0–150 A in 10 A steps —
which is exactly why <code>max_current</code> in the config table tops out at 150.</p>
<h3 id="rampalphatobytea-piecewise-non-linear-quantisation"><code>rampAlphaToByte(a)</code> — piecewise non-linear quantisation</h3>
<p>A single byte covering four orders of magnitude, with finer resolution at the
low (smoother) end:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Input range</th>
<th>Byte value</th>
<th>Resolution</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>a &lt;= 0.0001</code></td>
<td><code>0</code></td>
<td>—</td>
</tr>
<tr>
<td><code>0.0001 &lt; a &lt;= 0.001</code></td>
<td><code>round((a − 0.0001) / 0.0001)</code> → 0–9</td>
<td>0.0001</td>
</tr>
<tr>
<td><code>0.001 &lt; a &lt;= 0.01</code></td>
<td><code>round(10 + (a − 0.001) / 0.0005)</code> → 10–28</td>
<td>0.0005</td>
</tr>
<tr>
<td><code>0.01 &lt; a &lt;= 0.1</code></td>
<td><code>round(28 + (a − 0.01) / 0.001)</code> → 28–118</td>
<td>0.001</td>
</tr>
<tr>
<td><code>0.1 &lt; a &lt;= 0.5</code></td>
<td><code>round(118 + (a − 0.1) / 0.005)</code> → 118–198</td>
<td>0.005</td>
</tr>
<tr>
<td><code>0.5 &lt; a &lt; 1</code></td>
<td><code>round(198 + (a − 0.5) / 0.01)</code> → 198–248</td>
<td>0.01</td>
</tr>
<tr>
<td><code>a &gt;= 1</code></td>
<td><code>248</code></td>
<td>—</td>
</tr>
</tbody>
</table></div>
<p>The bands join cleanly (0.001→10, 0.01→28, 0.1→118, 0.5→198, 1.0→248), so this
is a deliberate companding curve, not accumulated drift. Byte values 249–255
are unreachable.</p>
<h3 id="ride-mode-presets">Ride mode presets</h3>
<p><code>RIDE_MODE_PARAMS</code>:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>id</th>
<th>maxCurrent</th>
<th>maxCurrentBrake</th>
<th>rampAlphaPos</th>
<th>rampAlphaNeg</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>sport</code></td>
<td>150</td>
<td>−90</td>
<td>0.015</td>
<td>0.008</td>
</tr>
<tr>
<td><code>cruise</code></td>
<td>150</td>
<td>−90</td>
<td>0.003</td>
<td>0.008</td>
</tr>
<tr>
<td><code>eco</code></td>
<td>90</td>
<td>−90</td>
<td>0.002</td>
<td>0.005</td>
</tr>
<tr>
<td><code>custom</code></td>
<td>150</td>
<td>−90</td>
<td>0.003</td>
<td>0.008</td>
</tr>
</tbody>
</table></div>
<p><code>sport</code> and <code>cruise</code> differ only in throttle ramp; braking is identical across
all four presets. <code>custom</code> is seeded from <code>cruise</code>.</p>
<p>UI metadata: Sport <code>#AC0E0E</code> "High performance mode with near-instant throttle
response", Cruise <code>#0E77AC</code> "Smooth, predictable power delivery suitable for
most riders", Eco <code>#198E13</code> "Limited acceleration and speed for a more relaxed
and efficient ride", Custom <code>#777777</code>, plus an <code>unknown</code> pseudo-mode
("Configuration doesn't match any known mode").</p>
<h3 id="custom-mode-slider-parameter-mapping">Custom mode: slider → parameter mapping</h3>
<p>When the user picks Custom, the four values come from sliders:</p>
<pre><code>maxCurrent      =  60 + throttleCurrent * 10
maxCurrentBrake = −(60 + brakeCurrent   * 10)
rampAlphaPos    =  throttleRamp === 10 ? 1 : RAMP_VALUES[throttleRamp − 1]
rampAlphaNeg    =  brakeRamp    === 10 ? 1 : RAMP_VALUES[brakeRamp    − 1]
</code></pre>
<p><code>RAMP_VALUES = [0.0005, 0.001, 0.002, 0.003, 0.005, 0.008, 0.015, 0.03, 0.06, 0.1]</code></p>
<p>Slider position 10 short-circuits to <code>rampAlpha = 1</code> (instant response,
byte 248) rather than reading <code>RAMP_VALUES[9] = 0.1</code>. <strong>Index 9 of the table is
therefore dead</strong> — position 9 gives 0.06 and position 10 jumps straight to 1.0.
Whether that's intentional ("10 = instant") or an off-by-one is unclear from
the bytecode; the effect is a large discontinuity at the top of the slider.</p>
<h3 id="reverse-mapping-determineinitialridemode">Reverse mapping — <code>determineInitialRideMode</code></h3>
<p>On connect, the app reads the live config and tries to match it back to a
preset using <code>isCloseEnough(a, b, epsilon = 0.001)</code> on each of the four values.
No match → the <code>unknown</code> pseudo-mode. A match on <code>custom</code>'s values →
<code>'custom'</code>. This is how the UI shows which mode a board is currently in without
the firmware reporting a mode ID.</p>
<p>Logging is verbose here: <code>"Sending ride mode command for &lt;name&gt;:"</code> with the
resolved values and <code>commandBytes</code>, then <code>"Successfully sent ride mode configuration for &lt;name&gt;"</code>. <code>"Cannot select unknown mode"</code> throws if the
<code>unknown</code> pseudo-mode is somehow selected.</p>
<hr>
<h2 id="3-sub-command-101-bike_wheelie_configure">3. Sub-command 101 — <code>BIKE_WHEELIE_CONFIGURE</code><a class="heading-anchor" href="#3-sub-command-101-bike_wheelie_configure" aria-label="Link to 3. Sub-command 101 — BIKE_WHEELIE_CONFIGURE" title="Link to 3. Sub-command 101 — BIKE_WHEELIE_CONFIGURE"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p><strong>Defined but not transmitted by app version 1.0.0.</strong> A supported payload
encoding for this sub-command is not established here.</p>
<p>The associated UI exists but is largely disabled. <code>WHEELIE_MODES</code>:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>id</th>
<th>name</th>
<th>icon</th>
<th>colour</th>
<th>disabled</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>agro</code></td>
<td>Agro</td>
<td><code>3</code></td>
<td><code>#AC0E0E</code></td>
<td><strong>true</strong></td>
</tr>
<tr>
<td><code>standard</code></td>
<td>Standard</td>
<td><code>2</code></td>
<td><code>#0E77AC</code></td>
<td>false</td>
</tr>
<tr>
<td><code>float</code></td>
<td>Float</td>
<td><code>1</code></td>
<td><code>#198E13</code></td>
<td><strong>true</strong></td>
</tr>
</tbody>
</table></div>
<p>Descriptions: Agro — "Stiff, locked in response; sticks to set wheelie angle at
all costs"; Standard — "Secure feeling wheelie with some give for a smooth, fun
ride"; Float — "Soft, loose wheelie for a more floaty feel".</p>
<p>The mode picker includes selection and cancellation state, but two options
are disabled and no transmit path is documented. These controls do not establish
a supported sub-command 101 payload. Separate <code>WHEELIE_CONFIG_SIGNATURES</code> and
<code>isWheelieConfig</code> identifiers suggest another config profile may have been
planned; that remains unconfirmed.</p>
<p>Wheelie <em>tuning</em> in this build goes through the ordinary
<code>COMM_SET_CUSTOM_CONFIG</code> path instead (the <code>WHEELIE_KP*</code> / <code>MAHONY_KP</code> params in
§5), not through sub-command 101.</p>
<hr>
<h2 id="4-sub-command-102-bike_feature_configure">4. Sub-command 102 — <code>BIKE_FEATURE_CONFIGURE</code><a class="heading-anchor" href="#4-sub-command-102-bike_feature_configure" aria-label="Link to 4. Sub-command 102 — BIKE_FEATURE_CONFIGURE" title="Link to 4. Sub-command 102 — BIKE_FEATURE_CONFIGURE"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>The big one: <strong>13 payload bytes</strong>, all scalar, no length prefix. Full frame
payload is 16 bytes.</p>
<pre><code>36  102  102  b0 b1 b2 b3 b4 b5 b6 b7 b8 b9 b10 b11 b12
</code></pre>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>#</th>
<th>Field</th>
<th>Encoding</th>
<th>Wire range</th>
</tr>
</thead>
<tbody>
<tr>
<td>0</td>
<td><code>selectedFunction</code></td>
<td><code>{wheelie:0, turbo:1, horn:2, disabled:3}</code>, unknown → 0, then <code>min(x, 3)</code></td>
<td>0–3</td>
</tr>
<tr>
<td>1</td>
<td><code>wheelieAngle</code></td>
<td><code>clamp(round(deg × 2), 10, 110)</code></td>
<td>10–110 (5–55°)</td>
</tr>
<tr>
<td>2</td>
<td><code>throttleBrakeTilt</code></td>
<td><code>bool ? 1 : 0</code></td>
<td>0/1</td>
</tr>
<tr>
<td>3</td>
<td><code>throttleTiltAngle</code></td>
<td><code>clamp(round((deg + 20) × 2), 0, 80)</code></td>
<td>0–80 (−20…+20°)</td>
</tr>
<tr>
<td>4</td>
<td><code>brakeTiltAngle</code></td>
<td><code>clamp(round((deg + 20) × 2), 0, 80)</code></td>
<td>0–80 (−20…+20°)</td>
</tr>
<tr>
<td>5</td>
<td><code>persistWheelieThrottle</code></td>
<td><code>bool ? 1 : 0</code></td>
<td>0/1</td>
</tr>
<tr>
<td>6</td>
<td><code>persistWheelieBrake</code></td>
<td><code>bool ? 1 : 0</code></td>
<td>0/1</td>
</tr>
<tr>
<td>7</td>
<td><code>antiLoopEnabled</code></td>
<td><code>bool ? 1 : 0</code></td>
<td>0/1</td>
</tr>
<tr>
<td>8</td>
<td><code>sameAngleAsButton</code></td>
<td><code>bool ? 1 : 0</code></td>
<td>0/1</td>
</tr>
<tr>
<td>9</td>
<td><code>antiLoopAngle</code></td>
<td><code>clamp(round(deg × 2), 10, 110)</code></td>
<td>10–110 (5–55°)</td>
</tr>
<tr>
<td>10</td>
<td><code>wheelLiftResistance</code></td>
<td><code>clamp(round(x × 10), 0, 30)</code></td>
<td>0–30 (0–3.0)</td>
</tr>
<tr>
<td>11</td>
<td><code>hornStrength</code></td>
<td><code>hornStrengthUiToWireByte(ui)</code>, clamped 0–250</td>
<td>10–100</td>
</tr>
<tr>
<td>12</td>
<td><code>hornNoteIndex</code></td>
<td><code>clamp(round(i), 0, 27)</code></td>
<td>0–27</td>
</tr>
</tbody>
</table></div>
<p>Two encoding families worth noting: <strong>angles are ×2 with a +20 bias</strong> where
they can go negative (tilt angles), and <strong>plain ×2</strong> where they can't (wheelie
and anti-loop angles, both 5–55°). The +20/×2 bias is what lets a signed
−20…+20° range fit an unsigned byte.</p>
<p><code>hornStrengthUiToWireByte(ui) = max(10, min(100, round(ui × 10)))</code> — UI 1–10
maps to wire 10–100. The inverse used when reading firmware state is
<code>hornStrengthFirmwareToUi(fw) = max(1, min(10, round(fw)))</code>, which does <strong>not</strong>
divide by 10. The two paths disagree by a factor of 10; the <code>horn_strength</code>
config field (§5) is a 0–100 double with default 4, and the UI default is also
4, so the round-trip only stays consistent because the config path and the
sub-command path are used in different places. Flagging as a likely bug.</p>
<h3 id="horn-note-table">Horn note table</h3>
<p><code>HORN_NOTE_LABELS</code> / <code>HORN_NOTE_HZ_TABLE</code>, 28 entries, G#3 through B5:</p>
<pre><code>G#3 208   A3 220   A#3 233   B3 247   C4 262   C#4 277   D4 294
D#4 311   E4 330   F4 349   F#4 370   G4 392   G#4 415   A4 440
A#4 466   B4 494   C5 523   C#5 554   D5 587   D#5 622   E5 659
F5 698    F#5 740  G5 784    G#5 831   A5 880   A#5 932   B5 988
</code></pre>
<p><code>DEFAULT_HORN_NOTE_INDEX_B4 = 15</code> → 494 Hz, matching the <code>horn_frequency</code>
config default exactly. Helpers: <code>hornNoteLabel(i)</code> and
<code>nearestHornNoteIndexFromHz(hz)</code> for converting a firmware-reported frequency
back to a table index. The horn is produced by motor haptics, not a speaker —
per the <code>button_function</code> description, "Trigger a horn sound through motor
haptics."</p>
<h3 id="not-transmitted">Not transmitted</h3>
<p><code>coastingBrakeStrength</code> is held in component state and written back to local
config after a successful save, but <strong>is not one of the 13 bytes</strong>. Either
it's carried by a different path or the feature is incomplete. Its default,
<code>DEFAULT_COASTING_BRAKE_STRENGTH</code>, is 0.</p>
<h3 id="defaults">Defaults</h3>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Constant</th>
<th>Value</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>DEFAULT_WHEELIE_ANGLE</code></td>
<td>30</td>
</tr>
<tr>
<td><code>DEFAULT_SELECTED_FUNCTION</code></td>
<td><code>'wheelie'</code></td>
</tr>
<tr>
<td><code>DEFAULT_THROTTLE_BRAKE_TILT</code></td>
<td><code>false</code></td>
</tr>
<tr>
<td><code>DEFAULT_THROTTLE_TILT_ANGLE</code></td>
<td>5</td>
</tr>
<tr>
<td><code>DEFAULT_BRAKE_TILT_ANGLE</code></td>
<td>−5</td>
</tr>
<tr>
<td><code>DEFAULT_PERSIST_WHEELIE_THROTTLE</code></td>
<td><code>false</code></td>
</tr>
<tr>
<td><code>DEFAULT_PERSIST_WHEELIE_BRAKE</code></td>
<td><code>false</code></td>
</tr>
<tr>
<td><code>DEFAULT_ANTI_LOOP_ENABLED</code></td>
<td><code>true</code></td>
</tr>
<tr>
<td><code>DEFAULT_ANTI_LOOP_ANGLE</code></td>
<td>30</td>
</tr>
<tr>
<td><code>DEFAULT_SAME_ANGLE_AS_BUTTON</code></td>
<td><code>true</code></td>
</tr>
<tr>
<td><code>DEFAULT_WHEEL_LIFT_RESISTANCE</code></td>
<td>1.5</td>
</tr>
<tr>
<td><code>WHEEL_LIFT_RESISTANCE_STEP</code></td>
<td>0.3</td>
</tr>
<tr>
<td><code>WHEEL_LIFT_RESISTANCE_MAX</code></td>
<td>3</td>
</tr>
<tr>
<td><code>WHEEL_LIFT_DISPLAY_SCALE</code></td>
<td>3.3333333333333335</td>
</tr>
<tr>
<td><code>DEFAULT_COASTING_BRAKE_STRENGTH</code></td>
<td>0</td>
</tr>
<tr>
<td><code>DEFAULT_HORN_STRENGTH_UI</code></td>
<td>4</td>
</tr>
<tr>
<td><code>OVERHEAT_THRESHOLD</code></td>
<td>75</td>
</tr>
</tbody>
</table></div>
<p><code>wheelLiftResistanceToDisplay(x) = round(x × 3.3333…)</code> turns the 0–3.0
internal value into a 0–10 display scale; <code>snapWheelLiftResistance</code> quantises
user input to the 0.3 step.</p>
<hr>
<h2 id="5-sub-command-103-bike_speed_limit_configure">5. Sub-command 103 — <code>BIKE_SPEED_LIMIT_CONFIGURE</code><a class="heading-anchor" href="#5-sub-command-103-bike_speed_limit_configure" aria-label="Link to 5. Sub-command 103 — BIKE_SPEED_LIMIT_CONFIGURE" title="Link to 5. Sub-command 103 — BIKE_SPEED_LIMIT_CONFIGURE"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>Single-byte payload:</p>
<pre><code>36  102  103  &lt;mph&gt;
</code></pre>
<p><code>speedLimitMphToWireByte(mph)</code>:</p>
<pre><code class="language-js">n = round(mph)
return (n &lt;= 0 || n &gt; SPEED_LIMIT_MAX) ? 0 : n
</code></pre>
<p>So the value is transmitted as <strong>whole mph, unscaled</strong>, and anything out of
range collapses to <code>0</code>. <code>SPEED_LIMIT_MIN = 5</code>, <code>SPEED_LIMIT_MAX = 35</code>,
<code>DEFAULT_SPEED_LIMIT = 20</code>. Note the guard is <code>n &lt;= 0</code>, not <code>n &lt; SPEED_LIMIT_MIN</code>
— values 1–4 pass through to the wire even though the UI won't produce them.</p>
<p><code>0</code> evidently means "no limit" rather than "limit of zero": the sibling helper
<code>speedLimitIconKey</code> uses the identical guard and returns the string <code>'none'</code>
for the same inputs, which is what the UI renders.</p>
<p>The send path is a save button with an optimistic-UI wrapper: on failure it
logs <code>"Failed to execute BIKE_SPEED_LIMIT_CONFIGURE command:"</code> and shows a
"Save Failed" / "Failed to save speed limit settings." alert; there's a
300 ms debounce branch and an "Unsaved Changes" discard confirmation on close.</p>
<p>The config table's <code>speed_limit</code> field has default <strong>19</strong>, while
<code>DEFAULT_SPEED_LIMIT</code> is <strong>20</strong> — the firmware default and the app's UI default
disagree by 1.</p>
<hr>
<h2 id="6-bike_config-the-full-custom-config-profile">6. <code>bike_config</code> — the full custom-config profile<a class="heading-anchor" href="#6-bike_config-the-full-custom-config-profile" aria-label="Link to 6. bike_config — the full custom-config profile" title="Link to 6. bike_config — the full custom-config profile"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>Read with <code>COMM_GET_CUSTOM_CONFIG</code> (93), written with <code>COMM_SET_CUSTOM_CONFIG</code>
(95). This is a separate channel from the 100–103 sub-commands: the sub-commands
are live/volatile pushes, the custom config is the persisted profile.</p>
<h3 id="write-payload-getfloatconfigpayloadversion-config">Write payload — <code>getFloatConfigPayload(version, config)</code></h3>
<pre><code>u8   COMM_SET_CUSTOM_CONFIG (95)
u8   0                       // config slot index
u32  floatSignatures[version]
...  floatConfigEncoders[version](config)
</code></pre>
<p>Buffer is pre-allocated at 250 bytes. An unknown version throws
<code>"Unsupported Package Version: &lt;v&gt;"</code>.</p>
<h3 id="read-parse-getfloatconfbuffer">Read parse — <code>getFloatConf(buffer)</code></h3>
<pre><code>u8   (discarded)
u32  signature   →  getWfbVersionForSignature()  →  parser table
</code></pre>
<p>Only <strong>version 1</strong> is registered. Unrecognised signature → <code>null</code> → the user
sees "Package Version Unsupported or failed to get config". The signature is
computed at runtime from the parameter table. Its numeric value is not
specified here and must be verified before writing configuration.</p>
<h3 id="wire-types">Wire types</h3>
<p><code>vTx</code> follows VESC Tool's <code>VESC_TX_*</code> enum (<em>inferred</em> from the observed values
and the accompanying scales):</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>vTx</th>
<th>Type</th>
<th>Used by</th>
</tr>
</thead>
<tbody>
<tr>
<td>1</td>
<td>uint8</td>
<td><code>speed_limit</code></td>
</tr>
<tr>
<td>3</td>
<td>uint16</td>
<td><code>horn_frequency</code></td>
</tr>
<tr>
<td>4</td>
<td>int16</td>
<td><code>max_current</code>, <code>max_current_brake</code></td>
</tr>
<tr>
<td>7</td>
<td>double16 with <code>vTxDoubleScale</code></td>
<td>all the tuning doubles</td>
</tr>
</tbody>
</table></div>
<p><code>type</code> is the editor type, not the wire type: <code>0</code> label, <code>1</code> double, <code>2</code> int,
<code>3</code> string, <code>4</code> enum, <code>5</code> bool.</p>
<h3 id="the-27-parameters">The 27 parameters</h3>
<p>Grouped by function rather than declaration order.</p>
<p><strong>Identity / control</strong></p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Name</th>
<th>cDefine</th>
<th>Wire</th>
<th>Default</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>config_name</code></td>
<td>—</td>
<td>string</td>
<td><code>bike_config</code></td>
</tr>
<tr>
<td><code>hw_name</code></td>
<td>—</td>
<td>label</td>
<td>"WFB Cfg" — <em>"This is the VESC Logging and Communication Module."</em></td>
</tr>
<tr>
<td><code>package_disable</code></td>
<td><code>PACKAGE_DISABLE</code></td>
<td>bool</td>
<td>1</td>
</tr>
</tbody>
</table></div>
<p><strong>Wheelie PID</strong> — the tune the disabled sub-command 101 would have driven</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Name</th>
<th>cDefine</th>
<th>Scale</th>
<th>Range</th>
<th>Default</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>kp</code> (Angle P)</td>
<td><code>WHEELIE_KP</code></td>
<td>10</td>
<td>0–40</td>
<td>30</td>
</tr>
<tr>
<td><code>kp2</code> (Rate P)</td>
<td><code>WHEELIE_KP2</code></td>
<td>100</td>
<td>0–3</td>
<td>1</td>
</tr>
<tr>
<td><code>mahony_kp</code></td>
<td><code>WHEELIE_MAHONY_KP</code></td>
<td>100</td>
<td>0–3</td>
<td>2</td>
</tr>
<tr>
<td><code>kp_brake</code></td>
<td><code>WHEELIE_KP_BRAKE</code></td>
<td>10</td>
<td>0.2–2</td>
<td>1.5</td>
</tr>
<tr>
<td><code>kp2_brake</code></td>
<td><code>WHEELIE_KP2_BRAKE</code></td>
<td>10</td>
<td>0–2</td>
<td>1.5</td>
</tr>
<tr>
<td><code>lift_resistance_kp2</code></td>
<td><code>BIKE_LIFT_RESISTANCE_KP2</code></td>
<td>100</td>
<td>0–3</td>
<td>1.5</td>
</tr>
</tbody>
</table></div>
<p>The <code>lift_resistance_kp2</code> description is unusually candid about the design:
Rate P responds to the nose's angular velocity independent of setpoint, and
although it's nominally a P term they use it "practically as the D (Derivative)
component for our PID loop, working as a damper for the Angle P component."
Recommended 0.1–0.6, with 0.7–1.2+ possible but flagged for side effects, and
advice to scale Angle P and Rate P together to preserve ride feel.</p>
<p><strong>Motor limits and ramps</strong> — same quantities sub-command 100 pushes live</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Name</th>
<th>cDefine</th>
<th>Wire</th>
<th>Range</th>
<th>Default</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>max_current</code></td>
<td><code>BIKE_MAX_CURRENT</code></td>
<td>int16</td>
<td>0–150</td>
<td>150</td>
</tr>
<tr>
<td><code>max_current_brake</code></td>
<td><code>BIKE_MAX_CURRENT_BRAKE</code></td>
<td>int16</td>
<td>−150–0</td>
<td>−90</td>
</tr>
<tr>
<td><code>ramp_alpha_pos</code></td>
<td><code>BIKE_RAMP_ALPHA_POS</code></td>
<td>×10000</td>
<td>0.0001–1</td>
<td>0.003</td>
</tr>
<tr>
<td><code>ramp_alpha_neg</code></td>
<td><code>BIKE_RAMP_ALPHA_NEG</code></td>
<td>×10000</td>
<td>0.0001–1</td>
<td>0.008</td>
</tr>
<tr>
<td><code>ramp_alpha_return</code></td>
<td><code>BIKE_RAMP_ALPHA_RETURN</code></td>
<td>×10000</td>
<td>0.0001–1</td>
<td>0.008</td>
</tr>
<tr>
<td><code>balance_pitch</code></td>
<td><code>BIKE_BALANCE_PITCH</code></td>
<td>×100</td>
<td>5–55</td>
<td>30</td>
</tr>
</tbody>
</table></div>
<p><code>ramp_alpha_return</code> has no counterpart in <code>createRideModeCommand</code> — the live
command carries only pos and neg. The persisted config has a third ramp the
live path can't set.</p>
<p><strong>Button and tilt</strong></p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Name</th>
<th>cDefine</th>
<th>Wire</th>
<th>Range</th>
<th>Default</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>button_function</code></td>
<td><code>BIKE_BUTTON_FUNCTION</code></td>
<td>enum</td>
<td>Wheelie / Turbo / Horn / Disabled</td>
<td>2 (Horn)</td>
</tr>
<tr>
<td><code>throttle_tilt_enabled</code></td>
<td><code>BIKE_THROTTLE_TILT_ENABLED</code></td>
<td>bool</td>
<td>—</td>
<td>1</td>
</tr>
<tr>
<td><code>throttle_tilt_angle_limit</code></td>
<td><code>BIKE_THROTTLE_TILT_ANGLE_LIMIT</code></td>
<td>×100</td>
<td>−20–20</td>
<td>5</td>
</tr>
<tr>
<td><code>brake_tilt_angle_limit</code></td>
<td><code>BIKE_BRAKE_TILT_ANGLE_LIMIT</code></td>
<td>×100</td>
<td>−20–20</td>
<td>−5</td>
</tr>
<tr>
<td><code>persist_wheelie_throttle</code></td>
<td><code>BIKE_PERSIST_WHEELIE_THROTTLE</code></td>
<td>bool</td>
<td>—</td>
<td>0</td>
</tr>
<tr>
<td><code>persist_wheelie_brake</code></td>
<td><code>BIKE_PERSIST_WHEELIE_BRAKE</code></td>
<td>bool</td>
<td>—</td>
<td>0</td>
</tr>
</tbody>
</table></div>
<p><code>button_function</code> enum labels are <code>['Wheelie', 'Turbo', 'Horn', 'Disabled']</code>,
matching the <code>{wheelie:0, turbo:1, horn:2, disabled:3}</code> map in sub-command 102.
Its description: Wheelie triggers a wheelie at any time; Turbo accelerates as
fast as possible while maintaining current angle; Horn triggers motor haptics;
Disabled assigns nothing.</p>
<p><code>throttle_tilt_enabled</code> description: while the button is held in Wheelie mode,
the bike's angle can be modulated with throttle and regen brake within the
configured tilt angles.</p>
<p>Note the config default for <code>button_function</code> is <strong>2 (Horn)</strong> while
<code>DEFAULT_SELECTED_FUNCTION</code> in the app is <strong><code>'wheelie'</code> (0)</strong> — another
app/firmware default mismatch.</p>
<p><strong>Anti-loop</strong></p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Name</th>
<th>cDefine</th>
<th>Wire</th>
<th>Range</th>
<th>Default</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>anti_loop_enabled</code></td>
<td><code>BIKE_ANTI_LOOP_ENABLED</code></td>
<td>bool</td>
<td>—</td>
<td>1</td>
</tr>
<tr>
<td><code>anti_loop_use_button_angle</code></td>
<td><code>BIKE_ANTI_LOOP_USE_BUTTON_ANGLE</code></td>
<td>bool</td>
<td>—</td>
<td>1</td>
</tr>
<tr>
<td><code>anti_loop_angle</code></td>
<td><code>BIKE_ANTI_LOOP_ANGLE</code></td>
<td>×100</td>
<td>5–55</td>
<td>30</td>
</tr>
</tbody>
</table></div>
<p>When <code>anti_loop_use_button_angle</code> is set, anti-loop references the wheelie
button's configured angle instead of <code>anti_loop_angle</code>.</p>
<p><strong>Horn and limit</strong></p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Name</th>
<th>cDefine</th>
<th>Wire</th>
<th>Range</th>
<th>Default</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>horn_strength</code></td>
<td><code>BIKE_HORN_STRENGTH</code></td>
<td>×100</td>
<td>0–100</td>
<td>4</td>
</tr>
<tr>
<td><code>horn_frequency</code></td>
<td><code>BIKE_HORN_FREQUENCY</code></td>
<td>uint16</td>
<td>200–1000</td>
<td>494</td>
</tr>
<tr>
<td><code>speed_limit</code></td>
<td><code>BIKE_SPEED_LIMIT</code></td>
<td>uint8</td>
<td>0–35</td>
<td>19</td>
</tr>
</tbody>
</table></div>
<p><code>horn_frequency</code>'s 200–1000 Hz range brackets the note table's 208–988 Hz
exactly.</p>
<hr>
<h2 id="7-telemetry-for-the-bike-screen">7. Telemetry for the bike screen<a class="heading-anchor" href="#7-telemetry-for-the-bike-screen" aria-label="Link to 7. Telemetry for the bike screen" title="Link to 7. Telemetry for the bike screen"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>Two loops, both driven off <code>loopTime</code> reported by <code>COMM_GET_VALUES_SETUP</code>:</p>
<pre><code>bikeRTDataInterval   = loopTime
bikeSlowDataInterval = max(loopTime, 1000)
</code></pre>
<p><code>getRTFloatData(ctx, mode = 1)</code> sends:</p>
<pre><code>36  102  10  &lt;mode&gt;
</code></pre>
<p>i.e. <code>GET_ALL_DATA</code> with a mode selector. All call sites in this build pass
only <code>ctx</code>, so <strong>mode is always 1</strong>. The response echoes the mode back as its
first byte, and the parser adds <code>tachAbs</code>, <code>tempMosfet</code>, <code>tempMotor</code>,
<code>tempBattery</code> only when <code>mode &gt;= 2</code> — meaning the temperature fields exist in
the protocol but this build never requests them through this path. It gets
temperatures from plain <code>COMM_GET_VALUES</code> instead.</p>
<p><code>getAllFloatData</code> additionally issues a plain <code>COMM_GET_VALUES_SETUP</code> and
derives <code>batteryPercent</code> (rounded to 2 dp), <code>batteryVolts</code> (1 dp), and
<code>wattHoursRemaining</code> for the dashboard.</p>
<p>Field layout of the <code>GET_ALL_DATA</code> response is in the main document, §6.</p>
<hr>
<h2 id="8-bike-layer-issues">8. Bike-layer issues<a class="heading-anchor" href="#8-bike-layer-issues" aria-label="Link to 8. Bike-layer issues" title="Link to 8. Bike-layer issues"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>#</th>
<th>Issue</th>
</tr>
</thead>
<tbody>
<tr>
<td>1</td>
<td><code>BIKE_WHEELIE_CONFIGURE</code> (101) defined but never sent; 2 of 3 wheelie modes <code>disabled: true</code></td>
</tr>
<tr>
<td>2</td>
<td>Ramp slider position 10 short-circuits to <code>alpha = 1</code> (byte 248); <code>RAMP_VALUES[9] = 0.1</code> is unreachable, giving a 0.06 → 1.0 jump</td>
</tr>
<tr>
<td>3</td>
<td><code>hornStrengthUiToWireByte</code> multiplies by 10, <code>hornStrengthFirmwareToUi</code> doesn't divide — asymmetric round-trip</td>
</tr>
<tr>
<td>4</td>
<td><code>coastingBrakeStrength</code> tracked in UI state but absent from the 13-byte <code>BIKE_FEATURE_CONFIGURE</code> payload</td>
</tr>
<tr>
<td>5</td>
<td><code>speed_limit</code> firmware default 19 vs app <code>DEFAULT_SPEED_LIMIT</code> 20</td>
</tr>
<tr>
<td>6</td>
<td><code>button_function</code> firmware default 2 (Horn) vs app <code>DEFAULT_SELECTED_FUNCTION</code> <code>'wheelie'</code> (0)</td>
</tr>
<tr>
<td>7</td>
<td><code>speedLimitMphToWireByte</code> guards on <code>n &lt;= 0</code>, not <code>n &lt; SPEED_LIMIT_MIN</code>; 1–4 mph reach the wire</td>
</tr>
<tr>
<td>8</td>
<td><code>ramp_alpha_return</code> exists in the persisted config but has no live-command equivalent</td>
</tr>
<tr>
<td>9</td>
<td>Ride-mode current resolution is 10 A per nibble — the UI can present finer values than the wire can carry</td>
</tr>
</tbody>
</table></div>
`,headings:[{id:`1-frame-shape`,title:`1. Frame shape`,level:2},{id:`2-sub-command-100-bike_ride_configure`,title:`2. Sub-command 100 — BIKE_RIDE_CONFIGURE`,level:2},{id:`rampalphatobytea-piecewise-non-linear-quantisation`,title:`rampAlphaToByte(a) — piecewise non-linear quantisation`,level:3},{id:`ride-mode-presets`,title:`Ride mode presets`,level:3},{id:`custom-mode-slider-parameter-mapping`,title:`Custom mode: slider → parameter mapping`,level:3},{id:`reverse-mapping-determineinitialridemode`,title:`Reverse mapping — determineInitialRideMode`,level:3},{id:`3-sub-command-101-bike_wheelie_configure`,title:`3. Sub-command 101 — BIKE_WHEELIE_CONFIGURE`,level:2},{id:`4-sub-command-102-bike_feature_configure`,title:`4. Sub-command 102 — BIKE_FEATURE_CONFIGURE`,level:2},{id:`horn-note-table`,title:`Horn note table`,level:3},{id:`not-transmitted`,title:`Not transmitted`,level:3},{id:`defaults`,title:`Defaults`,level:3},{id:`5-sub-command-103-bike_speed_limit_configure`,title:`5. Sub-command 103 — BIKE_SPEED_LIMIT_CONFIGURE`,level:2},{id:`6-bike_config-the-full-custom-config-profile`,title:`6. bike_config — the full custom-config profile`,level:2},{id:`write-payload-getfloatconfigpayloadversion-config`,title:`Write payload — getFloatConfigPayload(version, config)`,level:3},{id:`read-parse-getfloatconfbuffer`,title:`Read parse — getFloatConf(buffer)`,level:3},{id:`wire-types`,title:`Wire types`,level:3},{id:`the-27-parameters`,title:`The 27 parameters`,level:3},{id:`7-telemetry-for-the-bike-screen`,title:`7. Telemetry for the bike screen`,level:2},{id:`8-bike-layer-issues`,title:`8. Bike-layer issues`,level:2}],search:`Companion to wheelie-vesc-comms-api.md. Everything here is the vendor-specific layer that sits on top of COMM_CUSTOM_APP_DATA. 1. Frame shape Every bike command is one VESC packet: 0x02 len 36 102 <sub> <payload…> crc16 0x03 │ │ │ │ │ │ │ └── FloatPacketTypes sub-command │ │ └──────── AppPacketTypes.WFB = 102 │ └──────────── COMM_CUSTOM_APP_DATA = 36 └───────────────── length byte getVescAppData reads bytes 0 and 1 of the payload and returns null unless byte 0 is 102. Stock Float package boards advertise 101, so this app is a closed loop with its own firmware package — it won't read a Float or Refloat board even though it understands the config format. Two send helpers exist: writeData(...bytes) — fire and forget, used by the three configure commands. getData(...bytes) — writes and awaits a matching response, used for polling. Both prepend [COMM_FORWARD_CAN, canId] automatically when _primaryCANDeviceId is set. 2. Sub-command 100 — BIKE_RIDE_CONFIGURE Sets the four motor-response parameters as a 3-byte packed payload. Full frame is 6 bytes of payload: 36 102 100 b0 b1 b2 Built by createRideModeCommand(maxCurrent, maxCurrentBrake, rampAlphaPos, rampAlphaNeg): Byte Content b0 low nibble clamp(round(maxCurrent / 10), 0, 15) b0 high nibble clamp(round(abs(maxCurrentBrake) / 10), 0, 15) b1 rampAlphaToByte(rampAlphaPos) b2 rampAlphaToByte(rampAlphaNeg) So currents are transmitted in 10 A units, 4 bits each, brake current as an absolute value in the high nibble. Range on the wire is 0–150 A in 10 A steps — which is exactly why max_current in the config table tops out at 150. rampAlphaToByte(a) — piecewise non-linear quantisation A single byte covering four orders of magnitude, with finer resolution at the low (smoother) end: Input range Byte value Resolution a <= 0.0001 0 — 0.0001 < a <= 0.001 round((a − 0.0001) / 0.0001) → 0–9 0.0001 0.001 < a <= 0.01 round(10 + (a − 0.001) / 0.0005) → 10–28 0.0005 0.01 < a <= 0.1 round(28 + (a − 0.01) / 0.001) → 28–118 0.001 0.1 < a <= 0.5 round(118 + (a − 0.1) / 0.005) → 118–198 0.005 0.5 < a < 1 round(198 + (a − 0.5) / 0.01) → 198–248 0.01 a >= 1 248 — The bands join cleanly (0.001→10, 0.01→28, 0.1→118, 0.5→198, 1.0→248), so this is a deliberate companding curve, not accumulated drift. Byte values 249–255 are unreachable. Ride mode presets RIDE_MODE_PARAMS: id maxCurrent maxCurrentBrake rampAlphaPos rampAlphaNeg sport 150 −90 0.015 0.008 cruise 150 −90 0.003 0.008 eco 90 −90 0.002 0.005 custom 150 −90 0.003 0.008 sport and cruise differ only in throttle ramp; braking is identical across all four presets. custom is seeded from cruise. UI metadata: Sport #AC0E0E "High performance mode with near-instant throttle response", Cruise #0E77AC "Smooth, predictable power delivery suitable for most riders", Eco #198E13 "Limited acceleration and speed for a more relaxed and efficient ride", Custom #777777, plus an unknown pseudo-mode ("Configuration doesn't match any known mode"). Custom mode: slider → parameter mapping When the user picks Custom, the four values come from sliders: maxCurrent = 60 + throttleCurrent * 10 maxCurrentBrake = −(60 + brakeCurrent * 10) rampAlphaPos = throttleRamp === 10 ? 1 : RAMP_VALUES[throttleRamp − 1] rampAlphaNeg = brakeRamp === 10 ? 1 : RAMP_VALUES[brakeRamp − 1] RAMP_VALUES = [0.0005, 0.001, 0.002, 0.003, 0.005, 0.008, 0.015, 0.03, 0.06, 0.1] Slider position 10 short-circuits to rampAlpha = 1 (instant response, byte 248) rather than reading RAMP_VALUES[9] = 0.1. Index 9 of the table is therefore dead — position 9 gives 0.06 and position 10 jumps straight to 1.0. Whether that's intentional ("10 = instant") or an off-by-one is unclear from the bytecode; the effect is a large discontinuity at the top of the slider. Reverse mapping — determineInitialRideMode On connect, the app reads the live config and tries to match it back to a preset using isCloseEnough(a, b, epsilon = 0.001) on each of the four values. No match → the unknown pseudo-mode. A match on custom's values → 'custom'. This is how the UI shows which mode a board is currently in without the firmware reporting a mode ID. Logging is verbose here: "Sending ride mode command for <name>:" with the resolved values and commandBytes, then "Successfully sent ride mode configuration for <name>". "Cannot select unknown mode" throws if the unknown pseudo-mode is somehow selected. 3. Sub-command 101 — BIKE_WHEELIE_CONFIGURE Defined but not transmitted by app version 1.0.0. A supported payload encoding for this sub-command is not established here. The associated UI exists but is largely disabled. WHEELIE_MODES: id name icon colour disabled agro Agro 3 #AC0E0E true standard Standard 2 #0E77AC false float Float 1 #198E13 true Descriptions: Agro — "Stiff, locked in response; sticks to set wheelie angle at all costs"; Standard — "Secure feeling wheelie with some give for a smooth, fun ride"; Float — "Soft, loose wheelie for a more floaty feel". The mode picker includes selection and cancellation state, but two options are disabled and no transmit path is documented. These controls do not establish a supported sub-command 101 payload. Separate WHEELIE_CONFIG_SIGNATURES and isWheelieConfig identifiers suggest another config profile may have been planned; that remains unconfirmed. Wheelie tuning in this build goes through the ordinary COMM_SET_CUSTOM_CONFIG path instead (the WHEELIE_KP* / MAHONY_KP params in §5), not through sub-command 101. 4. Sub-command 102 — BIKE_FEATURE_CONFIGURE The big one: 13 payload bytes, all scalar, no length prefix. Full frame payload is 16 bytes. 36 102 102 b0 b1 b2 b3 b4 b5 b6 b7 b8 b9 b10 b11 b12 # Field Encoding Wire range 0 selectedFunction {wheelie:0, turbo:1, horn:2, disabled:3}, unknown → 0, then min(x, 3) 0–3 1 wheelieAngle clamp(round(deg × 2), 10, 110) 10–110 (5–55°) 2 throttleBrakeTilt bool ? 1 : 0 0/1 3 throttleTiltAngle clamp(round((deg + 20) × 2), 0, 80) 0–80 (−20…+20°) 4 brakeTiltAngle clamp(round((deg + 20) × 2), 0, 80) 0–80 (−20…+20°) 5 persistWheelieThrottle bool ? 1 : 0 0/1 6 persistWheelieBrake bool ? 1 : 0 0/1 7 antiLoopEnabled bool ? 1 : 0 0/1 8 sameAngleAsButton bool ? 1 : 0 0/1 9 antiLoopAngle clamp(round(deg × 2), 10, 110) 10–110 (5–55°) 10 wheelLiftResistance clamp(round(x × 10), 0, 30) 0–30 (0–3.0) 11 hornStrength hornStrengthUiToWireByte(ui), clamped 0–250 10–100 12 hornNoteIndex clamp(round(i), 0, 27) 0–27 Two encoding families worth noting: angles are ×2 with a +20 bias where they can go negative (tilt angles), and plain ×2 where they can't (wheelie and anti-loop angles, both 5–55°). The +20/×2 bias is what lets a signed −20…+20° range fit an unsigned byte. hornStrengthUiToWireByte(ui) = max(10, min(100, round(ui × 10))) — UI 1–10 maps to wire 10–100. The inverse used when reading firmware state is hornStrengthFirmwareToUi(fw) = max(1, min(10, round(fw))), which does not divide by 10. The two paths disagree by a factor of 10; the horn_strength config field (§5) is a 0–100 double with default 4, and the UI default is als`};export{e as default};