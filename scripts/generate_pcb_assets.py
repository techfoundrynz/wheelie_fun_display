"""Regenerate GUITION CAN PCB illustrations from its EasyEDA Pro archive.

See pcb/REGENERATING.md. The isometric illustration uses simplified packages,
not the external vendor meshes referenced by the archive.
"""

from __future__ import annotations

import argparse
import hashlib
import html
import io
import json
import math
from pathlib import Path
import zipfile

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
MIL = 0.0254


def read_project(source):
    sections = []
    with zipfile.ZipFile(source) as archive:
        bad = archive.testzip()
        if bad:
            raise ValueError(f"Archive CRC failed: {bad}")
        drawings = [name for name in archive.namelist() if name.endswith(".epru")]
        if len(drawings) != 1:
            raise ValueError("Expected one EasyEDA drawing database")
        for line in archive.read(drawings[0]).decode("utf-8").splitlines():
            header, body = line.split("||", 1)
            header = json.loads(header)
            body = json.loads(body.rstrip("|")) if body.rstrip("|") else None
            if header["type"] == "DOCHEAD":
                sections.append({"head": body, "records": []})
            else:
                # The database is an edit log. Empty bodies delete a record.
                rows = sections[-1]["records"]
                rows[:] = [
                    r
                    for r in rows
                    if (r[0]["type"], r[0]["id"]) != (header["type"], header["id"])
                ]
                if body is not None:
                    rows.append((header, body))
        previews = {
            name: archive.read(name)
            for name in archive.namelist()
            if name.startswith("IMAGE/") and name.endswith(".webp")
        }
    return sections, previews


def records(section, kind):
    return [(head, body) for head, body in section["records"] if head["type"] == kind]


def polygon_path(path):
    """Read this project's straight-edged package contours; reject new shapes."""
    values = [
        value for value in path if value == "L" or isinstance(value, (int, float))
    ]
    if len(values) != len(path):
        raise ValueError("Unsupported package contour command")
    values = [value for value in values if value != "L"]
    return list(zip(values[::2], values[1::2]))


def placed(comp, x, y):
    angle = math.radians(comp["angle"])
    if comp["layerId"] == 2:
        y = -y
    return (
        (comp["x"] + x * math.cos(angle) - y * math.sin(angle)) * MIL,
        (comp["y"] + x * math.sin(angle) + y * math.cos(angle)) * MIL,
    )


