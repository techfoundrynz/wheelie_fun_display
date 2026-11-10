var e={source:`cad/waveshare-206/README.md`,route:`hardware/waveshare-206/case-mount`,title:`Waveshare case and mount`,group:`Hardware`,description:`STEP downloads, assembly views and hardware for the 2.06-inch casing and clamp.`,body:`<p>Part of the <a href="../">Waveshare 2.06 hardware guide</a>.</p>
<p>A compact casing and split handlebar clamp for the Waveshare ESP32-S3 Touch AMOLED 2.06 module. The Fusion assembly shows portrait and landscape installations of the same custom parts, including a flexible cable-entry grommet.</p>
<p><img src="../../../assets/images/waveshare-206-mounted.png" alt="Portrait and landscape display casings mounted on a handlebar"></p>
<p>These are shaded CAD views of the current design. Display glass and the handlebar are reference geometry; the screen is shown blank. The long handlebar is cropped in the mounted view to keep the two cases visible.</p>
<h2 id="before-you-make-parts">Before you make parts</h2>
<ol>
<li>Check your real display, mounting screws and connector clearance against the model.</li>
<li>Download the STEP files below for fabrication, or the Fusion archive near the end to edit the design.</li>
<li>Gather the listed fasteners and test-fit the casing, liner and cable grommet before final assembly.</li>
</ol>
<h2 id="download-the-parts">Download the parts</h2>
<p>Each STEP file contains one solid component at its modelled size and carries its own length units. Import at 1:1 scale and honour the file units; dimensions in this guide are in millimetres. Standard fasteners, reference display geometry, the handlebar and the reference cable jacket are excluded. The casing export contains only the shell; its nested grommet is supplied separately.</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Part</th>
<th>STEP download</th>
<th>Model designation</th>
</tr>
</thead>
<tbody>
<tr>
<td>Rear casing</td>
<td><a href="../../../cad/waveshare-206/rear-casing.step">rear-casing.step</a></td>
<td><code>01_Rear_body_PRINT</code> — printed shell</td>
</tr>
<tr>
<td>Upper clamp saddle</td>
<td><a href="../../../cad/waveshare-206/upper-saddle.step">upper-saddle.step</a></td>
<td><code>03_Upper_saddle_AL6061</code> — aluminium 6061</td>
</tr>
<tr>
<td>Lower clamp cap</td>
<td><a href="../../../cad/waveshare-206/lower-clamp-cap.step">lower-clamp-cap.step</a></td>
<td><code>04_Lower_cap_AL6061</code> — aluminium 6061</td>
</tr>
<tr>
<td>Split rubber liner</td>
<td><a href="../../../cad/waveshare-206/rubber-liner.step">rubber-liner.step</a></td>
<td><code>HW_split_rubber_liner_0p5</code> — 0.5 mm wall</td>
</tr>
<tr>
<td>Cable-entry grommet</td>
<td><a href="../../../cad/waveshare-206/cable-grommet.step">cable-grommet.step</a></td>
<td><code>06_One_piece_grommet_TPU75A</code> — flexible TPU, nominal Shore 75A</td>
</tr>
</tbody>
</table></div>
<p>The clamp parts are designated aluminium in the source model. The casing and TPU grommet are intended for printing; no slicer profile or machining toolpaths are included.</p>
<h2 id="portrait-or-landscape-mounting">Portrait or landscape mounting</h2>
<p>The same casing and clamp can be assembled in <strong>portrait or landscape</strong>. Rotate the casing 90° relative to the upper saddle before fastening it; the square 20 × 20 mm mounting pattern supports both orientations without different printed parts. The mounted view above shows both arrangements side by side.</p>
<p>On the display, open <strong>Settings &gt; Orientation</strong> to match the UI to the physical mounting. The firmware cycles through <strong>0°, 90°, 180° and 270°</strong>, so either portrait or landscape direction is available. The layout and touch coordinates rotate together.</p>
<h2 id="fit-and-assembly">Fit and assembly</h2>
<p>The source model uses these nominal dimensions:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Feature</th>
<th>Dimension</th>
</tr>
</thead>
<tbody>
<tr>
<td>Casing outside width × height</td>
<td>42 × 51 mm</td>
</tr>
<tr>
<td>Shell depth, before rear mounting pad</td>
<td>10.5 mm</td>
</tr>
<tr>
<td>Display cavity width × height</td>
<td>38.2 × 47 mm</td>
</tr>
<tr>
<td>Handlebar outside diameter</td>
<td>22.225 mm (7/8 inch)</td>
</tr>
<tr>
<td>Rubber liner wall</td>
<td>0.5 mm</td>
</tr>
<tr>
<td>Clamp bore, including liner</td>
<td>23.225 mm</td>
</tr>
<tr>
<td>Clamp width along the handlebar</td>
<td>16 mm</td>
</tr>
<tr>
<td>Retained liner width along the handlebar</td>
<td>14.3 mm</td>
</tr>
<tr>
<td>Liner installation split</td>
<td>0.5 mm, through the wall and full width</td>
</tr>
<tr>
<td>Liner retaining lip width / radial depth</td>
<td>0.75 / 0.2 mm</td>
</tr>
<tr>
<td>Casing-to-saddle mounting pitch</td>
<td>20 × 20 mm</td>
</tr>
<tr>
<td>Casing cable-entry hole</td>
<td>7.8 mm</td>
</tr>
<tr>
<td>Cable jacket outside diameter</td>
<td>6 mm</td>
</tr>
<tr>
<td>Grommet bore / barrel outside diameter</td>
<td>5.8 / 7.9 mm</td>
</tr>
<tr>
<td>Grommet panel grip length</td>
<td>3.8 mm</td>
</tr>
<tr>
<td>Grommet inner / outer flange diameter</td>
<td>9.4 / 8.6 mm</td>
</tr>
</tbody>
</table></div>
<p>The model shows the following hardware <strong>per display assembly</strong>:</p>
<ul>
<li>Four M3 × 6 ISO 4762 socket-head screws.</li>
<li>Four M3 heat-set inserts, nominal 5 mm outside diameter × 4 mm long.</li>
<li>Two M4 × 12 DIN 7984 low-head socket screws for the clamp.</li>
<li>Two M2 × 5 countersunk display screws, modelled with a reference drive.</li>
<li>One split rubber liner.</li>
<li>One one-piece cable-entry grommet in flexible TPU, nominal Shore 75A.</li>
</ul>
<p>The upper saddle has modelled 3.3 mm M4 tap-drill holes; STEP geometry does not supply a machining or tapping operation. Confirm the actual display, insert fit, screw engagement and cable clearance before fabrication. The source display component is explicitly marked as reference geometry to verify, rather than a vendor-accurate manufacturing model.</p>
<h2 id="split-liner-installation">Split liner installation</h2>
<p><img src="../../../assets/images/waveshare-206-liner.png" alt="Close-up of the rubber liner showing its full-width installation split"></p>
<p>The rubber liner has a <strong>0.5 mm longitudinal split</strong> through its full <strong>0.5 mm wall</strong> and <strong>14.3 mm width</strong>. Open the flexible liner at this seam and wrap it around the handlebar; the grips do not need to be removed. Seat it between the clamp's retaining lips before fitting the lower cap. Use flexible rubber for the liner so it can open around the bar.</p>
<h2 id="cable-grommet-installation">Cable grommet installation</h2>
<p><img src="../../../assets/images/waveshare-206-grommet.png" alt="One-piece snap-in cable grommet with its flanges, panel groove and cable bore"></p>
<p>The wiring entry uses a <strong>one-piece snap-in grommet</strong>, modelled for <strong>TPU 75A</strong> and a <strong>6 mm outside-diameter cable jacket</strong>. Download <a href="../../../cad/waveshare-206/cable-grommet.step">cable-grommet.step</a>. The Fusion material is labelled Rubber; use the nominal TPU75A designation as the printing reference and test the actual material fit.</p>
<p>Its nominal <strong>5.8 mm bore</strong> provides 0.2 mm diametral interference with the cable jacket. Its <strong>7.9 mm barrel</strong> fits the casing's <strong>7.8 mm hole</strong>, with a <strong>3.8 mm grip length</strong> matching the modelled panel stack. The <strong>9.4 mm inner flange</strong> stays inside the casing; the <strong>8.6 mm outer flange</strong> sits outside. These are design dimensions, so test the printed fit with your actual material and cable before final assembly.</p>
<ol>
<li>Print the grommet in flexible TPU appropriate to the model's nominal Shore 75A hardness. Check that the bore and retaining groove are open and free of print defects.</li>
<li>Thread the cable through the grommet before fitting any connector that cannot pass through the bore. This grommet is <strong>not split</strong>, so it cannot wrap around an already terminated cable like the handlebar liner wraps around the bar. The 6 mm dimension refers to the complete cable jacket, not an individual wire.</li>
<li>With the display still out of the casing, feed the cable through the casing opening. Seat the grommet from inside the casing, pushing the smaller outer flange through the hole until the panel sits in the groove between the flanges. Check both flanges are fully seated without a folded lip.</li>
<li>Route the wiring clear of the display, insert bosses, mounting screws and clamp. Leave enough slack for the chosen portrait or landscape orientation without pulling on PCB connections. Check the cable's bend and clamp clearance in that orientation before fastening the display.</li>
<li>Fit the display with the M2 screws approaching from underneath, then complete the casing-to-saddle and handlebar-clamp assembly shown below. Check the grommet stays seated as the wiring is gently moved.</li>
</ol>
<p>The CAD fit has not been qualified for a particular printed material, cable pull load or water-ingress rating. Verify retention and any required sealing on the finished assembly.</p>
<h2 id="assembly-animation">Assembly animation</h2>
<p><img src="../../../assets/images/waveshare-206-assembly.gif" alt="Exploded-to-assembled CAD animation of the casing, cable grommet, clamp, liner and fasteners"></p>
<p>The animation separates the components and brings them together to show how they fit. It is an assembly overview, rather than a fastening or machining sequence; the handlebar and reference display, including its cover glass, are omitted for visibility. The cable grommet moves independently into the casing, with its larger flange inside. The M2 display screws approach upward from underneath. <a href="../../../assets/images/waveshare-206-exploded.png">View the static exploded image</a>.</p>
<h2 id="clamp-detail">Clamp detail</h2>
<p><img src="../../../assets/images/waveshare-206-clamp.png" alt="Underside view of the saddle, split clamp, liner and mounting screws"></p>
<p>The underside view omits the handlebar so the liner, split and fastener access are visible. The updated clamp has retaining lips for the narrower rubber liner, with 0.2 mm total axial clearance in its channel.</p>
<h2 id="casing-interior">Casing interior</h2>
<p><img src="../../../assets/images/waveshare-206-casing.png" alt="Isolated printed casing showing the display cavity, mounting islands and cable opening"></p>
<h2 id="editable-fusion-design">Editable Fusion design</h2>
<p>Download the <a href="../../../cad/wheelie-fun-display-mounts.f3d">complete Fusion archive</a> to edit the assembly, user parameters and feature timeline. It includes both Waveshare mounting orientations, the GUITION case and mount, shared hardware, user parameters and reference displays, handlebar and cable jackets.</p>
<p>The F3D archive keeps the editable components, parameters and feature timeline together.</p>
<h2 id="source-and-updates">Source and updates</h2>
<p>Exported from <strong>Wheelie Fun Display - Waveshare 2.06</strong>, Fusion document version <strong>17</strong> (saved source, with no unsaved changes), on <strong>8 November 2026</strong>. These STEP files are solid exports; they do not retain the Fusion parameter timeline. Re-export affected parts when the source design changes and update the views and dimensions here.</p>
<p>These project CAD exports use the repository's <a href="https://github.com/techfoundrynz/wheelie_fun_display/blob/main/LICENSE">GPL-3.0 license</a>.</p>
`,headings:[{id:`before-you-make-parts`,title:`Before you make parts`,level:2},{id:`download-the-parts`,title:`Download the parts`,level:2},{id:`portrait-or-landscape-mounting`,title:`Portrait or landscape mounting`,level:2},{id:`fit-and-assembly`,title:`Fit and assembly`,level:2},{id:`split-liner-installation`,title:`Split liner installation`,level:2},{id:`cable-grommet-installation`,title:`Cable grommet installation`,level:2},{id:`assembly-animation`,title:`Assembly animation`,level:2},{id:`clamp-detail`,title:`Clamp detail`,level:2},{id:`casing-interior`,title:`Casing interior`,level:2},{id:`editable-fusion-design`,title:`Editable Fusion design`,level:2},{id:`source-and-updates`,title:`Source and updates`,level:2}],search:`Part of the Waveshare 2.06 hardware guide. A compact casing and split handlebar clamp for the Waveshare ESP32-S3 Touch AMOLED 2.06 module. The Fusion assembly shows portrait and landscape installations of the same custom parts, including a flexible cable-entry grommet. These are shaded CAD views of the current design. Display glass and the handlebar are reference geometry; the screen is shown blank. The long handlebar is cropped in the mounted view to keep the two cases visible. Before you make parts Check your real display, mounting screws and connector clearance against the model. Download the STEP files below for fabrication, or the Fusion archive near the end to edit the design. Gather the listed fasteners and test-fit the casing, liner and cable grommet before final assembly. Download the parts Each STEP file contains one solid component at its modelled size and carries its own length units. Import at 1:1 scale and honour the file units; dimensions in this guide are in millimetres. Standard fasteners, reference display geometry, the handlebar and the reference cable jacket are excluded. The casing export contains only the shell; its nested grommet is supplied separately. Part STEP download Model designation Rear casing rear-casing.step 01_Rear_body_PRINT — printed shell Upper clamp saddle upper-saddle.step 03_Upper_saddle_AL6061 — aluminium 6061 Lower clamp cap lower-clamp-cap.step 04_Lower_cap_AL6061 — aluminium 6061 Split rubber liner rubber-liner.step HW_split_rubber_liner_0p5 — 0.5 mm wall Cable-entry grommet cable-grommet.step 06_One_piece_grommet_TPU75A — flexible TPU, nominal Shore 75A The clamp parts are designated aluminium in the source model. The casing and TPU grommet are intended for printing; no slicer profile or machining toolpaths are included. Portrait or landscape mounting The same casing and clamp can be assembled in portrait or landscape. Rotate the casing 90° relative to the upper saddle before fastening it; the square 20 × 20 mm mounting pattern supports both orientations without different printed parts. The mounted view above shows both arrangements side by side. On the display, open Settings > Orientation to match the UI to the physical mounting. The firmware cycles through 0°, 90°, 180° and 270°, so either portrait or landscape direction is available. The layout and touch coordinates rotate together. Fit and assembly The source model uses these nominal dimensions: Feature Dimension Casing outside width × height 42 × 51 mm Shell depth, before rear mounting pad 10.5 mm Display cavity width × height 38.2 × 47 mm Handlebar outside diameter 22.225 mm (7/8 inch) Rubber liner wall 0.5 mm Clamp bore, including liner 23.225 mm Clamp width along the handlebar 16 mm Retained liner width along the handlebar 14.3 mm Liner installation split 0.5 mm, through the wall and full width Liner retaining lip width / radial depth 0.75 / 0.2 mm Casing-to-saddle mounting pitch 20 × 20 mm Casing cable-entry hole 7.8 mm Cable jacket outside diameter 6 mm Grommet bore / barrel outside diameter 5.8 / 7.9 mm Grommet panel grip length 3.8 mm Grommet inner / outer flange diameter 9.4 / 8.6 mm The model shows the following hardware per display assembly: Four M3 × 6 ISO 4762 socket-head screws. Four M3 heat-set inserts, nominal 5 mm outside diameter × 4 mm long. Two M4 × 12 DIN 7984 low-head socket screws for the clamp. Two M2 × 5 countersunk display screws, modelled with a reference drive. One split rubber liner. One one-piece cable-entry grommet in flexible TPU, nominal Shore 75A. The upper saddle has modelled 3.3 mm M4 tap-drill holes; STEP geometry does not supply a machining or tapping operation. Confirm the actual display, insert fit, screw engagement and cable clearance before fabrication. The source display component is explicitly marked as reference geometry to verify, rather than a vendor-accurate manufacturing model. Split liner installation The rubber liner has a 0.5 mm longitudinal split through its full 0.5 mm wall and 14.3 mm width. Open the flexible liner at this seam and wrap it around the handlebar; the grips do not need to be removed. Seat it between the clamp's retaining lips before fitting the lower cap. Use flexible rubber for the liner so it can open around the bar. Cable grommet installation The wiring entry uses a one-piece snap-in grommet, modelled for TPU 75A and a 6 mm outside-diameter cable jacket. Download cable-grommet.step. The Fusion material is labelled Rubber; use the nominal TPU75A designation as the printing reference and test the actual material fit. Its nominal 5.8 mm bore provides 0.2 mm diametral interference with the cable jacket. Its 7.9 mm barrel fits the casing's 7.8 mm hole, with a 3.8 mm grip length matching the modelled panel stack. The 9.4 mm inner flange stays inside the casing; the 8.6 mm outer flange sits outside. These are design dimensions, so test the printed fit with your actual material and cable before final assembly. Print the grommet in flexible TPU appropriate to the model's nominal Shore 75A hardness. Check that the bore and retaining groove are open and free of print defects. Thread the cable through the grommet before fitting any connector that cannot pass through the bore. This grommet is not split, so it cannot wrap around an already terminated cable like the handlebar liner wraps around the bar. The 6 mm dimension refers to the complete cable jacket, not an individual wire. With the display still out of the casing, feed the cable through the casing opening. Seat the grommet from inside the casing, pushing the smaller outer flange through the hole until the panel sits in the groove between the flanges. Check both flanges are fully seated without a folded lip. Route the wiring clear of the display, insert bosses, mounting screws and clamp. Leave enough slack for the chosen portrait or landscape orientation without pulling on PCB connections. Check the cable's bend and clamp clearance in that orientation before fastening the display. Fit the display with the M2 screws approaching from underneath, then complete the casing-to-saddle and handlebar-clamp assembly shown below. Check the grommet stays seated as the wiring is gently moved. The CAD fit has not been qualified for a particular printed material, cable pull load or water-ingress rating. Verify retention and any required sealing on the finished assembly. Assembly animation The animation separates the components and brings them together to show how they fit. It is an assembly overview, rather than a fastening or machining sequence; the handlebar and reference display, including its cover glass, are omitted for visibility. The cable grommet moves independently into the casing, with its larger flange inside. The M2 display screws approach upward from underneath. View the static exploded image. Clamp detail The underside view omits the handlebar so the liner, split and fastener access are visible. The updated clamp has retaining lips for the narrower rubber l`};export{e as default};