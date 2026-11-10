var e={source:`cad/guition-43/README.md`,route:`hardware/guition-43/case-mount`,title:`GUITION case and mount`,group:`Hardware`,description:`Case and clamp STEP downloads, Fusion archive, assembly animation and fit checks.`,body:`<p>Part of the <a href="../">GUITION hardware guide</a>.</p>
<p>A rear housing and handlebar mount for the GUITION JC4880P443C display, using the same aluminium saddle, lower clamp and split liner as the smaller Waveshare assembly. The source assembly shows the GUITION display mounted in landscape.</p>
<p><img src="../../../assets/images/guition-43-mounted.png" alt="GUITION display case and clamp mounted on a handlebar"></p>
<p>These are shaded CAD views using the source materials and appearances. The blank display, cover glass, cable jacket and handlebar are reference geometry. The handlebar is cropped in the mounted view to keep the case visible.</p>
<p>The source GUITION display component is explicitly named <code>G02_REF_OEM_display_DEPTH_UNVERIFIED</code>. Its <strong>5 mm module depth</strong>, including <strong>0.5 mm reference glass</strong>, is an assumption in this CAD, not a verified vendor dimension. Check your actual module thickness, mounting threads, screw engagement, PCB/components, USB access and cable clearance before manufacturing the case. Firmware support for this board is described separately in the hardware guide.</p>
<h2 id="before-you-make-parts">Before you make parts<a class="heading-anchor" href="#before-you-make-parts" aria-label="Link to Before you make parts" title="Link to Before you make parts"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<ol>
<li>Check your real display, mounting screws and connector clearance against the model.</li>
<li>Download the STEP files below for fabrication, or the Fusion archive near the end to edit the design.</li>
<li>Gather the listed fasteners and test-fit the casing, liner and cable grommet before final assembly.</li>
</ol>
<h2 id="download-the-parts">Download the parts<a class="heading-anchor" href="#download-the-parts" aria-label="Link to Download the parts" title="Link to Download the parts"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>Each STEP contains one solid at its modelled size and carries its own length units. Import at 1:1 scale and honour the file units; dimensions in this guide are in millimetres. Reference electronics, cable, handlebar and standard fasteners are excluded.</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Part</th>
<th>STEP download</th>
<th>Source component / material</th>
</tr>
</thead>
<tbody>
<tr>
<td>Rear housing</td>
<td><a href="../../../cad/guition-43/rear-casing.step">rear-casing.step</a></td>
<td><code>G01_Rear_housing_PRINT_PROTO</code> / ABS Plastic</td>
</tr>
<tr>
<td>Upper saddle</td>
<td><a href="../../../cad/guition-43/upper-saddle.step">upper-saddle.step</a></td>
<td><code>03_Upper_saddle_AL6061</code> / aluminium 6061</td>
</tr>
<tr>
<td>Lower clamp cap</td>
<td><a href="../../../cad/guition-43/lower-clamp-cap.step">lower-clamp-cap.step</a></td>
<td><code>04_Lower_cap_AL6061</code> / aluminium 6061</td>
</tr>
<tr>
<td>Split liner</td>
<td><a href="../../../cad/guition-43/rubber-liner.step">rubber-liner.step</a></td>
<td><code>HW_split_rubber_liner_0p5</code> / Rubber</td>
</tr>
<tr>
<td>Cable grommet</td>
<td><a href="../../../cad/guition-43/cable-grommet.step">cable-grommet.step</a></td>
<td><code>G03_Snap_grommet_TPU75A</code> / Rubber, nominal TPU75A designation</td>
</tr>
</tbody>
</table></div>
<p>The clamp files are machining geometry for the model's aluminium parts. No slicer profiles or machining toolpaths are supplied. Check fit with the actual printed material and cable; the generic Fusion Rubber material does not qualify a specific TPU formulation.</p>
<h2 id="dimensions-and-hardware">Dimensions and hardware<a class="heading-anchor" href="#dimensions-and-hardware" aria-label="Link to Dimensions and hardware" title="Link to Dimensions and hardware"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Feature</th>
<th>Nominal source dimension</th>
</tr>
</thead>
<tbody>
<tr>
<td>Housing outside width × height</td>
<td>95 × 63 mm</td>
</tr>
<tr>
<td>Housing overall depth, including rear pad</td>
<td>11.2 mm</td>
</tr>
<tr>
<td>Frame corner radius / wall parameter</td>
<td>4 / 2.4 mm</td>
</tr>
<tr>
<td>Reference display footprint</td>
<td>94.5 × 62 mm</td>
</tr>
<tr>
<td>Reference display depth, unverified</td>
<td>5 mm, including 0.5 mm glass</td>
</tr>
<tr>
<td>Display screw pattern</td>
<td>84.5 × 52.5 mm</td>
</tr>
<tr>
<td>Display screw head recess diameter / retaining web</td>
<td>6.4 / 2 mm</td>
</tr>
<tr>
<td>Case-to-saddle mounting pitch</td>
<td>20 × 20 mm</td>
</tr>
<tr>
<td>Handlebar diameter</td>
<td>22.225 mm (7/8 inch)</td>
</tr>
<tr>
<td>Clamp bore / width</td>
<td>23.225 / 16 mm</td>
</tr>
<tr>
<td>Liner wall / retained width / installation split</td>
<td>0.5 / 14.3 / 0.5 mm</td>
</tr>
<tr>
<td>Cable jacket OD / casing hole</td>
<td>6 / 7.8 mm</td>
</tr>
<tr>
<td>Grommet bore / barrel OD / panel grip</td>
<td>5.8 / 7.9 / 3.8 mm</td>
</tr>
<tr>
<td>Grommet inner / outer flange OD</td>
<td>9.4 / 8.6 mm</td>
</tr>
</tbody>
</table></div>
<p>The model contains the following hardware per GUITION assembly:</p>
<ul>
<li>Eight M3 × 6 ISO 4762 socket-head screws: four for the case-to-saddle joint and four reference display screws.</li>
<li>Four M3 heat-set inserts, nominal 5 mm OD × 4 mm long, for the case-to-saddle joint.</li>
<li>Two M4 × 12 DIN 7984 low-head socket screws for the handlebar clamp.</li>
<li>One split rubber liner and one one-piece cable grommet.</li>
</ul>
<p>The four display screws use the model's <code>GDisplayScrewLength_REF</code> parameter; their 6 mm length and thread compatibility must be checked against the real display. The upper saddle's 3.3 mm M4 tap-drill holes still require the appropriate tapping operation; STEP geometry does not provide threads or toolpaths.</p>
<h2 id="housing-and-wiring">Housing and wiring<a class="heading-anchor" href="#housing-and-wiring" aria-label="Link to Housing and wiring" title="Link to Housing and wiring"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p><img src="../../../assets/images/guition-43-casing.png" alt="GUITION housing interior with corner display seats, mounting islands and cable entry"></p>
<p><img src="../../../assets/images/guition-43-grommet.png" alt="GUITION snap-in cable grommet with its bore and retaining flanges"></p>
<ol>
<li>Verify the real display against the source's reference envelope and screw pattern. Check component and connector access before committing to a finished housing.</li>
<li>Prepare the case and fit its four M3 heat-set inserts. Test the saddle mounting screws for engagement without contact with the display or wiring.</li>
<li>Thread the cable through the <strong>one-piece, unsplit grommet</strong> before attaching any connector too large for its 5.8 mm bore. The 6 mm cable dimension is the outside jacket diameter, not an individual conductor.</li>
<li>Feed the cable through the case opening and seat the grommet from inside. The <strong>larger 9.4 mm flange remains inside</strong>, the smaller 8.6 mm flange outside, and the 3.8 mm panel stack sits in its retaining groove. Check both lips are seated without folding.</li>
<li>Route the wiring clear of the display, corner seats, insert bosses and mounting screws. Fit the display using its four verified screws from underneath; the source shows M3 × 6 reference screws on the 84.5 × 52.5 mm pattern.</li>
<li>Fasten the housing to the upper saddle with four M3 × 6 screws, keeping the cable free of the joint.</li>
</ol>
<p>The grommet fit has not been qualified for a particular print, cable pull load or water-ingress rating. Check retention and any required sealing on the completed assembly.</p>
<h2 id="clamp-and-split-liner">Clamp and split liner<a class="heading-anchor" href="#clamp-and-split-liner" aria-label="Link to Clamp and split liner" title="Link to Clamp and split liner"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p><img src="../../../assets/images/guition-43-clamp.png" alt="GUITION underside view with the clamp, liner and fastener access"></p>
<p><img src="../../../assets/images/guition-43-liner.png" alt="Split rubber liner for installation around an existing handlebar"></p>
<p>Open the liner's <strong>0.5 mm longitudinal split</strong> and wrap it around the bar without removing the grips. The split extends through the 0.5 mm wall and full 14.3 mm width. Seat the liner between the clamp's retaining lips, fit the lower cap and install its two M4 screws. Confirm the display and cable remain clear throughout the required handlebar and steering movement.</p>
<h2 id="assembly-animation">Assembly animation<a class="heading-anchor" href="#assembly-animation" aria-label="Link to Assembly animation" title="Link to Assembly animation"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p><img src="../../../assets/images/guition-43-assembly.gif" alt="Exploded-to-assembled GUITION housing, grommet, clamp, liner and screws"></p>
<p>The animation shows how the parts fit, rather than prescribing a fastening or machining sequence. The handlebar, reference cable and entire reference display, including glass, are hidden. Both the display screws and clamp fasteners approach from underneath; the grommet seats into the case from inside. <a href="../../../assets/images/guition-43-exploded.png">View the static exploded image</a>.</p>
<h2 id="orientation">Orientation<a class="heading-anchor" href="#orientation" aria-label="Link to Orientation" title="Link to Orientation"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>The CAD includes one GUITION landscape assembly. Its square 20 × 20 mm case-to-saddle pattern also permits a 90° change in case orientation, subject to checking the larger display's cable and mechanical clearance. A separate GUITION portrait assembly is not posed in this source.</p>
<p>Set <strong>Settings &gt; Orientation</strong> to match the physical installation. The firmware supports 0°, 90°, 180° and 270° with matching touch coordinates.</p>
<h2 id="editable-fusion-design">Editable Fusion design<a class="heading-anchor" href="#editable-fusion-design" aria-label="Link to Editable Fusion design" title="Link to Editable Fusion design"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>Download the <a href="../../../cad/wheelie-fun-display-mounts.f3d">complete editable Fusion archive</a>. It contains the GUITION assembly, both Waveshare orientations, shared hardware, user parameters and the source timeline. All components are local, so the archive is F3D rather than an F3Z package.</p>
<h2 id="source-and-updates">Source and updates<a class="heading-anchor" href="#source-and-updates" aria-label="Link to Source and updates" title="Link to Source and updates"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M10 13a5 5 0 0 0 7 .1l3-3a5 5 0 0 0-7.1-7.1l-1.7 1.7M14 11a5 5 0 0 0-7-.1l-3 3a5 5 0 0 0 7.1 7.1l1.7-1.7"></path></svg></a></h2>
<p>Exported from <strong>Wheelie Fun Display - Waveshare 2.06</strong>, saved Fusion version <strong>17</strong>, with no unsaved changes, on <strong>8 November 2026</strong>. Despite its historical document name, this source now contains both display families. STEP exports preserve solid geometry, while the shared F3D retains the editable source design.</p>
<p>These project CAD exports use the repository's <a href="https://github.com/techfoundrynz/wheelie_fun_display/blob/main/LICENSE">GPL-3.0 license</a>.</p>
`,headings:[{id:`before-you-make-parts`,title:`Before you make parts`,level:2},{id:`download-the-parts`,title:`Download the parts`,level:2},{id:`dimensions-and-hardware`,title:`Dimensions and hardware`,level:2},{id:`housing-and-wiring`,title:`Housing and wiring`,level:2},{id:`clamp-and-split-liner`,title:`Clamp and split liner`,level:2},{id:`assembly-animation`,title:`Assembly animation`,level:2},{id:`orientation`,title:`Orientation`,level:2},{id:`editable-fusion-design`,title:`Editable Fusion design`,level:2},{id:`source-and-updates`,title:`Source and updates`,level:2}],search:`Part of the GUITION hardware guide. A rear housing and handlebar mount for the GUITION JC4880P443C display, using the same aluminium saddle, lower clamp and split liner as the smaller Waveshare assembly. The source assembly shows the GUITION display mounted in landscape. These are shaded CAD views using the source materials and appearances. The blank display, cover glass, cable jacket and handlebar are reference geometry. The handlebar is cropped in the mounted view to keep the case visible. The source GUITION display component is explicitly named G02_REF_OEM_display_DEPTH_UNVERIFIED. Its 5 mm module depth, including 0.5 mm reference glass, is an assumption in this CAD, not a verified vendor dimension. Check your actual module thickness, mounting threads, screw engagement, PCB/components, USB access and cable clearance before manufacturing the case. Firmware support for this board is described separately in the hardware guide. Before you make parts Check your real display, mounting screws and connector clearance against the model. Download the STEP files below for fabrication, or the Fusion archive near the end to edit the design. Gather the listed fasteners and test-fit the casing, liner and cable grommet before final assembly. Download the parts Each STEP contains one solid at its modelled size and carries its own length units. Import at 1:1 scale and honour the file units; dimensions in this guide are in millimetres. Reference electronics, cable, handlebar and standard fasteners are excluded. Part STEP download Source component / material Rear housing rear-casing.step G01_Rear_housing_PRINT_PROTO / ABS Plastic Upper saddle upper-saddle.step 03_Upper_saddle_AL6061 / aluminium 6061 Lower clamp cap lower-clamp-cap.step 04_Lower_cap_AL6061 / aluminium 6061 Split liner rubber-liner.step HW_split_rubber_liner_0p5 / Rubber Cable grommet cable-grommet.step G03_Snap_grommet_TPU75A / Rubber, nominal TPU75A designation The clamp files are machining geometry for the model's aluminium parts. No slicer profiles or machining toolpaths are supplied. Check fit with the actual printed material and cable; the generic Fusion Rubber material does not qualify a specific TPU formulation. Dimensions and hardware Feature Nominal source dimension Housing outside width × height 95 × 63 mm Housing overall depth, including rear pad 11.2 mm Frame corner radius / wall parameter 4 / 2.4 mm Reference display footprint 94.5 × 62 mm Reference display depth, unverified 5 mm, including 0.5 mm glass Display screw pattern 84.5 × 52.5 mm Display screw head recess diameter / retaining web 6.4 / 2 mm Case-to-saddle mounting pitch 20 × 20 mm Handlebar diameter 22.225 mm (7/8 inch) Clamp bore / width 23.225 / 16 mm Liner wall / retained width / installation split 0.5 / 14.3 / 0.5 mm Cable jacket OD / casing hole 6 / 7.8 mm Grommet bore / barrel OD / panel grip 5.8 / 7.9 / 3.8 mm Grommet inner / outer flange OD 9.4 / 8.6 mm The model contains the following hardware per GUITION assembly: Eight M3 × 6 ISO 4762 socket-head screws: four for the case-to-saddle joint and four reference display screws. Four M3 heat-set inserts, nominal 5 mm OD × 4 mm long, for the case-to-saddle joint. Two M4 × 12 DIN 7984 low-head socket screws for the handlebar clamp. One split rubber liner and one one-piece cable grommet. The four display screws use the model's GDisplayScrewLength_REF parameter; their 6 mm length and thread compatibility must be checked against the real display. The upper saddle's 3.3 mm M4 tap-drill holes still require the appropriate tapping operation; STEP geometry does not provide threads or toolpaths. Housing and wiring Verify the real display against the source's reference envelope and screw pattern. Check component and connector access before committing to a finished housing. Prepare the case and fit its four M3 heat-set inserts. Test the saddle mounting screws for engagement without contact with the display or wiring. Thread the cable through the one-piece, unsplit grommet before attaching any connector too large for its 5.8 mm bore. The 6 mm cable dimension is the outside jacket diameter, not an individual conductor. Feed the cable through the case opening and seat the grommet from inside. The larger 9.4 mm flange remains inside, the smaller 8.6 mm flange outside, and the 3.8 mm panel stack sits in its retaining groove. Check both lips are seated without folding. Route the wiring clear of the display, corner seats, insert bosses and mounting screws. Fit the display using its four verified screws from underneath; the source shows M3 × 6 reference screws on the 84.5 × 52.5 mm pattern. Fasten the housing to the upper saddle with four M3 × 6 screws, keeping the cable free of the joint. The grommet fit has not been qualified for a particular print, cable pull load or water-ingress rating. Check retention and any required sealing on the completed assembly. Clamp and split liner Open the liner's 0.5 mm longitudinal split and wrap it around the bar without removing the grips. The split extends through the 0.5 mm wall and full 14.3 mm width. Seat the liner between the clamp's retaining lips, fit the lower cap and install its two M4 screws. Confirm the display and cable remain clear throughout the required handlebar and steering movement. Assembly animation The animation shows how the parts fit, rather than prescribing a fastening or machining sequence. The handlebar, reference cable and entire reference display, including glass, are hidden. Both the display screws and clamp fasteners approach from underneath; the grommet seats into the case from inside. View the static exploded image. Orientation The CAD includes one GUITION landscape assembly. Its square 20 × 20 mm case-to-saddle pattern also permits a 90° change in case orientation, subject to checking the larger display's cable and mechanical clearance. A separate GUITION portrait assembly is not posed in this source. Set Settings > Orientation to match the physical installation. The firmware supports 0°, 90°, 180° and 270° with matching touch coordinates. Editable Fusion design Download the complete editable Fusion archive. It contains the GUITION assembly, both Waveshare orientations, shared hardware, user parameters and the source timeline. All components are local, so the archive is F3D rather than an F3Z package. Source and updates Exported from Wheelie Fun Display - Waveshare 2.06, saved Fusion version 17, with no unsaved changes, on 8 November 2026. Despite its historical document name, this source now contains both display families. STEP exports preserve solid geometry, while the shared F3D retains the editable source design. These project CAD exports use the repository's GPL-3.0 license.`};export{e as default};