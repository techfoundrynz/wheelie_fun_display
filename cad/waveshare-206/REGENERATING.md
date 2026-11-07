# Regenerating Waveshare 2.06 CAD assets

Agent/maintainer instructions. This file is intentionally not registered in `scripts/build_docs.py`, linked from the public guides, or copied into the Pages artifact. Public assembly instructions belong in `README.md` beside this file.

## Scope and source inspection

Use the connected Fusion tools to inspect the active design and execute Fusion Python API scripts. Query `apiDocumentation` for unfamiliar API members before use. Scripts must define `run(_context: str)`; use `readOnly: true` only for inspection. Use real Fusion renders, not generated or painted geometry.

The expected source document is **Wheelie Fun Display - Waveshare 2.06**. Inspect its actual version, saved/modified state, component tree, parameters and dimensions every time. Export the current design without saving the user's cloud document. The last verified export was **saved version 15**, with **39 occurrences (including nested occurrences), 13 components, 128 user parameters and 95 timeline items**, on **8 November 2026**. These are verification records, not constants to impose on later versions.

The source has two mounting orientations: one at X=0 and one translated +9 cm. All components are local, so the native archive is F3D. Use a genuine F3Z package only if external references are introduced; never rename an F3D archive to F3Z.

## Exported parts

| Component | Destination under `cad/waveshare-206/` |
|---|---|
| `01_Rear_body_PRINT` | `rear-casing.step` |
| `03_Upper_saddle_AL6061` | `upper-saddle.step` |
| `04_Lower_cap_AL6061` | `lower-clamp-cap.step` |
| `HW_split_rubber_liner_0p5` | `rubber-liner.step` |
| `06_One_piece_grommet_TPU75A` | `cable-grommet.step` |

**Nested components matter:** the casing contains both the grommet and `REF_Cable_jacket_D6_NOT_WIRE_DETAIL`. Exporting the casing component directly also includes its children. Export only its own solid by copying that body into an otherwise empty temporary component. Keep the grommet as a separate one-solid STEP and exclude the cable reference from all manufacturing STEP exports. The complete F3D must retain both nested components.

Export the native archive **before** changing visibility, appearance or transforms. Reopen it in a temporary document and compare occurrence, component, parameter and timeline counts with the source. Make all render/export preparation changes in this temporary document; close only it with `close(False)` in `finally`, then reactivate the original source. Do not hide, move or recolor the user's source design.

## Cable grommet verification

Check the current model parameters and final solid rather than relying only on component names. Version 14 uses:

| Feature | Nominal value |
|---|---|
| Material designation | TPU, Shore 75A |
| Cable jacket OD / bore | 6 / 5.8 mm |
| Casing hole / grommet barrel OD | 7.8 / 7.9 mm |
| Panel grip length | 3.8 mm |
| Inner / outer flange OD | 9.4 / 8.6 mm |
| Inner / outer flange thickness | 0.8 / 0.6 mm |

The larger flange stays **inside** the casing. This is a **one-piece snap-in grommet with no installation split**; it needs to be threaded onto the cable before an oversized connector is terminated. Do not describe it as a threaded compression gland, a split grommet, or an independently verified waterproof/strain-relief system. Keep the reference 6 mm cable jacket hidden in documentation renders. It represents cable diameter only, not conductor routing or connector geometry.

Use the grommet's current source material and appearance in every render. Do not apply the former temporary gray appearance override, including in the exploded view, close-up or animation. Inspect body, face and occurrence appearance overrides when checking a source change. Version 15 uses the material named `Rubber` and the body appearance named `Rubber - Soft`; the geometry and `TPU75A` component designation remain unchanged. The TPU 75A printing recommendation in the public guide comes from the design's component designation and parameters, not a qualification of Fusion's generic physical material.

## Views and animation requirements

Capture mounted, clamp, casing, liner and grommet PNGs at **2000 × 1400** with anti-aliasing and transparent backgrounds. Use orthographic cameras, fit once and enlarge extents by 20 percent. Use shaded style; visible edges help the isolated liner and grommet close-ups.

| Image suffix | Visible content | Eye / target / up (Fusion world coordinates, cm) |
|---|---|---|
| `mounted` | Both orientations and handlebar | (13,-14,22) / (4.5,0,-1) / (0,1,0) |
| `clamp` | First assembly, no handlebar | (10,-14,-15) / (0,0,-1.2) / (0,0,1) |
| `casing` | First casing's own body; hide nested grommet and cable | (8,-11,17) / (0,0,0.4) / (0,1,0) |
| `liner` | First split liner only | (7,-5,-12) / (0,0,-1.95) / (0,0,1) |
| `grommet` | First nested grommet only; keep its parent occurrence visible but hide the casing body | (4,-1,-7) / (1.4,1.75,-0.1) / (0,0,1) |

