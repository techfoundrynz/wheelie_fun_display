# Regenerating Waveshare 2.06 CAD assets

Agent/maintainer instructions. This file is intentionally not registered in `scripts/build_docs.py`, linked from the public guides, or copied into the Pages artifact. Public assembly instructions belong in `README.md` beside this file.

## Scope and tools

Work from the repository root (currently `C:/Repos/wfb_disp`). Use the connected Fusion tools to inspect the active document and execute Fusion Python API scripts. Inspect tool schemas and query `apiDocumentation` for unfamiliar API members before use. Scripts must define `run(_context: str)`; use `readOnly: true` only for inspection.

The expected source document is **Wheelie Fun Display - Waveshare 2.06**. Read its actual saved version, active components, user parameters and modification state every time. Do not assume the model or dimensions still match this recipe. Export the user's current design, but do not save their cloud document without explicit instruction.

Use real Fusion renders. Do not synthesize geometry with an image generator or paint over features in screenshots.

## Export STEP parts and native archive

| Component | Destination under `cad/waveshare-206/` |
|---|---|
| `01_Rear_body_PRINT` | `rear-casing.step` |
| `03_Upper_saddle_AL6061` | `upper-saddle.step` |
| `04_Lower_cap_AL6061` | `lower-clamp-cap.step` |
| `HW_split_rubber_liner_0p5` | `rubber-liner.step` |

For each component, require exactly one matching component and one solid body. Use `design.exportManager.createSTEPExportOptions(full_filename, component)`, then require `design.exportManager.execute(options)` to succeed. Export each unique component once, rather than its repeated occurrences.

Export the complete native assembly with `createFusionArchiveExportOptions(full_filename)` to `wheelie-fun-display-waveshare-206.f3d` **before** making temporary visibility or position changes. This is a single local design: F3D preserves the assembly, parameters and timeline. Use a genuine F3Z export only if the source later gains external references; do not rename an F3D archive.

Normalize STEP text to LF after export. Keep native Fusion archives binary. Verify one `MANIFOLD_SOLID_BREP(` in each STEP and its ISO-10303-21 start/end markers.

## Still views

Use a temporary document imported from the exported F3D, rather than moving or hiding things in the user's source. Obtain it using `app.importManager.createFusionArchiveImportOptions(filename)` followed by `importToNewDocument(options)`. Keep a reference to the original document. Close only this temporary document with `close(False)` in `finally`, then `original.activate()`.

The current model has two assemblies: the first at X=0, the second translated +9 cm. Inspect that arrangement if the source changes.

| Image under `docs/images/` | Visible occurrences | Eye / target / up, in Fusion world coordinates |
|---|---|---|
| `waveshare-206-mounted.png` | All; both display orientations and handlebar | (13,-14,22) / (4.5,0,-1) / (0,1,0) |
| `waveshare-206-clamp.png` | First assembly (`transform2.translation.x < 5`), excluding `REF_Handlebar*` | (10,-14,-15) / (0,0,-1.2) / (0,0,1) |
| `waveshare-206-casing.png` | Only occurrence `01_Rear_body_PRINT:1` | (8,-11,17) / (0,0,0.4) / (0,1,0) |
| `waveshare-206-liner.png` | Only occurrence `HW_split_rubber_liner_0p5:1` | (7,-5,-12) / (0,0,-1.95) / (0,0,1) |

Use `ShadedVisualStyle` and `OrthographicCameraType`; for the isolated liner close-up, use `ShadedWithVisibleEdgesOnlyVisualStyle` so the slit is easier to inspect. Set the camera eye, target and up vector; disable smooth transitions and fit once. Apply the camera, refresh, retrieve it again, call `getExtents()` to obtain `(success, width, height)`, disable fitting and apply `setExtents(width * 1.2, height * 1.2)`. This adds framing space.

Capture at **2000 × 1400**, anti-aliased, with transparent background (also removes the construction grid). Create options with `SaveImageFileOptions.create(full_filename)`; set width, height, `isAntiAliased = True` and `isBackgroundTransparent = True`, then require `viewport.saveAsImageFileWithOptions(options)`.

