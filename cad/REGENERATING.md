# Regenerating display CAD and assembly documentation

Agent/maintainer instructions only. This file and the module-specific `REGENERATING.md` files must remain absent from the public Pages routes, links and artifacts. User-facing assembly instructions belong in each module's `README.md`.

## Source and scope

Inspect the connected Fusion document before every regeneration. The current saved source is **Wheelie Fun Display - Waveshare 2.06**, version **17**, exported on **8 November 2026** with no unsaved changes. Despite the historical name, it contains three grouped assemblies:

- `01_Waveshare_Landscape`: Waveshare landscape case and clamp.
- `02_Waveshare_Portrait`: Waveshare portrait case and clamp.
- `03_Guition_Landscape_PROTOTYPE`: GUITION landscape case and clamp.

The verified archive has **63 occurrences, 20 components, 147 user parameters and 46 timeline items**. These are provenance records, not constants to impose on newer designs. Read the actual source version, modification state, groups, transforms, body geometry, parameters and materials each time. The shared saddle, cap, liner and fasteners occur in all three assemblies; changing them requires regenerating both module guides.

The source contains only local components. Export one complete **F3D** to `cad/wheelie-fun-display-mounts.f3d`, retaining all assemblies, references, parameters and timeline. Do not export duplicate whole-design archives under the individual modules. Use a genuine F3Z export only if the design gains external linked components; never rename an F3D to F3Z. Do not save or rename the user's cloud design without instruction.

## Run the Fusion exporter

The executable recipe is [scripts/export_cad.py](../scripts/export_cad.py). Read it and adjust `ROOT` for another checkout. Use the connected Fusion tools' `apiDocumentation` query for unfamiliar API members; do not guess signatures. Pass the file's complete text to the Fusion `script` execution tool. It defines `run(_context: str)` and imports `adsk`; do **not** run it with ordinary desktop Python or set `readOnly: true` for this export/render operation.

The exporter:

1. Exports the current source to the shared F3D before changing anything.
2. Imports that archive into a temporary document and compares occurrence, component, parameter and timeline counts with the source.
3. Exports five one-solid STEP files per module: housing, saddle, lower cap, liner and cable grommet. Native bodies are copied into empty temporary components, so nested grommets or reference cables never leak into the casing STEP. Body volume is checked after copying.
4. Captures five 2000 × 1400 anti-aliased, transparent-background stills per module: mounted, clamp, casing, liner and grommet. Source material/appearance is preserved, with **no temporary gray override**. The mounted view fits the cases first and then enables the long handlebar as intentionally cropped context.
5. Captures 57 animation frames at 900 × 1000 under `.cache/cad-assembly-<module>/`, with a fixed orthographic camera and smoothstep easing. The entire display reference including glass, cable jackets, handlebar and other assemblies are hidden.
6. Restores exact assembled transforms, closes only the temporary document with `close(False)` in `finally`, and reactivates the user's original document. Its modification state must remain unchanged.
7. Writes source version, counts, parameter values, body materials, appearances and dimensions to `.cache/cad-export-manifest.json` for updating the guides.

The exporter discovers assembly groups by their prefixes and parts by exact component names. Update those mappings only after inspecting an intentional source change. An unmatched group, extra/missing body or unknown animation part must fail rather than silently generate misleading assets.

## Transform and visibility rules

Use **root-context proxies from `rootComponent.allOccurrences`**, including for nested parts. The old root-only/X<5 recipe does not work with the grouped version-17 design. Proxy `transform2` matrices are world transforms; apply parent offsets before child offsets and always start from saved original matrices.

- Upper saddle stays fixed. Housing and inserts explode +1.8 cm in world Z.
- Grommet explodes +4.2 cm total in world Z, keeping its larger flange inside the final housing. This already includes the housing's motion; do not add that motion twice.
- Lower cap explodes -2 cm; liner moves +2.8 cm in world X; M4 screws move -3.2 cm in world Z.
- Waveshare M2 display screws approach **upward from below**, using -2.6 cm world-Z offsets.
- GUITION has four extra M3 display screws translated to source world Z=11 mm; these also explode **downward** by 2.6 cm. The four M3 saddle screws use -1.2 cm. Keep original screw orientation.