For the exploded still and GIF:

- Hide the entire `REF_Display_outline_VERIFY_NOT_FABRICATE` occurrence, including its frame and cover glass. Hide the handlebar, cable reference and second assembly.
- M2 display screws approach **upward from underneath**: explode with **negative world Z** offsets, keeping their original orientation.
- Separate the grommet along its cable axis above the casing, so its larger flange comes to rest inside. Get the nested occurrence **proxy from `rootComponent.allOccurrences`**. Setting `transform2` on the native nested occurrence fails in this parametric assembly. Proxy transforms use world coordinates: casing +1.8 cm and grommet an additional +2.4 cm mean a total grommet offset of **+4.2 cm**, not +2.4 cm.
- Apply offsets from saved original transforms, never cumulatively. Restore the exact original assembled transforms. Use one fixed camera fitted to the full explosion; do not zoom per frame.
- Capture 57 frames at 900 × 1000: 29 exploded-to-assembled poses and 28 returning poses, with smoothstep easing. This is a fit overview, not a prescribed fastening sequence.

## Fusion regeneration script

Run from the repository root. Update `ROOT` if needed. This script exports the native archive and five one-solid STEP files, captures all five still views and captures animation frames under `.cache/cad-assembly-waveshare-206/`. It verifies the reopened archive before temporarily modifying it and returns the user's original document unchanged.

