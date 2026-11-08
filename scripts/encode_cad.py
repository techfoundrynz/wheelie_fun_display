"""Encode the real Fusion frames and normalize manufacturing STEP exports."""

import re
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def encode(slug):
    frames = []
    for index in range(57):
        source = Image.open(
            ROOT / ".cache" / ("cad-assembly-" + slug) / f"frame-{index:03d}.png"
        ).convert("RGBA")
        background = Image.new("RGBA", source.size, (8, 11, 14, 255))
        background.alpha_composite(source)
        frames.append(
            background.convert("RGB").resize((720, 800), Image.Resampling.LANCZOS)
        )
    images = ROOT / "docs/images"
    frames[0].save(images / f"{slug}-exploded.png", optimize=True)
    frames[28].save(ROOT / ".cache" / f"{slug}-assembled.png")
    sheet = Image.new("RGB", (1200, 804))
    for index, frame in enumerate(frames):
        sheet.paste(frame.resize((120, 134)), ((index % 10) * 120, (index // 10) * 134))
    palette = sheet.quantize(colors=192)
    quantized = [
        frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames
    ]
    durations = [80] * 57
    durations[0], durations[28] = 1400, 1800
    gif = images / f"{slug}-assembly.gif"
    quantized[0].save(
        gif,
        save_all=True,
        append_images=quantized[1:],
        duration=durations,
        loop=0,
        disposal=2,
        optimize=True,
    )
    with Image.open(gif) as image:
        assert image.n_frames == 57
        for index in range(image.n_frames):
            image.seek(index)
            image.load()
    for path in (ROOT / "cad" / slug).glob("*.step"):
        source = path.read_text(encoding="utf-8")
        text = "\n".join(line.rstrip() for line in source.splitlines()) + "\n"
        assert text.startswith("ISO-10303-21;")
        assert text.rstrip().endswith("END-ISO-10303-21;")
        assert text.count("MANIFOLD_SOLID_BREP(") == 1, path
        assert re.search(
            r"LENGTH_UNIT\(\)\s*NAMED_UNIT\(\*\)\s*SI_UNIT\(\.(?:CENTI|MILLI)\.,\.METRE\.\)",
            text,
        ), path
        path.write_text(text, encoding="utf-8", newline="\n")
    print(f"Validated {slug}: five one-solid STEP parts and 57-frame GIF")


if __name__ == "__main__":
    for model in ("waveshare-206", "guition-43"):
        encode(model)
