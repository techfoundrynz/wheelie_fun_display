# Regenerating Waveshare 2.06 CAD assets

Agent/maintainer instructions only; do not register or link this file in Pages.

Follow the [shared CAD regeneration instructions](../REGENERATING.md). Use [export_cad.py](../../scripts/export_cad.py) inside Fusion and [encode_cad.py](../../scripts/encode_cad.py) from the repository.

The source now groups the Waveshare occurrences under `01_Waveshare_Landscape` and `02_Waveshare_Portrait`. Do not use the old recipe that assumed every part was a root occurrence or used world X<5 to isolate one assembly. The GUITION group shares the same saddle, lower cap and liner.

The casing `01_Rear_body_PRINT` contains a nested `06_One_piece_grommet_TPU75A` and reference cable. Export only the casing's own body into its STEP, and supply the grommet separately. Use current source materials and appearances with no gray override. Keep the larger grommet flange inside the casing.

For the exploded view and GIF, hide the entire reference display including glass, the cable, handlebar and other assemblies. M2 display screws approach from below with negative world-Z offsets. Use root-context occurrence proxies and world transforms: the casing moves +1.8 cm, and its nested grommet moves +4.2 cm total. Apply parent transforms before child transforms, always from saved original matrices, then verify the assembled matrices are restored exactly.

Preserve the liner's 0.5 mm longitudinal slit through its full 0.5 mm wall and 14.3 mm width. This split liner differs from the unsplit cable grommet.

Outputs are five one-solid STEP files here, six final PNGs and a 57-frame GIF under `docs/images/` with the `waveshare-206-` prefix. The editable source now lives in shared `cad/wheelie-fun-display-mounts.f3d`; do not recreate the old Waveshare-only archive filename.
