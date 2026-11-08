# Regenerating GUITION case and mount assets

Agent/maintainer instructions only; do not register or link this file in Pages.

Follow the [shared CAD regeneration instructions](../REGENERATING.md). The executable recipes are [export_cad.py](../../scripts/export_cad.py), run inside Fusion, and [encode_cad.py](../../scripts/encode_cad.py), run from the repository.

The current source group begins `03_Guition_Landscape`. Its own parts are `G01_Rear_housing_PRINT_PROTO` and `G03_Snap_grommet_TPU75A`; saddle, cap and liner are shared with Waveshare. Keep the source material/appearance. Hide `G02_REF_OEM_display_DEPTH_UNVERIFIED`, its glass, `G04_REF_6mm_cable_jacket` and the handlebar in the exploded view and animation.

The eight M3 screws are two sets: four saddle screws at source world Z=0 and four display screws translated to Z=11 mm. Both sets approach from below, with negative world-Z explosion offsets. Do not accidentally move the display screws upward or export them into the casing STEP.

Keep the guide's dimensions and hardware count synchronized with the actual source. In particular, the OEM display's 5 mm depth and reference display screws are unverified CAD assumptions. Do not imply that the solid model is a vendor-qualified electronics envelope or label the supported firmware target experimental.

Output five one-solid STEP files under this folder, six final PNGs and one 57-frame GIF with the `guition-43-` prefix under `docs/images/`. The complete native design is the shared `cad/wheelie-fun-display-mounts.f3d`, not another duplicate archive in this folder.