The fully assembled pose must exactly match the source. The animation is a fit overview, not a prescribed fastening or machining sequence. Hide whole reference display occurrences, not just their cover-glass bodies, so no floating dummy screen plate remains.

## Model-specific checks

See [Waveshare notes](waveshare-206/REGENERATING.md) and [GUITION notes](guition-43/REGENERATING.md). Preserve the split liner's 0.5 mm slit through its full 0.5 mm wall and 14.3 mm width. The cable grommets are **unsplit** and have a nominal 6 mm cable jacket / 5.8 mm bore, 7.8 mm case hole / 7.9 mm barrel, 3.8 mm grip and 9.4 / 8.6 mm inner / outer flanges. Verify current parameters and final BRep before repeating those dimensions in the guide.

The GUITION reference display's 5 mm depth and 0.5 mm glass thickness remain explicitly unverified, as do its reference display screws. Preserve that fit limitation in the CAD guide without mislabelling the supported firmware target experimental. Treat Fusion's generic Rubber material separately from the parts' TPU75A designation and nominal hardness parameter.

## Encode and validate

Run `python scripts/encode_cad.py` using Python with Pillow installed. It creates each module's static exploded PNG and 57-frame GIF from real Fusion captures, with a shared 192-color palette and dark compositing to avoid flicker and GIF transparency artifacts. It checks all frames decode, normalizes STEP text to LF without trailing whitespace and requires one `MANIFOLD_SOLID_BREP(` plus ISO-10303-21 markers in every STEP. Fusion currently exports explicitly declared centimetre (`.CENTI.`) length units even though the source display units and guide dimensions are millimetres. Honour the STEP unit declaration; never relabel `.CENTI.` to `.MILLI.` without scaling the geometry. Reimport the casing STEP files in temporary Fusion documents and verify physical extents: Waveshare 42 × 51 × 13.5 mm, GUITION 95 × 63 × 11.2 mm. Export axes may follow the posed assembly orientation, so compare dimensions independent of X/Y order.

Inspect all stills, exploded and assembled poses and intermediate animation frames. Verify glass/reference cable are absent, screws approach from below, grommets seat inside and the cases are not clipped. Cropping the context handlebar in mounted views is intentional. Final PNGs/GIFs belong in `docs/images/`; intermediate frames and assembled QA captures belong under `.cache/`.

The F3D uses ZIP compression method 93 (Zstandard), unsupported by Python 3.13's `zipfile` and older 7-Zip versions. An unsupported-codec error is not evidence of a corrupt archive. Reopen in Fusion and use a capable reader: `tar -xOf cad/wheelie-fun-display-mounts.f3d > $null` works with the installed Windows tar; require `$LASTEXITCODE` zero.

Update both guides' provenance, materials, dimensions, hardware and installation sequence. Keep the CAD index and parent hardware pages current. Each case/mount page must appear directly below its hardware page in the Pages navigation. Routes are registered in `scripts/build_docs.py`; its asset copier includes STEP/F3D/F3Z downloads and source images, while excluding these instructions and Python recipes. Keep `scripts/publish_pages.py`'s asset allowlist aligned without bypassing path, symlink or size checks.

```powershell
python scripts/encode_cad.py
python -m ruff format --check scripts tests
.cache/web-test-venv/Scripts/python.exe scripts/build_docs.py --output .cache/pages/site --images .cache/pages/docs-images
.cache/web-test-venv/Scripts/python.exe tests/docs_browser_test.py --site .cache/pages/site --output .cache/cad-docs-checks --browser-executable 'C:/Program Files/Google/Chrome/Application/chrome.exe'
.cache/web-test-venv/Scripts/python.exe tests/preview_browser_test.py --site .cache/pages/site --browser-executable 'C:/Program Files/Google/Chrome/Application/chrome.exe'
python -m unittest discover -s tests -p publish_pages_tests.py
git diff --check
```

Compare published CAD download/image bytes with the source, and confirm no `REGENERATING.md` or CAD Python recipe appears in the Pages artifact or links. Use current tooling paths if they change. Do not check in intermediate frames, manifests or QA screenshots.