def layout_image(sections, bottom):
    """Side-specific assembly layouts; no fabricated copper-pour reconstruction."""
    pcb = next(s for s in sections if s["head"]["docType"] == "PCB")
    footprints = {
        s["head"]["uuid"]: s for s in sections if s["head"]["docType"] == "FOOTPRINT"
    }
    attrs = {}
    for _, body in records(pcb, "ATTR"):
        attrs.setdefault(body["parentId"], {})[body["key"]] = body["value"]
    image = Image.new("RGB", (1800, 1140), "#101820")
    draw = ImageDraw.Draw(image)

    def font(size):
        for name in ("C:/Windows/Fonts/arial.ttf", "DejaVuSans.ttf"):
            try:
                return ImageFont.truetype(name, size)
            except OSError:
                pass
        return ImageFont.load_default(size=size)

    def point(x, y):
        return (165 + (35 - x if bottom else x) * 42, 180 - y * 42)

    def poly(points, fill, outline=None, width=1):
        draw.polygon(
            [point(x, y) for x, y in points], fill=fill, outline=outline, width=width
        )

    title = "UNDERSIDE · viewed from below" if bottom else "TOP · component side"
    draw.text(
        (70, 45), f"GUITION CAN breakout | {title}", font=font(38), fill="#edf5f8"
    )
    draw.text(
        (70, 100),
        "35 × 18 mm · source-derived PCB assembly layout",
        font=font(27),
        fill="#aabdc8",
    )
    draw.rounded_rectangle(
        (165, 180, 1635, 936), radius=42, fill="#1d5947", outline="#99ba9b", width=3
    )
    for _, trace in records(pcb, "LINE"):
        if trace["layerId"] == (2 if bottom else 1):
            draw.line(
                [
                    point(trace[f"{end}X"] * MIL, trace[f"{end}Y"] * MIL)
                    for end in ("start", "end")
                ],
                fill="#6bc5e8" if bottom else "#6fd8b0",
                width=max(2, round(trace["width"] * MIL * 42)),
            )
    for _, via in records(pcb, "VIA"):
        x, y = point(via["centerX"] * MIL, via["centerY"] * MIL)
        radius = via["viaDiameter"] * MIL * 21
        hole = via["holeDiameter"] * MIL * 21
        draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill="#d8b85c")
        draw.ellipse((x - hole, y - hole, x + hole, y + hole), fill="#101820")
    labels = []
    for header, comp in records(pcb, "COMPONENT"):
        attr = attrs[header["id"]]
        footprint = footprints[attr["Footprint"]]
        visible = comp["layerId"] == (2 if bottom else 1)
        for _, pad in records(footprint, "PAD"):
            if not visible and pad["layerId"] != 12:
                continue
            x, y = pad["centerX"], pad["centerY"]
            w, h = pad["defaultPad"]["width"] / 2, pad["defaultPad"]["height"] / 2
            if pad["defaultPad"]["padType"] == "ELLIPSE":
                corners = [
                    (
                        x + w * math.cos(i * math.tau / 32),
                        y + h * math.sin(i * math.tau / 32),
                    )
                    for i in range(32)
                ]
            else:
                corners = [
                    (x + dx, y + dy) for dx, dy in ((-w, -h), (w, -h), (w, h), (-w, h))
                ]
            poly([placed(comp, px, py) for px, py in corners], "#d8b85c")
            hole = pad.get("hole")
            if hole and hole["width"] > 0:
                poly(
                    [
                        placed(
                            comp,
                            x + hole["width"] / 2 * math.cos(i * math.tau / 32),
                            y + hole["height"] / 2 * math.sin(i * math.tau / 32),
                        )
                        for i in range(32)
                    ],
                    "#101820",
                )
        if visible:
            contours = [
                b["path"] for _, b in records(footprint, "POLY") if b["layerId"] == 48
            ]
            if not contours:
                contours = [
                    b["path"][0]
                    for _, b in records(footprint, "FILL")
                    if b["layerId"] == 48
                ]
            for contour in contours:
                poly(
                    [placed(comp, x, y) for x, y in polygon_path(contour)],
                    None,
                    "#edf5f8",
                    3,
                )
            x, y = point(comp["x"] * MIL, comp["y"] * MIL)
            labels.append((x, y, attr["Designator"]))
    for x, y, ref in labels:
        # Label box stays centred over the source placement, with contrasting fill.
        box = draw.textbbox((0, 0), ref, font=font(26))
        width = box[2] - box[0]
        draw.rounded_rectangle(
            (x - width / 2 - 8, y - 22, x + width / 2 + 8, y + 22),
            radius=5,
            fill="#101820",
        )
        draw.text((x, y), ref, anchor="mm", font=font(26), fill="#edf5f8")
    draw.text(
        (70, 990),
        "Component outlines, pads, vias and side-specific tracks from the supplied project. Copper pours omitted.",
        font=font(27),
        fill="#edf5f8",
    )
    draw.text(
        (70, 1035),
        "The underside is mirrored for physical viewing. Use the native project for fabrication and connector pin numbering.",
        font=font(26),
        fill="#aabdc8",
    )
    draw.text(
        (70, 1080),
        "Assembly reference illustration · not a Gerber, solder-mask or fabrication plot",
        font=font(25),
        fill="#aabdc8",
    )
    return image


