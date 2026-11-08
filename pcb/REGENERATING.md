# Regenerating PCB documentation assets

Agent/maintainer instructions only, following [the CAD regeneration workflow](../cad/REGENERATING.md). Do not add this file to public Pages routes, links or artifacts. Rider-facing wiring, downloads and pictures belong in each board's `README.md`.

## Source and scope

The GUITION CAN breakout source is [guition-43/Guition_CAN_PCB.epro2](guition-43/Guition_CAN_PCB.epro2), a native **EasyEDA Pro** project archive. Keep it under `pcb/guition-43/` alongside the public wiring guide. The source supplied on 8 November 2026 records editor version **3.2.209** and SHA-256 **54203559f98167aa1baa23f7ff27a0e4ecd5499a7ac1770475f69fe6f1c0d4a2**. This is provenance for that export, not a hash to impose on later designs.

When the user provides a new export, copy it without modifying its bytes. Compare the source and repository hashes, check the archive CRC, and inspect the actual current schematic, layout, pin assignments, components and layer stack before updating the guide. Do not rename an unrelated file to `.epro2`, upload the design to a cloud editor, or save changes to the user's editor project without authorization. Keep generated Gerbers or fabrication claims out of the guide unless they have actually been exported and checked.

## Repeatable asset generator

Run [scripts/generate_pcb_assets.py](../scripts/generate_pcb_assets.py) from the checkout using Python with Pillow installed:

```powershell
python scripts/generate_pcb_assets.py
```

The default input is `pcb/guition-43/Guition_CAN_PCB.epro2`; `--source` and `--output` can select a new source export and a temporary QA output directory. The recipe is specific to this board's labelled component set, rather than a general EasyEDA renderer. Update its reviewed mappings when the design changes; do not silently reuse old labels on a different board.

The generator:

1. Opens the archive with `zipfile`, verifies every entry's CRC and requires one `.epru` drawing database.
2. Reads its `JSON header || JSON body |` records, splitting only once at `||`. Records form an edit log: later records replace the same type/ID and empty bodies delete them. Start a new drawing section at each `DOCHEAD`.
3. Extracts embedded native previews as `docs/images/guition-can-pcb-layout.png` and `guition-can-pcb-schematic.png`. Preview IDs are explicit and must be visually re-identified after an editor export changes them; do not infer board versus schematic from filename order.
4. Creates `docs/images/guition-can-pcb-labelled.svg`, a self-contained labelled top/underside isometric illustration from the current outline, component footprints, pad locations and copper line segments. The source hash is included in the illustration.
5. Creates separate `guition-can-pcb-top.png` and `guition-can-pcb-bottom.png` assembly layouts from source tracks, pads, vias and package contours. The underside image is mirrored horizontally to represent looking at the physical board from below. Through-hole pads appear on both sides; SMD pads and package contours appear only on their mounting side. Copper pours are omitted. These are source-derived assembly illustrations, not native editor fabrication plots. The PNG generator uses Arial on Windows or DejaVu Sans where available; use the same font environment when comparing byte reproducibility.
6. Rejects changed outlines, unexpected components, unsupported contour commands or missing previews so the maintainer must review new geometry.

The archive currently contains image previews and external 3D model identifiers, **not embedded 3D meshes**. The SVG therefore uses simplified package bodies. It is not a native 3D capture, fabrication drawing or clearance model; retain that description in the SVG and public caption. Do not replace it with an AI-generated board image. Native PCB/schematic previews remain alongside it to show the actual editor output.

## Coordinate and component checks

Coordinates are in mils; multiply by **0.0254** for millimetres. The current outline spans X=0…35 mm and Y=-18…0 mm, with 1 mm corner radii. The layer stack is approximately 1.6 mm overall. Those are values to verify again after a source update.

Top-side placements use the component's rotation then translation. For the bottom-side U1 footprint, mirror **local Y** before rotation and translation. Keep top and underside views explicitly labelled. Do not show U1 installed on the top face or accidentally mirror its pin numbers. Pin 1's local position in the current U1 footprint is (-600, -50) mil; with the bottom placement (690, -130) mil it becomes (90, -80) mil in board coordinates.

Current expected components are U1, CN1, U2, R1, C1 and C2. Package heights in the illustration are 1.7 mm for U2, 0.8 mm for the capacitors and 0.4 mm for R1, from source model names. The 3.5 mm CN1 and 4 mm U1 heights are **illustrative assumptions**. Copper pours, full connector interiors and detailed leads are not modelled. Never use these approximations for checking case fit.

Verify the public tables against schematic and PCB pad nets: CN1 pins 1–6 are +5V, GND, CANH, CANL, GPIO31 and GPIO30; U2 TXD/RXD are GPIO51/GPIO52; R1 is 120 Ω across CANH/CANL. Check supplies, termination and footprint pin-1 orientation again rather than carrying them forward from this document. Keep the optional CAN build flags consistent with the current schematic, without silently enabling hardware pins in the firmware target.

## Preferred native 3D export

When EasyEDA Pro UI automation is available, import/open the supplied archive locally and inspect its native 3D view. Resolve external model references only through the editor's supported workflow. Capture both component side and underside with a neutral dark background, consistent camera and enough margin for labels. Preserve the real component geometry, materials and placements. If exporting a board STEP, verify it actually contains the PCB and all populated parts before presenting it as a clearance model.

Use supported computer-use tools and read their skill first; do not substitute guessed private editor APIs. If editor access or external models are unavailable, use the source-based illustration and retain its approximation caption. Do not claim a native capture was made until one has been obtained and inspected.

If native renders replace the simplified illustration, update the public caption and this recipe to describe exactly how they were made. Keep the original schematic/layout previews and editable source download. Put intermediate renders and extraction metadata under `.cache/`, not in the repository.

## Documentation and validation

The public board guide is `pcb/guition-43/README.md`, routed as `hardware/guition-43/can-breakout` in `scripts/build_docs.py`. It must appear directly below the GUITION hardware pages with a **CAN breakout (PCB)** navigation label. Keep the parent hardware page's link current.

The Pages builder copies `.epro2` downloads and documentation images, without copying this file or the Python recipe. Keep `scripts/publish_pages.py`'s static allowlist aligned with the native file extension; do not relax path, symlink or size checks. Test both production and PR preview publication to avoid a repeat of rejected hardware downloads.

```powershell
python scripts/generate_pcb_assets.py
python -m ruff format --check scripts tests
.cache/web-test-venv/Scripts/python.exe scripts/build_docs.py --output .cache/pages/site --images .cache/pages/docs-images
.cache/web-test-venv/Scripts/python.exe tests/docs_browser_test.py --site .cache/pages/site --output .cache/pcb-docs-checks --browser-executable 'C:/Program Files/Google/Chrome/Application/chrome.exe'
.cache/web-test-venv/Scripts/python.exe tests/preview_browser_test.py --site .cache/pages/site --browser-executable 'C:/Program Files/Google/Chrome/Application/chrome.exe'
python -m unittest discover -s tests -p publish_pages_tests.py
git diff HEAD --check
```

Use current dependency/tool paths if they change. Inspect the generated SVG and all four PNGs, the final board page at desktop and mobile sizes, all leader lines and labels, and the source pin mappings. Verify both side-specific layout images are present, with correct layer selection and underside mirroring; the combined native preview does not replace them. Compare published project/image bytes with repository originals and confirm `REGENERATING.md` and generation scripts are absent from the Pages tree. Re-run generation to a temporary output and compare hashes to check reproducibility. Leave the user's native project unmodified.