The mounted, clamp and casing views retain their existing glass visibility. The exploded view and animation have different visibility rules below.

## Exploded view and assembly animation

Requirements requested by the user:

- Hide the entire `REF_Display_outline_VERIFY_NOT_FABRICATE` reference component, including `Display cover glass - reference 0.6mm` and its dummy frame/boss body, so no floating screen plate remains.
- Hide the handlebar and the second assembly. These visibility rules apply only to the GIF and static exploded view.
- M2 screws must approach **upward from below**: their exploded offset is **negative world Z**, not positive Z. Do not invert their original orientation.
- Apply offsets relative to saved original occurrence transforms, never cumulatively. Use a fixed camera fitted to the fully exploded pose so it does not zoom between frames.
- Keep the final assembled positions exactly equal to the source model. This is a fit overview, not a prescribed fastening sequence.

Run this script through Fusion's script tool after exporting the native archive. The scratch directory name is historical; update the path in both scripts together if changing it.

```python
import adsk.core, adsk.fusion, os

def run(_context: str):
    app=adsk.core.Application.get()
    original=app.activeDocument
    imported=app.importManager.importToNewDocument(app.importManager.createFusionArchiveImportOptions(r'C:\Repos\wfb_disp\cad\waveshare-206\wheelie-fun-display-waveshare-206.f3d'))
    assert imported
    try:
        d=adsk.fusion.Design.cast(app.activeProduct)
        for component in d.allComponents:
            if component.name.startswith('REF_Display'):
                for body in component.bRepBodies:
                    if 'cover glass' in body.name.lower():
                        body.isLightBulbOn=False
        parts=[]
        for o in d.rootComponent.occurrences:
            visible=o.transform2.translation.x<5 and not o.component.name.startswith(('REF_Handlebar','REF_Display'))
            o.isLightBulbOn=visible
            if visible: parts.append((o,o.transform2))
        offsets={'REF_Display':(0,0,3.6),'01_Rear':(0,0,1.8),'04_Lower':(0,0,-2),'HW_split':(2.8,0,0),'HW_M3_Voron':(0,0,1.8),'HW_M3x6':(0,0,-1.2),'HW_M4':(0,0,-3.2),'HW_M2':(0,0,-2.6)}
        def pose(amount):
            for o,base in parts:
                transform=base.copy()
                v=transform.translation
                offset=next((offset for prefix,offset in offsets.items() if o.component.name.startswith(prefix)),(0,0,0))
                v.x+=offset[0]*amount
                v.y+=offset[1]*amount
                v.z+=offset[2]*amount
                transform.translation=v
                o.transform2=transform
        viewport=app.activeViewport
        viewport.visualStyle=adsk.core.VisualStyles.ShadedVisualStyle
        pose(1)
        camera=viewport.camera
        camera.cameraType=adsk.core.CameraTypes.OrthographicCameraType
        camera.eye=adsk.core.Point3D.create(11,-17,9)
        camera.target=adsk.core.Point3D.create(0,0,-0.5)
        camera.upVector=adsk.core.Vector3D.create(0,0,1)
        camera.isSmoothTransition=False
        camera.isFitView=True
        viewport.camera=camera
        viewport.refresh()
        camera=viewport.camera
        success,width,height=camera.getExtents()
        assert success
        camera.isFitView=False
        assert camera.setExtents(width*1.2,height*1.2)
        viewport.camera=camera
        folder=r'C:\Repos\wfb_disp\.cache\cad-assembly-v12'
        os.makedirs(folder,exist_ok=True)
        amounts=[1-(i/28)**2*(3-2*i/28) for i in range(29)]+[(i/28)**2*(3-2*i/28) for i in range(1,29)]
        for index,amount in enumerate(amounts):
            pose(amount)
            viewport.refresh()
            adsk.doEvents()
            options=adsk.core.SaveImageFileOptions.create(os.path.join(folder,'frame-%03d.png'%index))
            options.width=900
            options.height=1000
            options.isAntiAliased=True
            options.isBackgroundTransparent=True
            assert viewport.saveAsImageFileWithOptions(options)
        print('Captured 57 frames: glass hidden; M2 screws approach from below')
    finally:
        imported.close(False)
        original.activate()
```