```python
import adsk.core, adsk.fusion, json, os

ROOT = r'C:\Repos\wfb_disp'
PARTS = {
    '01_Rear_body_PRINT': 'rear-casing.step',
    '03_Upper_saddle_AL6061': 'upper-saddle.step',
    '04_Lower_cap_AL6061': 'lower-clamp-cap.step',
    'HW_split_rubber_liner_0p5': 'rubber-liner.step',
    '06_One_piece_grommet_TPU75A': 'cable-grommet.step',
}

def run(_context: str):
    app = adsk.core.Application.get()
    original = app.activeDocument
    source = adsk.fusion.Design.cast(app.activeProduct)
    assert original.name == 'Wheelie Fun Display - Waveshare 2.06'
    counts = (source.rootComponent.allOccurrences.count, source.userParameters.count, source.timeline.count, source.allComponents.count)
    modified = original.isModified
    archive = os.path.join(ROOT, 'cad', 'waveshare-206', 'wheelie-fun-display-waveshare-206.f3d')
    assert source.exportManager.execute(source.exportManager.createFusionArchiveExportOptions(archive))
    imported = app.importManager.importToNewDocument(app.importManager.createFusionArchiveImportOptions(archive))
    assert imported
    try:
        d = adsk.fusion.Design.cast(app.activeProduct)
        assert (d.rootComponent.allOccurrences.count, d.userParameters.count, d.timeline.count, d.allComponents.count) == counts
        components = {}
        for name, filename in PARTS.items():
            matches = [c for c in d.allComponents if c.name == name]
            assert len(matches) == 1 and matches[0].bRepBodies.count == 1
            component = matches[0]
            assert component.bRepBodies.item(0).isSolid
            components[name] = component
            if name != '01_Rear_body_PRINT':
                assert component.occurrences.count == 0
                assert d.exportManager.execute(d.exportManager.createSTEPExportOptions(os.path.join(ROOT, 'cad', 'waveshare-206', filename), component))
        viewport = app.activeViewport
        def configure(kind):
            for c in d.allComponents:
                for b in c.bRepBodies:
                    b.isLightBulbOn = True
            for o in d.rootComponent.occurrences:
                name = o.component.name
                if kind == 'mounted': visible = True
                elif kind == 'clamp': visible = o.transform2.translation.x < 5 and not name.startswith('REF_Handlebar')
                elif kind in ('casing', 'grommet'): visible = o.name == '01_Rear_body_PRINT:1'
                else: visible = o.name == 'HW_split_rubber_liner_0p5:1'
                o.isLightBulbOn = visible
            for o in d.rootComponent.allOccurrences:
                if o.component.name.startswith('REF_Cable'): o.isLightBulbOn = False
                if o.component.name.startswith('06_One_piece'): o.isLightBulbOn = kind != 'casing'
            if kind == 'grommet':
                for b in components['01_Rear_body_PRINT'].bRepBodies: b.isLightBulbOn = False
            viewport.visualStyle = adsk.core.VisualStyles.ShadedWithVisibleEdgesOnlyVisualStyle if kind in ('liner','grommet') else adsk.core.VisualStyles.ShadedVisualStyle
        def camera(eye, target, up, margin=1.2):
            c = viewport.camera
            c.cameraType = adsk.core.CameraTypes.OrthographicCameraType
            c.eye = adsk.core.Point3D.create(*eye)
            c.target = adsk.core.Point3D.create(*target)
            c.upVector = adsk.core.Vector3D.create(*up)
            c.isSmoothTransition = False
            c.isFitView = True
            viewport.camera = c
            viewport.refresh()
            c = viewport.camera
            success, width, height = c.getExtents()
            assert success
            c.isFitView = False
            assert c.setExtents(width*margin, height*margin)
            viewport.camera = c
        def save(path, width=2000, height=1400):
            viewport.refresh()
            adsk.doEvents()
            options = adsk.core.SaveImageFileOptions.create(path)
            options.width = width
            options.height = height
            options.isAntiAliased = True
            options.isBackgroundTransparent = True
            assert viewport.saveAsImageFileWithOptions(options)
        views = {
            'mounted': ((13,-14,22),(4.5,0,-1),(0,1,0)),
            'clamp': ((10,-14,-15),(0,0,-1.2),(0,0,1)),
            'casing': ((8,-11,17),(0,0,0.4),(0,1,0)),
            'liner': ((7,-5,-12),(0,0,-1.95),(0,0,1)),
            'grommet': ((4,-1,-7),(1.4,1.75,-0.1),(0,0,1)),
        }
        for name, view in views.items():
            configure(name)
            camera(*view)
            save(os.path.join(ROOT, 'docs', 'images', 'waveshare-206-'+name+'.png'))
        # Restore body visibility, then isolate the first assembly for the animation.
        configure('clamp')
        parts = []
        for o in d.rootComponent.occurrences:
            visible = o.transform2.translation.x < 5 and not o.component.name.startswith(('REF_Handlebar','REF_Display'))
            o.isLightBulbOn = visible
            if visible: parts.append((o,o.transform2.copy()))
        rear = components['01_Rear_body_PRINT']
        nested = [o for o in d.rootComponent.allOccurrences if o.fullPathName.startswith('01_Rear_body_PRINT:1+') and o.component.name.startswith('06_One_piece_grommet')]
        assert len(nested) == 1
        grommet = nested[0]
        grommet.isLightBulbOn = True
        grommet_base = grommet.transform2.copy()
        offsets = {'01_Rear':(0,0,1.8),'04_Lower':(0,0,-2),'HW_split':(2.8,0,0),'HW_M3_Voron':(0,0,1.8),'HW_M3x6':(0,0,-1.2),'HW_M4':(0,0,-3.2),'HW_M2':(0,0,-2.6)}
        def pose(amount):
            for o,base in parts:
                transform = base.copy()
                v = transform.translation
                offset = next((offset for prefix,offset in offsets.items() if o.component.name.startswith(prefix)),(0,0,0))
                v.x += offset[0]*amount
                v.y += offset[1]*amount
                v.z += offset[2]*amount
                transform.translation = v
                o.transform2 = transform
            transform = grommet_base.copy()
            v = transform.translation
            v.z += 4.2*amount
            transform.translation = v
            grommet.transform2 = transform
        viewport.visualStyle = adsk.core.VisualStyles.ShadedVisualStyle
        pose(1)
        camera((11,-17,9),(0,0,-0.5),(0,0,1))
        folder = os.path.join(ROOT, '.cache', 'cad-assembly-waveshare-206')
        os.makedirs(folder, exist_ok=True)
        amounts = [1-(i/28)**2*(3-2*i/28) for i in range(29)]+[(i/28)**2*(3-2*i/28) for i in range(1,29)]
        for index,amount in enumerate(amounts):
            pose(amount)
            save(os.path.join(folder,'frame-%03d.png'%index),900,1000)
        pose(0)
        for o,base in parts:
            assert all(abs(a-b)<1e-8 for a,b in zip(o.transform2.asArray(),base.asArray()))
        assert all(abs(a-b)<1e-8 for a,b in zip(grommet.transform2.asArray(),grommet_base.asArray()))
        # Export only the casing's own body: its nested grommet/reference cable
        # must not accidentally become manufacturing solids in rear-casing.step.
        export_occ = d.rootComponent.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        export_occ.component.name = 'Casing STEP export only'
        assert rear.bRepBodies.item(0).copyToComponent(export_occ)
        assert export_occ.component.bRepBodies.count == 1
        assert export_occ.component.occurrences.count == 0
        assert d.exportManager.execute(d.exportManager.createSTEPExportOptions(os.path.join(ROOT,'cad','waveshare-206','rear-casing.step'),export_occ.component))
        print(json.dumps({'archive_counts':counts,'version':original.dataFile.versionNumber,'source_modified':modified,'steps':list(PARTS.values()),'stills':list(views),'frames':len(amounts)}))
    finally:
        imported.close(False)
        original.activate()
    assert original.isModified == modified
```

## Encode and validate assets

Run with Python and Pillow installed. The shared palette prevents color flicker; dark compositing avoids GIF transparency/disposal artifacts. Frame 0 is the static exploded PNG, and frame 28 is the assembled QA image. Scratch images stay under `.cache/`.