def illustration(sections, digest):
    pcb = next(s for s in sections if s["head"]["docType"] == "PCB")
    footprints = {
        s["head"]["uuid"]: s for s in sections if s["head"]["docType"] == "FOOTPRINT"
    }
    attrs = {}
    for _, body in records(pcb, "ATTR"):
        attrs.setdefault(body["parentId"], {})[body["key"]] = body["value"]
    components = [(body, attrs[head["id"]]) for head, body in records(pcb, "COMPONENT")]
    expected = {"U1", "U2", "CN1", "R1", "C1", "C2"}
    if {a["Designator"] for _, a in components} != expected:
        raise ValueError("Component set changed; review labels and package heights")
    outline = [
        b for _, b in records(pcb, "POLY") if b.get("polyType") == "BOARD_OUTLINE"
    ]
    if len(outline) != 1 or outline[0]["path"] != [
        "R",
        0,
        0,
        1377.9528,
        708.6614,
        0,
        39.3701,
    ]:
        raise ValueError("Board outline changed; update geometry and guide dimensions")
    svg = [
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1400 940" role="img" aria-labelledby="title description">',
        '<title id="title">GUITION CAN breakout: labelled top and underside</title>',
        '<desc id="description">Isometric layout illustration using source placements and footprints, with simplified component bodies. Not a native 3D export or clearance model.</desc>',
        '<rect width="1400" height="940" rx="24" fill="#101820"/>',
        '<g font-family="Arial, sans-serif" fill="#edf5f8">',
        '<text x="54" y="65" font-size="32" font-weight="700">GUITION CAN breakout</text>',
        '<text x="54" y="98" font-size="19" fill="#aabdc8">35 × 18 mm · source-derived isometric layout illustration</text>',
    ]

    def text(x, y, content, size=20, color="#edf5f8"):
        svg.append(
            f'<text x="{x}" y="{y}" font-size="{size}" fill="{color}">{html.escape(content)}</text>'
        )

    def view(cx, cy, bottom):
        # A true orthographic projection; underside view reverses the surface normal.
        normal = (0.35, -0.65, -1 if bottom else 1)
        norm = math.sqrt(sum(v * v for v in normal))
        normal = tuple(v / norm for v in normal)
        right = (0.880471, 0.4741, 0)
        up = (
            normal[1] * right[2] - normal[2] * right[1],
            normal[2] * right[0] - normal[0] * right[2],
            normal[0] * right[1] - normal[1] * right[0],
        )

        def point(p):
            p = (p[0] - 17.5, p[1] + 9, p[2])
            return (
                cx + 15 * sum(a * b for a, b in zip(p, right)),
                cy - 15 * sum(a * b for a, b in zip(p, up)),
            )

        def poly(points, fill, stroke="#133c32"):
            xy = " ".join(f"{x:.2f},{y:.2f}" for x, y in map(point, points))
            svg.append(
                f'<polygon points="{xy}" fill="{fill}" stroke="{stroke}" stroke-width="1"/>'
            )

        def solid(shape, z0, z1, color):
            faces = []
            for a, b in zip(shape, shape[1:] + shape[:1]):
                face = [
                    (a[0], a[1], z0),
                    (b[0], b[1], z0),
                    (b[0], b[1], z1),
                    (a[0], a[1], z1),
                ]
                depth = sum(sum(v * n for v, n in zip(p, normal)) for p in face)
                faces.append((depth, face))
            for _, face in sorted(faces):
                poly(face, color[1])
            poly([(x, y, z1) for x, y in shape], color[0])

        # Rounded board outline, radius 1 mm, in the source's top-side coordinates.
        shape = []
        for x, y, start in ((34, -1, 0), (1, -1, 90), (1, -17, 180), (34, -17, 270)):
            for step in range(9):
                angle = math.radians(start + step * 90 / 8)
                shape.append((x + math.cos(angle), y + math.sin(angle)))
        z = 0 if bottom else 1.6
        solid(shape, 1.6 if bottom else 0, z, ("#236e54", "#b39b67"))
        for _, trace in records(pcb, "LINE"):
            if trace["layerId"] != (2 if bottom else 1):
                continue
            a, b = [
                point((trace[f"{end}X"] * MIL, trace[f"{end}Y"] * MIL, z))
                for end in ("start", "end")
            ]
            svg.append(
                f'<path d="M{a[0]:.2f},{a[1]:.2f} L{b[0]:.2f},{b[1]:.2f}" stroke="#429b74" stroke-width="{max(1, trace["width"] * MIL * 15):.2f}" fill="none"/>'
            )
        bodies = []
        anchors = {}
        for comp, attr in components:
            footprint = footprints[attr["Footprint"]]
            ref = attr["Designator"]

            def transform(x, y):
                return placed(comp, x, y)

            for _, pad in records(footprint, "PAD"):
                if comp["layerId"] != (2 if bottom else 1) and pad["layerId"] != 12:
                    continue
                x, y = pad["centerX"], pad["centerY"]
                w, h = pad["defaultPad"]["width"] / 2, pad["defaultPad"]["height"] / 2
                corners = [
                    transform(x + dx, y + dy)
                    for dx, dy in ((-w, -h), (w, -h), (w, h), (-w, h))
                ]
                poly([(px, py, z) for px, py in corners], "#d8b85c", "#c6a24e")
                hole = pad.get("hole")
                if hole and hole["width"] > 0:
                    hole_shape = [
                        transform(
                            x + hole["width"] / 2 * math.cos(i * math.tau / 20),
                            y + hole["height"] / 2 * math.sin(i * math.tau / 20),
                        )
                        for i in range(20)
                    ]
                    poly([(px, py, z) for px, py in hole_shape], "#101820", "#d8b85c")
            if comp["layerId"] != (2 if bottom else 1):
                continue
            contours = [
                b["path"] for _, b in records(footprint, "POLY") if b["layerId"] == 48
            ]
            if not contours:
                contours = [
                    b["path"][0]
                    for _, b in records(footprint, "FILL")
                    if b["layerId"] == 48
                ]
            if len(contours) != 1:
                raise ValueError(f"Expected one package body: {ref}")
            body_shape = [transform(x, y) for x, y in polygon_path(contours[0])]
            height = {"U1": 4, "CN1": 3.5, "U2": 1.7, "R1": 0.4, "C1": 0.8, "C2": 0.8}[
                ref
            ]
            top = z - height if bottom else z + height
            color = ("#dfdfce", "#9c9d90") if ref == "CN1" else ("#414956", "#252b34")
            if ref.startswith("C") and ref != "CN1":
                color = ("#c8b393", "#8e7a60")
            depth = sum(
                v * n for v, n in zip((comp["x"] * MIL, comp["y"] * MIL, top), normal)
            )
            sockets = []
            if ref == "U1":
                for _, pad in records(footprint, "PAD"):
                    x, y = pad["centerX"], pad["centerY"]
                    sockets.append(
                        [
                            (*transform(x + dx, y + dy), top)
                            for dx, dy in ((-19, -19), (19, -19), (19, 19), (-19, 19))
                        ]
                    )
            bodies.append((depth, body_shape, top, color, sockets))
            anchors[ref] = point((comp["x"] * MIL, comp["y"] * MIL, top))
        for _, shape, top, color, sockets in sorted(bodies):
            solid(shape, z, top, color)
            for socket in sockets:
                poly(socket, "#0b1018", "#68717b")
        labels = (
            {"U1": (cx - 220, cy + 180, "U1 · underside 2 × 13 socket")}
            if bottom
            else {
                "CN1": (cx - 285, cy + 180, "CN1 · 6-pin vehicle connector"),
                "U2": (cx + 30, cy + 210, "U2 · CAN transceiver"),
                "R1": (cx - 30, cy + 260, "R1 · 120 Ω termination"),
                "C1": (cx + 80, cy - 170, "C1 · 5 V bypass"),
                "C2": (cx - 240, cy - 170, "C2 · 3.3 V bypass"),
            }
        )
        for ref, (x, y, label) in labels.items():
            a, b = anchors[ref]
            svg.append(
                f'<path d="M{a:.1f},{b:.1f} L{x},{y - 9}" stroke="#8adcca" fill="none" stroke-width="1.5"/>'
            )
            text(x, y, label, 18)

    text(54, 159, "TOP / COMPONENT SIDE", 18, "#8adcca")
    text(755, 159, "UNDERSIDE / DISPLAY SOCKET", 18, "#8adcca")
    view(350, 400, False)
    view(1040, 400, True)
    text(
        54,
        785,
        "Placements, footprints, pads and track segments are read from the supplied PCB source.",
        20,
    )
    text(
        54,
        823,
        "Package bodies/heights are simplified; connector heights are illustrative. Copper pours are omitted.",
        18,
        "#aabdc8",
    )
    text(
        54,
        853,
        "Use the native project for fabrication, pin orientation and mechanical clearance checks.",
        18,
        "#aabdc8",
    )
    text(54, 900, f"Source SHA-256: {digest}", 14, "#aabdc8")
    svg.append("</g></svg>")
    return "\n".join(svg) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source", type=Path, default=ROOT / "pcb/guition-43/Guition_CAN_PCB.epro2"
    )
    parser.add_argument("--output", type=Path, default=ROOT / "docs/images")
    args = parser.parse_args()
    sections, previews = read_project(args.source)
    digest = hashlib.sha256(args.source.read_bytes()).hexdigest()
    svg = illustration(sections, digest)
    # Explicit source preview identities: fail rather than swap schematic and board.
    names = {
        "IMAGE/84508ea0144e638c.webp": "guition-can-pcb-layout.png",
        "IMAGE/2741601ba4c06bd9.webp": "guition-can-pcb-schematic.png",
    }
    if not set(names).issubset(previews):
        raise ValueError(
            "Embedded preview IDs changed; inspect and update their mapping"
        )
    args.output.mkdir(parents=True, exist_ok=True)
    for name, destination in names.items():
        with Image.open(io.BytesIO(previews[name])) as image:
            image.convert("RGB").save(args.output / destination)
    (args.output / "guition-can-pcb-labelled.svg").write_text(
        svg, encoding="utf-8", newline="\n"
    )
    for bottom, name in ((False, "top"), (True, "bottom")):
        layout_image(sections, bottom).save(args.output / f"guition-can-pcb-{name}.png")
    print(f"Generated 5 PCB images from {args.source.name}; SHA-256 {digest}")


if __name__ == "__main__":
    main()