Encode the captured frames from the repository root using Python with Pillow installed. The shared 192-colour palette avoids frame-to-frame colour flicker; dark compositing avoids GIF transparency/disposal artefacts. Frame 0 is also the static exploded PNG.

```python
from pathlib import Path
from PIL import Image

frames = []
for index in range(57):
    source = Image.open(f".cache/cad-assembly-v12/frame-{index:03d}.png").convert("RGBA")
    background = Image.new("RGBA", source.size, (8, 11, 14, 255))
    background.alpha_composite(source)
    frames.append(background.convert("RGB").resize((720, 800), Image.Resampling.LANCZOS))
frames[0].save("docs/images/waveshare-206-exploded.png", optimize=True)
sheet = Image.new("RGB", (1200, 804))
for index, frame in enumerate(frames):
    sheet.paste(frame.resize((120, 134)), ((index % 10) * 120, (index // 10) * 134))
palette = sheet.quantize(colors=192)
quantized = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
durations = [80] * 57
durations[0] = 1400
durations[28] = 1800
quantized[0].save(
    "docs/images/waveshare-206-assembly.gif",
    save_all=True, append_images=quantized[1:], duration=durations,
    loop=0, disposal=2, optimize=True,
)
with Image.open("docs/images/waveshare-206-assembly.gif") as image:
    assert image.n_frames == 57
    for index in range(image.n_frames):
        image.seek(index)
        image.load()
print("Validated 57-frame assembly GIF")
```

## Liner split verification

Before adding a new cut, inspect the final liner geometry: its existing cut is a 0.5 mm slit at local Y = -0.25 mm / +0.25 mm, on the lower side of the sleeve. The two planar slit faces span the full 14.3 mm axial width and bridge the inner/outer cylindrical surfaces, proving the cut goes through the 0.5 mm wall. Feature names include `Liner split gap` and `Liner split`, but validate the final BRep rather than relying on names or the unrelated `SplitGap` user parameter. Preserve this seam and show it in the close-up.

## Update documentation and verify

1. Update the source version/date in `cad/waveshare-206/README.md` using the actual export version. If exporting unsaved changes, explicitly say so instead of claiming the saved version alone.
2. Review changed dimensions, materials, fasteners, mounting arrangements and reference geometry. Update affected README tables and captions from current model evidence.
3. Reopen the exported F3D in a temporary document. Compare occurrence, parameter and timeline counts with the source, then close only the temporary document.
4. Inspect all still images plus exploded and assembled animation frames. Confirm glass is absent and M2 screws lie below their final holes in the exploded pose. Check the GIF decodes all 57 frames.
5. Build the documentation and verify source/download byte equality. The builder copies STEP/F3D/F3Z files and source images, but not this maintainer file.
6. Keep `scripts/publish_pages.py`'s static-file allowlist aligned with published asset types. It must allow `.step`, `.f3d`, `.f3z` and `.gif`; do not bypass its path, symlink or size checks.

```powershell
.cache/web-test-venv/Scripts/python.exe scripts/build_docs.py --output .cache/pages/site --images .cache/pages/docs-images
.cache/web-test-venv/Scripts/python.exe tests/docs_browser_test.py --site .cache/pages/site --output .cache/cad-docs-checks --browser-executable 'C:/Program Files/Google/Chrome/Application/chrome.exe'
.cache/web-test-venv/Scripts/python.exe tests/preview_browser_test.py --site .cache/pages/site --browser-executable 'C:/Program Files/Google/Chrome/Application/chrome.exe'
python -m unittest discover -s tests -p publish_pages_tests.py
python -m ruff format --check scripts tests
git diff --check
```

Use the repository's current tooling paths if they change. Keep animation frames and validation screenshots under `.cache/`; commit only final CAD exports, final documentation images and the relevant notes.