```python
from pathlib import Path
from PIL import Image
folder=Path('.cache/cad-assembly-waveshare-206');folder.mkdir(exist_ok=True)
frames=[]
for index in range(57):
    source=Image.open(folder/f'frame-{index:03d}.png').convert('RGBA')
    background=Image.new('RGBA',source.size,(8,11,14,255));background.alpha_composite(source)
    frames.append(background.convert('RGB').resize((720,800),Image.Resampling.LANCZOS))
frames[0].save('docs/images/waveshare-206-exploded.png',optimize=True)
frames[28].save('.cache/cad-waveshare-206-assembled.png')
sheet=Image.new('RGB',(1200,804))
for index,frame in enumerate(frames):sheet.paste(frame.resize((120,134)),((index%10)*120,(index//10)*134))
palette=sheet.quantize(colors=192)
quantized=[frame.quantize(palette=palette,dither=Image.Dither.NONE) for frame in frames]
durations=[80]*57;durations[0]=1400;durations[28]=1800
quantized[0].save('docs/images/waveshare-206-assembly.gif',save_all=True,append_images=quantized[1:],duration=durations,loop=0,disposal=2,optimize=True)
with Image.open('docs/images/waveshare-206-assembly.gif') as image:
    assert image.n_frames==57
    for i in range(image.n_frames):image.seek(i);image.load()
for path in Path('cad/waveshare-206').glob('*.step'):
    text=path.read_text(encoding='utf-8')
    text='\n'.join(line.rstrip() for line in text.splitlines())+'\n'
    path.write_text(text,encoding='utf-8',newline='\n')
    assert text.startswith('ISO-10303-21;') and text.rstrip().endswith('END-ISO-10303-21;')
    assert text.count('MANIFOLD_SOLID_BREP(')==1,(path,text.count('MANIFOLD_SOLID_BREP('))
    print(path.name,'one solid',path.stat().st_size)
print('Validated 57-frame GIF')
```

## Liner split verification

Preserve the liner's existing 0.5 mm slit at local Y = -0.25 / +0.25 mm on the lower side of the sleeve. Verify the two planar slit faces span the full 14.3 mm axial width and connect the inner/outer cylindrical surfaces through the 0.5 mm wall. Do not infer the cut from the unrelated `SplitGap` parameter. Show the seam in the liner close-up. The split liner and unsplit cable grommet are different parts.

## Documentation and acceptance checks

1. Update `cad/waveshare-206/README.md` with actual source version/date/modification state, dimensions, material designations, hardware and installation steps. Include the cable grommet STEP, close-up and installation sequence. Keep portrait/landscape mounting explained.
2. Update the case/mount section of `docs/hardware-waveshare-206.md` and the CAD index when the downloadable part set changes.
3. Inspect all stills, the exploded pose and the fully assembled pose. Check that the grommet is visible and seats inside the casing, glass/reference cable are absent in the animation, M2 screws start below their holes, and nothing is clipped. Decode all 57 GIF frames.
4. Require one `MANIFOLD_SOLID_BREP(` and ISO-10303-21 start/end markers in every STEP. Normalize STEP text to LF; retain native archives as binary. Check F3D archive integrity with a Zstandard-capable reader: Fusion version 14 exports ZIP compression method 93, which Python 3.13 `zipfile` and older 7-Zip releases cannot decode. An unsupported-method error from those tools does not mean the archive is corrupt. The Windows `tar` in this environment can decode all entries; run `tar -xOf cad/waveshare-206/wheelie-fun-display-waveshare-206.f3d > $null` and require `$LASTEXITCODE` to be zero, in addition to reopening the F3D in Fusion.
5. Build the docs, verify download/image bytes equal the source, and check links and desktop/mobile rendering. Confirm this maintainer file is not shipped or linked in Pages. The builder copies `.step`, `.f3d`, `.f3z` and source images; keep `scripts/publish_pages.py`'s allowlist aligned without bypassing its path, symlink or size checks.

```powershell
.cache/web-test-venv/Scripts/python.exe scripts/build_docs.py --output .cache/pages/site --images .cache/pages/docs-images
.cache/web-test-venv/Scripts/python.exe tests/docs_browser_test.py --site .cache/pages/site --output .cache/cad-docs-checks --browser-executable 'C:/Program Files/Google/Chrome/Application/chrome.exe'
.cache/web-test-venv/Scripts/python.exe tests/preview_browser_test.py --site .cache/pages/site --browser-executable 'C:/Program Files/Google/Chrome/Application/chrome.exe'
python -m unittest discover -s tests -p publish_pages_tests.py
python -m ruff format --check scripts tests
git diff --check
```

Use current repository tooling paths if they change. Commit only final CAD exports, documentation images and relevant notes; keep animation frames and QA captures under `.cache/`.
