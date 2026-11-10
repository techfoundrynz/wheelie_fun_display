var e={source:`pcb/guition-43/README.md`,route:`hardware/guition-43/can-breakout`,title:`GUITION CAN breakout`,group:`Hardware`,description:`Editable PCB source, labelled board views, schematic and CAN wiring.`,body:`<p>This board adds a CAN transceiver and vehicle connector to the GUITION 4.3-inch display's expansion header. It is a separate PCB project from the <a href="../case-mount/">case and handlebar mount</a>.</p>
<h2 id="setup-checklist">Setup checklist</h2>
<ol>
<li>Check the connector pin numbers and mating-side orientation before making a cable.</li>
<li>Confirm 5 V power, ground, CANH and CANL against the tables below.</li>
<li>Fit the 120 Ω termination only if this board is at a bus end that needs it.</li>
<li>Use firmware configured for <strong>CAN TX GPIO51 / RX GPIO52</strong>, then select CAN in Settings.</li>
</ol>
<p>The supplied design has not been verified as a fabricated assembly. Check the native project before manufacturing or wiring.</p>
<h2 id="labelled-board-views">Labelled board views</h2>
<p><img src="../../../assets/images/guition-can-pcb-labelled.svg" alt="GUITION CAN breakout labelled top and underside isometric illustration"></p>
<p>This source-derived isometric illustration uses the actual board outline, placements, footprints, pads and track segments. Component bodies are simplified, connector heights are illustrative and copper pours are omitted. It is not a native vendor-model render or a mechanical clearance model. <strong>U1 is mounted on the underside</strong>, while the other components are on the top side.</p>
<h3 id="top-layout">Top layout</h3>
<p><img src="../../../assets/images/guition-can-pcb-top.png" alt="Top-side PCB layout with component outlines, pads and copper tracks"></p>
<h3 id="underside-layout">Underside layout</h3>
<p><img src="../../../assets/images/guition-can-pcb-bottom.png" alt="Underside PCB layout, mirrored as viewed from below, with U1 socket outline"></p>
<p>These side-specific assembly illustrations are generated from the source geometry. They show each side's tracks and component outlines, plus through-hole pads; copper pours are omitted. The underside is mirrored for physical viewing. Use the native project for manufacturing plots and connector pin numbering.</p>
<h3 id="native-combined-layer-preview">Native combined-layer preview</h3>
<p><img src="../../../assets/images/guition-can-pcb-layout.png" alt="Native PCB layout preview from the EasyEDA archive"></p>
<h3 id="schematic">Schematic</h3>
<p><img src="../../../assets/images/guition-can-pcb-schematic.png" alt="Native schematic preview from the EasyEDA archive"></p>
<h2 id="components">Components</h2>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Reference</th>
<th>Supplied part</th>
<th>Function</th>
</tr>
</thead>
<tbody>
<tr>
<td>U1</td>
<td>2 × 13 female header, 2.54 mm pitch</td>
<td>Underside display expansion socket</td>
</tr>
<tr>
<td>CN1</td>
<td>HC-PH-6AWT, six positions, 2 mm pitch</td>
<td>Vehicle wiring connector</td>
</tr>
<tr>
<td>U2</td>
<td>NXP TJA1057GT/3J, SOIC-8</td>
<td>CAN transceiver with 5 V supply and 3.3 V logic supply</td>
</tr>
<tr>
<td>R1</td>
<td>120 Ω, 0402</td>
<td>CANH-to-CANL termination</td>
</tr>
<tr>
<td>C1</td>
<td>100 nF, 0603</td>
<td>5 V supply bypass</td>
</tr>
<tr>
<td>C2</td>
<td>100 nF, 0603</td>
<td>3.3 V logic supply bypass</td>
</tr>
</tbody>
</table></div>
<p>R1 is wired directly across CANH and CANL in this design. Populate termination according to the board's position on the bus; do not add an extra terminating resistor to an already terminated intermediate node.</p>
<h2 id="vehicle-connector-cn1">Vehicle connector CN1</h2>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Pin</th>
<th>Net</th>
<th>Purpose</th>
</tr>
</thead>
<tbody>
<tr>
<td>1</td>
<td>+5V</td>
<td>5 V rail</td>
</tr>
<tr>
<td>2</td>
<td>GND</td>
<td>Common ground</td>
</tr>
<tr>
<td>3</td>
<td>CANH</td>
<td>CAN high</td>
</tr>
<tr>
<td>4</td>
<td>CANL</td>
<td>CAN low</td>
</tr>
<tr>
<td>5</td>
<td>GPIO31</td>
<td>Additional display GPIO</td>
</tr>
<tr>
<td>6</td>
<td>GPIO30</td>
<td>Additional display GPIO</td>
</tr>
</tbody>
</table></div>
<p>These are the source's pin numbers. Confirm the pin-1 marker and mating-side orientation in the editor and on the actual connector before making a cable. The footprint's pads 7 and 8 are unconnected mechanical pads, not additional signal pins.</p>
<h2 id="display-socket-u1">Display socket U1</h2>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Pin</th>
<th>Net</th>
<th>Pin</th>
<th>Net</th>
</tr>
</thead>
<tbody>
<tr>
<td>1</td>
<td>+5V</td>
<td>2</td>
<td>+3v3</td>
</tr>
<tr>
<td>3</td>
<td>+5V</td>
<td>4</td>
<td>+3v3</td>
</tr>
<tr>
<td>5</td>
<td>GND</td>
<td>6</td>
<td>GND</td>
</tr>
<tr>
<td>7</td>
<td>Unconnected</td>
<td>8</td>
<td>GPIO52 / CAN RX</td>
</tr>
<tr>
<td>9</td>
<td>GPIO31</td>
<td>10</td>
<td>GPIO51 / CAN TX</td>
</tr>
<tr>
<td>11</td>
<td>GPIO30</td>
<td>12</td>
<td>Unconnected</td>
</tr>
<tr>
<td>13</td>
<td>Unconnected</td>
<td>14</td>
<td>Unconnected</td>
</tr>
<tr>
<td>15</td>
<td>GND</td>
<td>16–26</td>
<td>Unconnected</td>
</tr>
</tbody>
</table></div>
<p>Pin numbering is taken from the underside-mounted footprint. Use the native project to check which physical end is pin 1 rather than treating an underside view as a top view.</p>
<h2 id="firmware-configuration">Firmware configuration</h2>
<p>The schematic connects U2 TXD to <strong>GPIO51</strong> and RXD to <strong>GPIO52</strong>. To enable this wiring, add these build flags to the GUITION hardware profile's existing flags:</p>
<pre><code class="language-ini">-DCAN_TX=51
-DCAN_RX=52
</code></pre>
<p>The supplied firmware profile does not enable those pins automatically. Once configured, choose <strong>CAN</strong> in Settings and use a bitrate matching the vehicle; the default is 500 kbit/s. See <a href="../../../connections/">connections and proxy</a> for transport selection and controller discovery, and <a href="../">GUITION hardware</a> for the rest of the board setup.</p>
<h2 id="editable-source">Editable source</h2>
<p><a href="../../../pcb/guition-43/Guition_CAN_PCB.epro2">Download the EasyEDA Pro project</a>. The supplied archive records editor version <strong>3.2.209</strong> and includes the schematic, PCB, footprints and component metadata. Import it into EasyEDA Pro as a project archive; keep the <code>.epro2</code> extension.</p>
<p>The supplied layout is <strong>35 × 18 mm</strong>, with 1 mm corner radii and an approximately 1.6 mm two-layer stack. These details describe the supplied design, rather than a verified fabricated assembly. Gerbers, drill files and an assembly-ready manufacturing package are not included here.</p>
`,headings:[{id:`setup-checklist`,title:`Setup checklist`,level:2},{id:`labelled-board-views`,title:`Labelled board views`,level:2},{id:`top-layout`,title:`Top layout`,level:3},{id:`underside-layout`,title:`Underside layout`,level:3},{id:`native-combined-layer-preview`,title:`Native combined-layer preview`,level:3},{id:`schematic`,title:`Schematic`,level:3},{id:`components`,title:`Components`,level:2},{id:`vehicle-connector-cn1`,title:`Vehicle connector CN1`,level:2},{id:`display-socket-u1`,title:`Display socket U1`,level:2},{id:`firmware-configuration`,title:`Firmware configuration`,level:2},{id:`editable-source`,title:`Editable source`,level:2}],search:`This board adds a CAN transceiver and vehicle connector to the GUITION 4.3-inch display's expansion header. It is a separate PCB project from the case and handlebar mount. Setup checklist Check the connector pin numbers and mating-side orientation before making a cable. Confirm 5 V power, ground, CANH and CANL against the tables below. Fit the 120 Ω termination only if this board is at a bus end that needs it. Use firmware configured for CAN TX GPIO51 / RX GPIO52, then select CAN in Settings. The supplied design has not been verified as a fabricated assembly. Check the native project before manufacturing or wiring. Labelled board views This source-derived isometric illustration uses the actual board outline, placements, footprints, pads and track segments. Component bodies are simplified, connector heights are illustrative and copper pours are omitted. It is not a native vendor-model render or a mechanical clearance model. U1 is mounted on the underside, while the other components are on the top side. Top layout Underside layout These side-specific assembly illustrations are generated from the source geometry. They show each side's tracks and component outlines, plus through-hole pads; copper pours are omitted. The underside is mirrored for physical viewing. Use the native project for manufacturing plots and connector pin numbering. Native combined-layer preview Schematic Components Reference Supplied part Function U1 2 × 13 female header, 2.54 mm pitch Underside display expansion socket CN1 HC-PH-6AWT, six positions, 2 mm pitch Vehicle wiring connector U2 NXP TJA1057GT/3J, SOIC-8 CAN transceiver with 5 V supply and 3.3 V logic supply R1 120 Ω, 0402 CANH-to-CANL termination C1 100 nF, 0603 5 V supply bypass C2 100 nF, 0603 3.3 V logic supply bypass R1 is wired directly across CANH and CANL in this design. Populate termination according to the board's position on the bus; do not add an extra terminating resistor to an already terminated intermediate node. Vehicle connector CN1 Pin Net Purpose 1 +5V 5 V rail 2 GND Common ground 3 CANH CAN high 4 CANL CAN low 5 GPIO31 Additional display GPIO 6 GPIO30 Additional display GPIO These are the source's pin numbers. Confirm the pin-1 marker and mating-side orientation in the editor and on the actual connector before making a cable. The footprint's pads 7 and 8 are unconnected mechanical pads, not additional signal pins. Display socket U1 Pin Net Pin Net 1 +5V 2 +3v3 3 +5V 4 +3v3 5 GND 6 GND 7 Unconnected 8 GPIO52 / CAN RX 9 GPIO31 10 GPIO51 / CAN TX 11 GPIO30 12 Unconnected 13 Unconnected 14 Unconnected 15 GND 16–26 Unconnected Pin numbering is taken from the underside-mounted footprint. Use the native project to check which physical end is pin 1 rather than treating an underside view as a top view. Firmware configuration The schematic connects U2 TXD to GPIO51 and RXD to GPIO52. To enable this wiring, add these build flags to the GUITION hardware profile's existing flags: -DCAN_TX=51 -DCAN_RX=52 The supplied firmware profile does not enable those pins automatically. Once configured, choose CAN in Settings and use a bitrate matching the vehicle; the default is 500 kbit/s. See connections and proxy for transport selection and controller discovery, and GUITION hardware for the rest of the board setup. Editable source Download the EasyEDA Pro project. The supplied archive records editor version 3.2.209 and includes the schematic, PCB, footprints and component metadata. Import it into EasyEDA Pro as a project archive; keep the .epro2 extension. The supplied layout is 35 × 18 mm, with 1 mm corner radii and an approximately 1.6 mm two-layer stack. These details describe the supplied design, rather than a verified fabricated assembly. Gerbers, drill files and an assembly-ready manufacturing package are not included here.`};export{e as default};