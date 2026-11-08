"""Run through Fusion's script tool; render copies, never edit the source design."""

import json
from datetime import date
from pathlib import Path

import adsk.core
import adsk.fusion

ROOT = Path(r"C:\Repos\wfb_disp")
ARCHIVE = ROOT / "cad/wheelie-fun-display-mounts.f3d"
MODELS = {
    "waveshare-206": {
        "groups": ("01_Waveshare_Landscape", "02_Waveshare_Portrait"),
        "casing": "01_Rear_body_PRINT",
        "grommet": "06_One_piece_grommet_TPU75A",
    },
    "guition-43": {
        "groups": ("03_Guition_Landscape",),
        "casing": "G01_Rear_housing_PRINT_PROTO",
        "grommet": "G03_Snap_grommet_TPU75A",
    },
}
COMMON = {
    "03_Upper_saddle_AL6061": "upper-saddle.step",
    "04_Lower_cap_AL6061": "lower-clamp-cap.step",
    "HW_split_rubber_liner_0p5": "rubber-liner.step",
}


def counts(design):
    return {
        "occurrences": design.rootComponent.allOccurrences.count,
        "components": design.allComponents.count,
        "parameters": design.userParameters.count,
        "timeline": design.timeline.count,
    }


def reference(name):
    return name.startswith(("REF_", "G02_REF", "G04_REF"))


