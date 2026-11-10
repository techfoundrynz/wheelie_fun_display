var e={source:`docs/wheelie-ride-controls.md`,route:`reference/wfb-ride-controls`,title:`WFB ride controls`,group:`Reference`,description:`Control types, rider-facing ranges, presets and configuration limits.`,body:`<p>Reference for every user-facing control that changes how the bike rides: where
it lives, what kind of control it is, and its exact label, range, step and
default.</p>
<h2 id="use-in-wheelie-fun-display">Use in Wheelie Fun Display</h2>
<p>This supplied reference informs the WFB tuning interface. The display uses
Response, Button, Throttle and Horn groups, with discrete sliders for
rider adjustments, a button-action picker and toggles. The Advanced section,
including return response and PID gains, is hidden to keep the available
controls aligned with the app. The runtime bike schema can narrow every documented range;
readback values outside a slider's stops are preserved until explicitly edited.
The amber marker identifies the reference default, rather than the bike's saved
value. A separate readback label shows what is actually stored on the bike.</p>
<p>The display deliberately uses one consistent, explicit confirmation and readback
flow for all writes. Slider dragging never sends changes directly. Angle sliders
and their buttons both cover the full documented range, rather than using
different drag and button ranges. Linked
anti-loop angles remain visible and disabled. Coasting brake and the inactive
wheelie-mode choices are omitted because this reference describes no effective
transmitted change for them.</p>
<p>The app and stored defaults differ; defaults in this reference must never
replace a bike's reported configuration on connection. See the
<a href="../../guide/#tune">display user guide</a> for current interaction behaviour.</p>
<hr>
<h2 id="1-where-the-controls-live">1. Where the controls live</h2>
<p>Five editors, each a separate modal with its own save behaviour.</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Editor</th>
<th>Opened from</th>
<th>Control pattern</th>
<th>Command</th>
</tr>
</thead>
<tbody>
<tr>
<td><strong>Ride Mode</strong></td>
<td>Dashboard mode button</td>
<td>4 preset tiles, applied on tap</td>
<td><code>BIKE_RIDE_CONFIGURE</code> (100)</td>
</tr>
<tr>
<td><strong>Custom Tuning</strong></td>
<td>"Custom" preset inside Ride Mode</td>
<td>4 discrete sliders + preset shortcuts</td>
<td><code>BIKE_RIDE_CONFIGURE</code> (100)</td>
</tr>
<tr>
<td><strong>Features</strong></td>
<td>Dashboard features card</td>
<td>3 tabs: sliders, toggles, checkboxes, picker</td>
<td><code>BIKE_FEATURE_CONFIGURE</code> (102)</td>
</tr>
<tr>
<td><strong>Speed Limit</strong></td>
<td>Dashboard speed-limit sign</td>
<td>Single slider with a "No Limit" stop</td>
<td><code>BIKE_SPEED_LIMIT_CONFIGURE</code> (103)</td>
</tr>
<tr>
<td><strong>Wheelie Mode</strong></td>
<td>Dashboard wheelie button</td>
<td>3 tiles (see §6)</td>
<td>—</td>
</tr>
</tbody>
</table></div>
<p>All five are translucent full-screen modals with a tap-to-dismiss scrim, a
close button, and hardware-back wired to the same close guard.</p>
<hr>
<h2 id="2-ride-mode">2. Ride Mode</h2>
<p><strong>Title:</strong> "Select a Ride Mode"</p>
<p>Four tiles. Selecting one applies immediately — no save step. The modal shows
a loading state while the write is in flight.</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>id</th>
<th>Label</th>
<th>Icon</th>
<th>Colour</th>
<th>Description</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>sport</code></td>
<td>Sport</td>
<td><code>S</code></td>
<td><code>#AC0E0E</code></td>
<td>High performance mode with near-instant throttle response</td>
</tr>
<tr>
<td><code>cruise</code></td>
<td>Cruise</td>
<td><code>C</code></td>
<td><code>#0E77AC</code></td>
<td>Smooth, predictable power delivery suitable for most riders</td>
</tr>
<tr>
<td><code>eco</code></td>
<td>Eco</td>
<td><code>E</code></td>
<td><code>#198E13</code></td>
<td>Limited acceleration and speed for a more relaxed and efficient ride</td>
</tr>
<tr>
<td><code>custom</code></td>
<td>Custom</td>
<td>—</td>
<td><code>#777777</code></td>
<td>Opens the Custom Tuning editor</td>
</tr>
</tbody>
</table></div>
<p>Values behind each preset:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>id</th>
<th>Max current</th>
<th>Max brake current</th>
<th>Throttle ramp α</th>
<th>Brake ramp α</th>
</tr>
</thead>
<tbody>
<tr>
<td>sport</td>
<td>150 A</td>
<td>−90 A</td>
<td>0.015</td>
<td>0.008</td>
</tr>
<tr>
<td>cruise</td>
<td>150 A</td>
<td>−90 A</td>
<td>0.003</td>
<td>0.008</td>
</tr>
<tr>
<td>eco</td>
<td>90 A</td>
<td>−90 A</td>
<td>0.002</td>
<td>0.005</td>
</tr>
<tr>
<td>custom</td>
<td>150 A</td>
<td>−90 A</td>
<td>0.003</td>
<td>0.008</td>
</tr>
</tbody>
</table></div>
<p>Sport and Cruise differ only in throttle ramp and have identical braking.
Eco uses the same brake-current limit but a different brake ramp. Custom is
seeded from Cruise.</p>
<h3 id="mode-detection-on-connect">Mode detection on connect</h3>
<p>The app reads the live configuration and matches it against each preset,
comparing all four values with a tolerance of 0.001. If nothing matches it
displays a fifth, non-selectable state:</p>
<blockquote>
<p><strong>Unknown</strong> — "Configuration doesn't match any known mode"</p>
</blockquote>
<p>This is how the dashboard shows the current mode without the firmware
reporting a mode ID.</p>
<hr>
<h2 id="3-custom-tuning">3. Custom Tuning</h2>
<p><strong>Title:</strong> "Custom Tuning"</p>
<p>A preset shortcut row, then four discrete sliders in two titled groups.</p>
<h3 id="preset-shortcuts">Preset shortcuts</h3>
<p>Label "Preset" followed by three 48 px circular buttons (<code>S</code>, <code>C</code>, <code>E</code>).
Tapping one loads that preset's values into the sliders, after a confirmation:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Situation</th>
<th>Title</th>
<th>Body</th>
<th>Buttons</th>
</tr>
</thead>
<tbody>
<tr>
<td>Switching to a different preset with unsaved edits</td>
<td><strong>Overwrite Custom Values?</strong></td>
<td>Changing presets will overwrite your current custom values. Are you sure you want to continue?</td>
<td>Cancel · <strong>Overwrite</strong></td>
</tr>
<tr>
<td>Re-tapping the preset already selected</td>
<td><strong>Reset to Preset Values?</strong></td>
<td>This will reset your custom values to the preset defaults. Are you sure you want to continue?</td>
<td>Cancel · <strong>Reset</strong></td>
</tr>
</tbody>
</table></div>
<h3 id="sliders">Sliders</h3>
<p>All four use the same control: a 10-position discrete track with tick marks, a
value pill in the header, and a small circular reset button beside the label
that restores that one slider to its preset value.</p>
<p><strong>Throttle</strong> group:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Label</th>
<th>End labels</th>
<th>Positions</th>
<th>Step</th>
<th>Effect</th>
</tr>
</thead>
<tbody>
<tr>
<td>Strength</td>
<td>WEAK / STRONG</td>
<td>0–9</td>
<td>1</td>
<td>Max current <code>60 + v × 10</code> → <strong>60–150 A</strong></td>
</tr>
<tr>
<td>Responsiveness</td>
<td>SLOW / QUICK</td>
<td>1–10</td>
<td>1</td>
<td>Throttle ramp α (see table below)</td>
</tr>
</tbody>
</table></div>
<p><strong>Brakes</strong> group: same two controls, same labels and ranges, driving max brake
current and brake ramp α.</p>
<p>Responsiveness positions map to ramp α:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Position</th>
<th>1</th>
<th>2</th>
<th>3</th>
<th>4</th>
<th>5</th>
<th>6</th>
<th>7</th>
<th>8</th>
<th>9</th>
<th>10</th>
</tr>
</thead>
<tbody>
<tr>
<td>α</td>
<td>0.0005</td>
<td>0.001</td>
<td>0.002</td>
<td>0.003</td>
<td>0.005</td>
<td>0.008</td>
<td>0.015</td>
<td>0.03</td>
<td>0.06</td>
<td><strong>1.0</strong></td>
</tr>
</tbody>
</table></div>
<p>Position 10 is a dedicated "instant" setting rather than a continuation of the
curve, so there is a deliberate jump between positions 9 and 10.</p>
<p>Current resolution on the wire is 10 A per position, which is why the strength
slider has exactly ten stops across 60–150 A.</p>
<h3 id="saving">Saving</h3>
<p>A save bar at the bottom commits all four values. Success is confirmed in the
UI; failure shows an alert: "Failed to save custom tuning settings."</p>
<p>Closing with unsaved edits raises a three-option prompt:</p>
<blockquote>
<p><strong>Unsaved Changes</strong> — You have unsaved changes. What would you like to do?
Cancel · <strong>Discard Changes</strong> · Save</p>
</blockquote>
<hr>
<h2 id="4-features">4. Features</h2>
<p>The largest editor: a modal with three tabs. Each tab has its own
<strong>"Reset to Defaults"</strong> button affecting only that tab's fields. A single save
commits everything regardless of which tab is open.</p>
<p>Tabs: <strong>Button</strong> · <strong>Throttle</strong> · <strong>Brakes</strong></p>
<h3 id="how-the-sliders-work">How the sliders work</h3>
<p>Angle and value sliders here share one behaviour worth knowing: <strong>the drag
range and the button range differ.</strong> Dragging the track covers a narrow band
around the centre; the <code>−</code> and <code>+</code> buttons either side reach considerably
further and support press-and-hold repeat. A vertical line marks the default
position, the current value renders large above the track, and when a value has
been changed the previous value appears beneath it in amber.</p>
<h3 id="button-tab">Button tab</h3>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Control</th>
<th>Type</th>
<th>Label</th>
<th>Options / range</th>
<th>Default</th>
</tr>
</thead>
<tbody>
<tr>
<td>Button function</td>
<td>Picker</td>
<td>"Select Button Function"</td>
<td>Wheelie · Turbo · Horn · Disabled</td>
<td>Wheelie</td>
</tr>
<tr>
<td>Wheelie angle</td>
<td>Angle slider</td>
<td><code>Wheelie Angle:</code></td>
<td>centre 30°; drag ±10°, buttons ±25° → <strong>5–55°</strong></td>
<td>30°</td>
</tr>
<tr>
<td>Maintain wheelie when held</td>
<td>Two checkboxes</td>
<td><code>Maintain Wheelie When Held:</code></td>
<td><code>Throttle</code>, <code>Brake</code></td>
<td>both off</td>
</tr>
</tbody>
</table></div>
<p>Button function values: Wheelie triggers a wheelie at any time; Turbo
accelerates as fast as possible while maintaining the current angle; Horn
triggers a horn sound through motor haptics; Disabled assigns no action.</p>
<h3 id="throttle-tab">Throttle tab</h3>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Control</th>
<th>Type</th>
<th>Label</th>
<th>Range</th>
<th>Default</th>
</tr>
</thead>
<tbody>
<tr>
<td>Tilt enable</td>
<td>Toggle</td>
<td><code>Throttle / Brake Tilt</code></td>
<td>on / off</td>
<td>on</td>
</tr>
<tr>
<td>Throttle tilt</td>
<td>Angle slider</td>
<td><code>Throttle Tilt:</code></td>
<td>drag ±10, buttons <strong>−20 … +20</strong></td>
<td>+5</td>
</tr>
<tr>
<td>Brake tilt</td>
<td>Angle slider</td>
<td><code>Brake Tilt:</code></td>
<td>drag ±10, buttons <strong>−20 … +20</strong></td>
<td>−5</td>
</tr>
<tr>
<td>Anti-loop enable</td>
<td>Toggle</td>
<td><code>Anti-Loop Throttle</code></td>
<td>on / off</td>
<td>on</td>
</tr>
<tr>
<td>Anti-loop angle</td>
<td>Angle slider</td>
<td><code>Anti-Loop Angle:</code></td>
<td>centre 30°; drag ±10°, buttons ±25° → <strong>5–55°</strong></td>
<td>30°</td>
</tr>
<tr>
<td>Same angle as button</td>
<td>Checkbox, inside the anti-loop control</td>
<td><code>Same Angle as Button</code></td>
<td>on / off</td>
<td>on</td>
</tr>
<tr>
<td>Wheel lift resistance</td>
<td>Value slider</td>
<td><code>Wheel Lift Resistance:</code></td>
<td><strong>0 – 3.0</strong>, step <strong>0.3</strong>, shown as <strong>0–10</strong></td>
<td>1.5 (displays as 5)</td>
</tr>
</tbody>
</table></div>
<p>Both tilt sliders show a signed value (<code>+5</code>, <code>−5</code>) and a default-position
marker. With tilt enabled, the bike's angle in Wheelie mode can be modulated
with throttle and regen brake while the button is held, within the configured
tilt angles.</p>
<p>"Same Angle as Button" makes anti-loop reference the wheelie button's
configured angle instead of its own; the anti-loop slider stays visible but is
driven by that value.</p>
<p>Wheel lift resistance is stored 0–3.0 and displayed on a 0–10 scale
(×3.3333), with tick marks every 0.3.</p>
<h3 id="brakes-tab">Brakes tab</h3>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Control</th>
<th>Type</th>
<th>Label</th>
<th>Range</th>
<th>Default</th>
</tr>
</thead>
<tbody>
<tr>
<td>Coasting brake strength</td>
<td>Value slider</td>
<td><code>Coasting Brake Strength:</code></td>
<td><strong>0–10</strong>, step 1, displayed as <strong>0–100 %</strong></td>
<td>0</td>
</tr>
</tbody>
</table></div>
<p>The remainder of the tab is a dimmed <strong>"Coming Soon"</strong> panel. Coasting brake
strength is stored with the local configuration; it is not part of the
transmitted feature payload in this version.</p>
<h3 id="horn-settings">Horn settings</h3>
<p>A <code>Horn Settings</code> section, relevant when the button function is set to Horn.
The horn is produced through motor haptics rather than a speaker.</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Control</th>
<th>Label</th>
<th>Range</th>
<th>Step</th>
<th>Default</th>
</tr>
</thead>
<tbody>
<tr>
<td>Strength</td>
<td><code>Horn Strength:</code></td>
<td><strong>1–10</strong></td>
<td>1</td>
<td>4</td>
</tr>
<tr>
<td>Frequency</td>
<td><code>Horn Frequency:</code></td>
<td><strong>0–27</strong> (note index)</td>
<td>1</td>
<td>15</td>
</tr>
</tbody>
</table></div>
<p>Frequency displays musical note names, not numbers, with tick marks every
2 positions. The 28-entry table spans G#3 to B5:</p>
<pre><code>0  G#3 208   7  D#4 311   14 A#4 466   21 F5  698
1  A3  220   8  E4  330   15 B4  494   22 F#5 740
2  A#3 233   9  F4  349   16 C5  523   23 G5  784
3  B3  247   10 F#4 370   17 C#5 554   24 G#5 831
4  C4  262   11 G4  392   18 D5  587   25 A5  880
5  C#4 277   12 G#4 415   19 D#5 622   26 A#5 932
6  D4  294   13 A4  440   20 E5  659   27 B5  988
</code></pre>
<p>Default index 15 is B4 = 494 Hz. The firmware accepts 200–1000 Hz, which
brackets the table exactly.</p>
<h3 id="saving_1">Saving</h3>
<p>Success shows a <strong>"Features Saved"</strong> confirmation for 2 seconds. Failure shows
an alert: "Failed to save feature settings."</p>
<p>Closing with unsaved edits:</p>
<blockquote>
<p><strong>Unsaved Changes</strong> — You have unsaved changes. Are you sure you want to
discard them?
Cancel · <strong>Discard Changes</strong></p>
</blockquote>
<hr>
<h2 id="5-speed-limit">5. Speed Limit</h2>
<p>A single slider with a stop past the top end.</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Property</th>
<th>Value</th>
</tr>
</thead>
<tbody>
<tr>
<td>Minimum</td>
<td>5 mph</td>
</tr>
<tr>
<td>Maximum</td>
<td>35 mph</td>
</tr>
<tr>
<td>Step</td>
<td>1 mph</td>
</tr>
<tr>
<td>Default</td>
<td>20 mph</td>
</tr>
<tr>
<td>Extra position</td>
<td><strong>No Limit</strong></td>
</tr>
</tbody>
</table></div>
<p>The slider has 32 positions: the first 31 cover 5–35 mph in 1 mph increments,
and the last is <strong>"No Limit"</strong>, which clears the limit on the bike. Labels
read <code>&lt;n&gt; mph</code> or <code>No Limit</code>.</p>
<p>The dashboard displays the active value in a highway-style <code>SPEED LIMIT</code> sign,
which switches to a no-limit graphic when the limit is cleared.</p>
<p>Saving is debounced by 300 ms. Success shows a <strong>"Speed Limit Saved"</strong>
confirmation for 2 seconds. Failure shows <strong>"Save Failed" — "Failed to save
speed limit settings."</strong> with an <code>Ok</code> button. Closing with unsaved edits raises
the same two-option discard prompt as the Features editor.</p>
<hr>
<h2 id="6-wheelie-mode">6. Wheelie Mode</h2>
<p><strong>Title:</strong> "Select a Wheelie Mode"</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>id</th>
<th>Label</th>
<th>Icon</th>
<th>Colour</th>
<th>Availability</th>
<th>Description</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>agro</code></td>
<td>Agro</td>
<td><code>3</code></td>
<td><code>#AC0E0E</code></td>
<td>Disabled</td>
<td>Stiff, locked in response; sticks to set wheelie angle at all costs</td>
</tr>
<tr>
<td><code>standard</code></td>
<td>Standard</td>
<td><code>2</code></td>
<td><code>#0E77AC</code></td>
<td>Available</td>
<td>Secure feeling wheelie with some give for a smooth, fun ride</td>
</tr>
<tr>
<td><code>float</code></td>
<td>Float</td>
<td><code>1</code></td>
<td><code>#198E13</code></td>
<td>Disabled</td>
<td>Soft, loose wheelie for a more floaty feel</td>
</tr>
</tbody>
</table></div>
<p>Two of the three options are disabled in this version, and selecting a wheelie
mode does not currently change any tuning on the bike.</p>
<p>The underlying wheelie PID parameters — angle P, rate P, Mahony P, brake angle
P, brake rate P and lift-resistance rate P — are part of the bike's stored
configuration but have no control surface in the app.</p>
<hr>
<h2 id="7-stored-configuration-parameters">7. Stored configuration parameters</h2>
<p>The parameters below live in the bike's persisted configuration. Where a
control exists, it is named; the rest are configuration-only in this version.</p>
<h3 id="wheelie-pid">Wheelie PID</h3>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Parameter</th>
<th>Range</th>
<th>Default</th>
<th>Control</th>
</tr>
</thead>
<tbody>
<tr>
<td>Angle P</td>
<td>0 – 40</td>
<td>30</td>
<td>—</td>
</tr>
<tr>
<td>Rate P</td>
<td>0 – 3</td>
<td>1</td>
<td>—</td>
</tr>
<tr>
<td>Mahony P</td>
<td>0 – 3</td>
<td>2</td>
<td>—</td>
</tr>
<tr>
<td>Brake angle P</td>
<td>0.2 – 2</td>
<td>1.5</td>
<td>—</td>
</tr>
<tr>
<td>Brake rate P</td>
<td>0 – 2</td>
<td>1.5</td>
<td>—</td>
</tr>
<tr>
<td>Lift resistance rate P</td>
<td>0 – 3</td>
<td>1.5</td>
<td>Wheel Lift Resistance (Throttle tab)</td>
</tr>
</tbody>
</table></div>
<p>Rate P responds to the nose's angular velocity independently of the setpoint
and acts as the damping term against Angle P. Recommended working range is
0.1–0.6; higher values up to 1.2 are usable but bring side effects. Angle P and
Rate P are best scaled together to preserve ride feel.</p>
<h3 id="motor-limits-and-ramps">Motor limits and ramps</h3>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Parameter</th>
<th>Range</th>
<th>Default</th>
<th>Control</th>
</tr>
</thead>
<tbody>
<tr>
<td>Max current</td>
<td>0 – 150 A</td>
<td>150</td>
<td>Custom Tuning → Throttle Strength</td>
</tr>
<tr>
<td>Max brake current</td>
<td>−150 – 0 A</td>
<td>−90</td>
<td>Custom Tuning → Brakes Strength</td>
</tr>
<tr>
<td>Throttle ramp α</td>
<td>0.0001 – 1</td>
<td>0.003</td>
<td>Custom Tuning → Throttle Responsiveness</td>
</tr>
<tr>
<td>Brake ramp α</td>
<td>0.0001 – 1</td>
<td>0.008</td>
<td>Custom Tuning → Brakes Responsiveness</td>
</tr>
<tr>
<td>Return ramp α</td>
<td>0.0001 – 1</td>
<td>0.008</td>
<td>—</td>
</tr>
<tr>
<td>Balance pitch</td>
<td>5 – 55</td>
<td>30</td>
<td>—</td>
</tr>
</tbody>
</table></div>
<h3 id="button-tilt-and-anti-loop">Button, tilt and anti-loop</h3>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Parameter</th>
<th>Range</th>
<th>Default</th>
<th>Control</th>
</tr>
</thead>
<tbody>
<tr>
<td>Button function</td>
<td>Wheelie / Turbo / Horn / Disabled</td>
<td>Horn</td>
<td>Button tab picker</td>
</tr>
<tr>
<td>Throttle/brake tilt enabled</td>
<td>on / off</td>
<td>on</td>
<td>Throttle tab toggle</td>
</tr>
<tr>
<td>Throttle tilt angle limit</td>
<td>−20 – 20</td>
<td>5</td>
<td><code>Throttle Tilt:</code></td>
</tr>
<tr>
<td>Brake tilt angle limit</td>
<td>−20 – 20</td>
<td>−5</td>
<td><code>Brake Tilt:</code></td>
</tr>
<tr>
<td>Persist wheelie on throttle</td>
<td>on / off</td>
<td>off</td>
<td><code>Maintain Wheelie When Held:</code> → Throttle</td>
</tr>
<tr>
<td>Persist wheelie on brake</td>
<td>on / off</td>
<td>off</td>
<td><code>Maintain Wheelie When Held:</code> → Brake</td>
</tr>
<tr>
<td>Anti-loop enabled</td>
<td>on / off</td>
<td>on</td>
<td><code>Anti-Loop Throttle</code></td>
</tr>
<tr>
<td>Use button angle for anti-loop</td>
<td>on / off</td>
<td>on</td>
<td><code>Same Angle as Button</code></td>
</tr>
<tr>
<td>Anti-loop angle</td>
<td>5 – 55</td>
<td>30</td>
<td><code>Anti-Loop Angle:</code></td>
</tr>
</tbody>
</table></div>
<h3 id="horn-and-limit">Horn and limit</h3>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Parameter</th>
<th>Range</th>
<th>Default</th>
<th>Control</th>
</tr>
</thead>
<tbody>
<tr>
<td>Horn strength</td>
<td>0 – 100</td>
<td>4</td>
<td><code>Horn Strength:</code> (1–10)</td>
</tr>
<tr>
<td>Horn frequency</td>
<td>200 – 1000 Hz</td>
<td>494</td>
<td><code>Horn Frequency:</code> (note index 0–27)</td>
</tr>
<tr>
<td>Speed limit</td>
<td>0 – 35 mph</td>
<td>19</td>
<td>Speed Limit modal (5–35 or No Limit)</td>
</tr>
</tbody>
</table></div>
<p>Note that the app's own default speed limit (20 mph) and default button
function (Wheelie) differ from the stored configuration defaults listed here
(19 mph, Horn); the stored values take effect on a bike that has never been
configured from the app.</p>
<hr>
<h2 id="8-interaction-summary">8. Interaction summary</h2>
<p><strong>Three save models:</strong></p>
<ol>
<li><strong>Immediate</strong> — Ride Mode presets apply on tap.</li>
<li><strong>Explicit</strong> — Features and Custom Tuning stage edits behind a save bar.</li>
<li><strong>Debounced</strong> — Speed Limit commits 300 ms after the last change.</li>
</ol>
<p><strong>Reset at three levels:</strong></p>
<ul>
<li>Per-slider — small circular button beside the label, restores that control's
preset or default value.</li>
<li>Per-tab — "Reset to Defaults" in the Features editor.</li>
<li>Per-editor — re-tapping the active preset in Custom Tuning.</li>
</ul>
<p><strong>Confirmation</strong> appears only where edits would be lost or overwritten: the two
preset prompts in Custom Tuning, a two-option discard in Features and Speed
Limit, and a three-option save/discard/cancel when closing Custom Tuning.</p>
<p><strong>Feedback</strong> is a 2-second success confirmation on save, and a blocking alert
on failure.</p>
<p><strong>Edit affordances</strong> across the sliders: discrete tick marks, a default-position
line, amber previous-value hints on changed rows, and ± buttons that reach
beyond the drag range.</p>
`,headings:[{id:`use-in-wheelie-fun-display`,title:`Use in Wheelie Fun Display`,level:2},{id:`1-where-the-controls-live`,title:`1. Where the controls live`,level:2},{id:`2-ride-mode`,title:`2. Ride Mode`,level:2},{id:`mode-detection-on-connect`,title:`Mode detection on connect`,level:3},{id:`3-custom-tuning`,title:`3. Custom Tuning`,level:2},{id:`preset-shortcuts`,title:`Preset shortcuts`,level:3},{id:`sliders`,title:`Sliders`,level:3},{id:`saving`,title:`Saving`,level:3},{id:`4-features`,title:`4. Features`,level:2},{id:`how-the-sliders-work`,title:`How the sliders work`,level:3},{id:`button-tab`,title:`Button tab`,level:3},{id:`throttle-tab`,title:`Throttle tab`,level:3},{id:`brakes-tab`,title:`Brakes tab`,level:3},{id:`horn-settings`,title:`Horn settings`,level:3},{id:`saving_1`,title:`Saving`,level:3},{id:`5-speed-limit`,title:`5. Speed Limit`,level:2},{id:`6-wheelie-mode`,title:`6. Wheelie Mode`,level:2},{id:`7-stored-configuration-parameters`,title:`7. Stored configuration parameters`,level:2},{id:`wheelie-pid`,title:`Wheelie PID`,level:3},{id:`motor-limits-and-ramps`,title:`Motor limits and ramps`,level:3},{id:`button-tilt-and-anti-loop`,title:`Button, tilt and anti-loop`,level:3},{id:`horn-and-limit`,title:`Horn and limit`,level:3},{id:`8-interaction-summary`,title:`8. Interaction summary`,level:2}],search:`Reference for every user-facing control that changes how the bike rides: where it lives, what kind of control it is, and its exact label, range, step and default. Use in Wheelie Fun Display This supplied reference informs the WFB tuning interface. The display uses Response, Button, Throttle and Horn groups, with discrete sliders for rider adjustments, a button-action picker and toggles. The Advanced section, including return response and PID gains, is hidden to keep the available controls aligned with the app. The runtime bike schema can narrow every documented range; readback values outside a slider's stops are preserved until explicitly edited. The amber marker identifies the reference default, rather than the bike's saved value. A separate readback label shows what is actually stored on the bike. The display deliberately uses one consistent, explicit confirmation and readback flow for all writes. Slider dragging never sends changes directly. Angle sliders and their buttons both cover the full documented range, rather than using different drag and button ranges. Linked anti-loop angles remain visible and disabled. Coasting brake and the inactive wheelie-mode choices are omitted because this reference describes no effective transmitted change for them. The app and stored defaults differ; defaults in this reference must never replace a bike's reported configuration on connection. See the display user guide for current interaction behaviour. 1. Where the controls live Five editors, each a separate modal with its own save behaviour. Editor Opened from Control pattern Command Ride Mode Dashboard mode button 4 preset tiles, applied on tap BIKE_RIDE_CONFIGURE (100) Custom Tuning "Custom" preset inside Ride Mode 4 discrete sliders + preset shortcuts BIKE_RIDE_CONFIGURE (100) Features Dashboard features card 3 tabs: sliders, toggles, checkboxes, picker BIKE_FEATURE_CONFIGURE (102) Speed Limit Dashboard speed-limit sign Single slider with a "No Limit" stop BIKE_SPEED_LIMIT_CONFIGURE (103) Wheelie Mode Dashboard wheelie button 3 tiles (see §6) — All five are translucent full-screen modals with a tap-to-dismiss scrim, a close button, and hardware-back wired to the same close guard. 2. Ride Mode Title: "Select a Ride Mode" Four tiles. Selecting one applies immediately — no save step. The modal shows a loading state while the write is in flight. id Label Icon Colour Description sport Sport S #AC0E0E High performance mode with near-instant throttle response cruise Cruise C #0E77AC Smooth, predictable power delivery suitable for most riders eco Eco E #198E13 Limited acceleration and speed for a more relaxed and efficient ride custom Custom — #777777 Opens the Custom Tuning editor Values behind each preset: id Max current Max brake current Throttle ramp α Brake ramp α sport 150 A −90 A 0.015 0.008 cruise 150 A −90 A 0.003 0.008 eco 90 A −90 A 0.002 0.005 custom 150 A −90 A 0.003 0.008 Sport and Cruise differ only in throttle ramp and have identical braking. Eco uses the same brake-current limit but a different brake ramp. Custom is seeded from Cruise. Mode detection on connect The app reads the live configuration and matches it against each preset, comparing all four values with a tolerance of 0.001. If nothing matches it displays a fifth, non-selectable state: Unknown — "Configuration doesn't match any known mode" This is how the dashboard shows the current mode without the firmware reporting a mode ID. 3. Custom Tuning Title: "Custom Tuning" A preset shortcut row, then four discrete sliders in two titled groups. Preset shortcuts Label "Preset" followed by three 48 px circular buttons (S, C, E). Tapping one loads that preset's values into the sliders, after a confirmation: Situation Title Body Buttons Switching to a different preset with unsaved edits Overwrite Custom Values? Changing presets will overwrite your current custom values. Are you sure you want to continue? Cancel · Overwrite Re-tapping the preset already selected Reset to Preset Values? This will reset your custom values to the preset defaults. Are you sure you want to continue? Cancel · Reset Sliders All four use the same control: a 10-position discrete track with tick marks, a value pill in the header, and a small circular reset button beside the label that restores that one slider to its preset value. Throttle group: Label End labels Positions Step Effect Strength WEAK / STRONG 0–9 1 Max current 60 + v × 10 → 60–150 A Responsiveness SLOW / QUICK 1–10 1 Throttle ramp α (see table below) Brakes group: same two controls, same labels and ranges, driving max brake current and brake ramp α. Responsiveness positions map to ramp α: Position 1 2 3 4 5 6 7 8 9 10 α 0.0005 0.001 0.002 0.003 0.005 0.008 0.015 0.03 0.06 1.0 Position 10 is a dedicated "instant" setting rather than a continuation of the curve, so there is a deliberate jump between positions 9 and 10. Current resolution on the wire is 10 A per position, which is why the strength slider has exactly ten stops across 60–150 A. Saving A save bar at the bottom commits all four values. Success is confirmed in the UI; failure shows an alert: "Failed to save custom tuning settings." Closing with unsaved edits raises a three-option prompt: Unsaved Changes — You have unsaved changes. What would you like to do? Cancel · Discard Changes · Save 4. Features The largest editor: a modal with three tabs. Each tab has its own "Reset to Defaults" button affecting only that tab's fields. A single save commits everything regardless of which tab is open. Tabs: Button · Throttle · Brakes How the sliders work Angle and value sliders here share one behaviour worth knowing: the drag range and the button range differ. Dragging the track covers a narrow band around the centre; the − and + buttons either side reach considerably further and support press-and-hold repeat. A vertical line marks the default position, the current value renders large above the track, and when a value has been changed the previous value appears beneath it in amber. Button tab Control Type Label Options / range Default Button function Picker "Select Button Function" Wheelie · Turbo · Horn · Disabled Wheelie Wheelie angle Angle slider Wheelie Angle: centre 30°; drag ±10°, buttons ±25° → 5–55° 30° Maintain wheelie when held Two checkboxes Maintain Wheelie When Held: Throttle, Brake both off Button function values: Wheelie triggers a wheelie at any time; Turbo accelerates as fast as possible while maintaining the current angle; Horn triggers a horn sound through motor haptics; Disabled assigns no action. Throttle tab Control Type Label Range Default Tilt enable Toggle Throttle / Brake Tilt on / off on Throttle tilt Angle slider Throttle Tilt: drag ±10, buttons −20 … +20 +5 Brake tilt Angle slider Brake Tilt: drag ±10, buttons −20 … +20 −5 Anti-loop enable Toggle Anti-Loop Throttle on / off on Anti-loop angle Angle slider Anti-Loop Angle: centre 30°; drag ±10°, buttons ±25° → 5–55° 30° Same angle as button Checkbox, insid`};export{e as default};