def run(_context: str):
    app = adsk.core.Application.get()
    original = app.activeDocument
    source = adsk.fusion.Design.cast(app.activeProduct)
    assert source and original.name == "Wheelie Fun Display - Waveshare 2.06"
    modified = original.isModified
    manifest = {
        "document": original.name,
        "version": original.dataFile.versionNumber,
        "modified": modified,
        "date": date.today().isoformat(),
        "counts": counts(source),
        "parameters": {
            p.name: {"expression": p.expression, "internal_value": p.value}
            for p in source.userParameters
        },
        "parts": {},
    }
    assert not any(o.isReferencedComponent for o in source.rootComponent.allOccurrences)
    assert source.exportManager.execute(
        source.exportManager.createFusionArchiveExportOptions(str(ARCHIVE))
    )
    imported = app.importManager.importToNewDocument(
        app.importManager.createFusionArchiveImportOptions(str(ARCHIVE))
    )
    assert imported
    try:
        design = adsk.fusion.Design.cast(app.activeProduct)
        assert counts(design) == manifest["counts"]
        viewport = app.activeViewport
        root_occurrences = list(design.rootComponent.occurrences)
        occurrences = list(design.rootComponent.allOccurrences)

        def component(name):
            matches = [c for c in design.allComponents if c.name == name]
            assert len(matches) == 1, name
            return matches[0]

        def group(prefix):
            matches = [
                o for o in root_occurrences if o.component.name.startswith(prefix)
            ]
            assert len(matches) == 1, prefix
            return matches[0]

        def children(parent):
            return [
                o for o in occurrences if o.fullPathName.startswith(parent.name + "+")
            ]

        def show(groups, glass=True):
            for c in design.allComponents:
                for body in c.bRepBodies:
                    body.isLightBulbOn = True
            for occurrence in root_occurrences:
                occurrence.isLightBulbOn = occurrence.name in groups
            for occurrence in occurrences:
                name = occurrence.component.name
                occurrence.isLightBulbOn = (
                    occurrence.name in groups
                    if "+" not in occurrence.fullPathName
                    else not reference(name)
                    or (glass and name.startswith(("REF_Display", "G02_REF")))
                )

        def camera(direction, target, margin=1.2):
            c = viewport.camera
            c.cameraType = adsk.core.CameraTypes.OrthographicCameraType
            c.eye = adsk.core.Point3D.create(
                *(target[i] + direction[i] for i in range(3))
            )
            c.target = adsk.core.Point3D.create(*target)
            c.upVector = adsk.core.Vector3D.create(0, 0, 1)
            c.isSmoothTransition = False
            c.isFitView = True
            viewport.camera = c
            viewport.refresh()
            c = viewport.camera
            ok, width, height = c.getExtents()
            assert ok
            c.isFitView = False
            assert c.setExtents(width * margin, height * margin)
            viewport.camera = c

        def capture(path, width=2000, height=1400):
            viewport.refresh()
            adsk.doEvents()
            options = adsk.core.SaveImageFileOptions.create(str(path))
            options.width, options.height = width, height
            options.isAntiAliased = True
            options.isBackgroundTransparent = True
            assert viewport.saveAsImageFileWithOptions(options)

        def center(occurrence):
            box = occurrence.boundingBox
            return [
                (a + b) / 2
                for a, b in zip(box.minPoint.asArray(), box.maxPoint.asArray())
            ]

        def export_part(name, path):
            c = component(name)
            assert c.bRepBodies.count == 1 and c.bRepBodies.item(0).isSolid, name
            body = c.bRepBodies.item(0)
            # Copy only the native body. Exporting its owning component can also
            # export nested grommets and reference cable geometry.
            scratch = design.rootComponent.occurrences.addNewComponent(
                adsk.core.Matrix3D.create()
            )
            try:
                scratch.component.name = "STEP export only"
                copied = body.copyToComponent(scratch)
                assert copied and copied.isSolid
                assert abs(copied.volume - body.volume) < 1e-8
                assert scratch.component.bRepBodies.count == 1
                assert scratch.component.occurrences.count == 0
                assert design.exportManager.execute(
                    design.exportManager.createSTEPExportOptions(
                        str(path), scratch.component
                    )
                )
            finally:
                assert scratch.deleteMe()
            manifest["parts"][str(path.relative_to(ROOT))] = {
                "component": name,
                "material": body.material.name,
                "appearance": body.appearance.name,
                "volume_cm3": body.volume,
                "min_cm": body.boundingBox.minPoint.asArray(),
                "max_cm": body.boundingBox.maxPoint.asArray(),
            }

        for slug, model in MODELS.items():
            folder = ROOT / "cad" / slug
            folder.mkdir(parents=True, exist_ok=True)
            parts = dict(COMMON)
            parts[model["casing"]] = "rear-casing.step"
            parts[model["grommet"]] = "cable-grommet.step"
            for name, filename in parts.items():
                export_part(name, folder / filename)
            main = group(model["groups"][0])
            groups = {group(prefix).name for prefix in model["groups"]}
            origin = main.transform2.translation.asArray()
            members = children(main)

            def still(kind):
                show(groups if kind == "mounted" else {main.name})
                viewport.visualStyle = adsk.core.VisualStyles.ShadedVisualStyle
                target = [origin[0], origin[1], -0.5]
                if kind in ("mounted", "clamp"):
                    direction = (13, -14, 23) if kind == "mounted" else (10, -14, -14)
                else:
                    name = (
                        model["casing"]
                        if kind == "casing"
                        else model["grommet"]
                        if kind == "grommet"
                        else "HW_split_rubber_liner_0p5"
                    )
                    chosen = [o for o in members if o.component.name == name]
                    assert len(chosen) == 1
                    target = center(chosen[0])
                    for c in design.allComponents:
                        for body in c.bRepBodies:
                            body.isLightBulbOn = c.name == name
                    direction = {
                        "casing": (8, -11, 16.6),
                        "grommet": (2.6, -2.75, -6.9),
                        "liner": (7, -5, -10.05),
                    }[kind]
                    if kind != "casing":
                        viewport.visualStyle = (
                            adsk.core.VisualStyles.ShadedWithVisibleEdgesOnlyVisualStyle
                        )
                camera(direction, target)
                if kind == "mounted":
                    # Fit the display assemblies first, then show the long bar
                    # as cropped context without shrinking the product view.
                    for o in root_occurrences:
                        if o.component.name.startswith("REF_Handlebar"):
                            o.isLightBulbOn = True
                capture(ROOT / "docs/images" / f"{slug}-{kind}.png")

            for kind in ("mounted", "clamp", "casing", "liner", "grommet"):
                still(kind)
            show({main.name}, glass=False)
            moving = sorted(
                [
                    (o, o.transform2.copy())
                    for o in members
                    if o.component.bRepBodies.count and not reference(o.component.name)
                ],
                key=lambda item: item[0].fullPathName.count("+"),
            )

            def offset(occurrence, base):
                name = occurrence.component.name
                if name == model["casing"] or name.startswith("HW_M3_Voron"):
                    return (0, 0, 1.8)
                if name == model["grommet"]:
                    return (0, 0, 4.2)
                if name.startswith("04_Lower"):
                    return (0, 0, -2)
                if name.startswith("03_Upper"):
                    return (0, 0, 0)
                if name.startswith("HW_split"):
                    return (2.8, 0, 0)
                if name.startswith("HW_M4"):
                    return (0, 0, -3.2)
                if name.startswith("HW_M2"):
                    return (0, 0, -2.6)
                if name.startswith("HW_M3x6"):
                    # GUITION has four additional display screws at Z=11 mm.
                    return (0, 0, -2.6 if base.translation.z > 0.5 else -1.2)
                raise AssertionError("No explosion offset for " + name)

            def pose(amount):
                for occurrence, base in moving:
                    matrix = base.copy()
                    vector = matrix.translation
                    x, y, z = offset(occurrence, base)
                    vector.x += x * amount
                    vector.y += y * amount
                    vector.z += z * amount
                    matrix.translation = vector
                    occurrence.transform2 = matrix

            pose(1)
            viewport.visualStyle = adsk.core.VisualStyles.ShadedVisualStyle
            camera((11, -17, 9.5), (origin[0], origin[1], -0.5))
            frames = ROOT / ".cache" / ("cad-assembly-" + slug)
            frames.mkdir(parents=True, exist_ok=True)
            amounts = [1 - (i / 28) ** 2 * (3 - 2 * i / 28) for i in range(29)]
            amounts += [(i / 28) ** 2 * (3 - 2 * i / 28) for i in range(1, 29)]
            for index, amount in enumerate(amounts):
                pose(amount)
                capture(frames / f"frame-{index:03d}.png", 900, 1000)
            pose(0)
            viewport.refresh()
            adsk.doEvents()
            for occurrence, base in moving:
                assert all(
                    abs(a - b) < 1e-8
                    for a, b in zip(occurrence.transform2.asArray(), base.asArray())
                ), occurrence.fullPathName
            print(f"Exported {slug}: five STEP parts, five views, 57 animation frames")
        (ROOT / ".cache/cad-export-manifest.json").write_text(
            json.dumps(manifest, indent=2), encoding="utf-8"
        )
    finally:
        imported.close(False)
        original.activate()
    assert original.isModified == modified
    print(json.dumps({k: manifest[k] for k in ("document", "version", "counts")}))
