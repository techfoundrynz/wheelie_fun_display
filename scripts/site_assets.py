"""Render shared website bicycle artwork from one geometry source."""

from pathlib import Path
from xml.etree import ElementTree

ASSETS = Path(__file__).resolve().parents[1] / "docs/site"
BIKE_MARKER = "<!-- WFD_BIKE_ICON -->"
SHAPE_MARKER = "<!-- WFD_BIKE_SHAPE -->"
FIT_MARKER = "<!-- WFD_BIKE_FIT -->"


def bike_icon():
    return (ASSETS / "bike.svg").read_text(encoding="utf-8").strip()


def favicon():
    source = bike_icon()
    x, y, width, height = map(
        float, ElementTree.fromstring(source).attrib["viewBox"].split()
    )
    scale = 28.8 / max(width, height)
    fit = f"translate(16 16) scale({scale:.9f}) translate({-(x + width / 2):g} {-(y + height / 2):g})"
    shape = source.partition(">")[2].rsplit("</svg>", 1)[0].strip()
    template = (ASSETS / "favicon.svg.in").read_text(encoding="utf-8")
    if template.count(SHAPE_MARKER) != 1 or template.count(FIT_MARKER) != 1:
        raise ValueError(
            "Favicon template must contain one shared bicycle shape marker"
        )
    return template.replace(SHAPE_MARKER, shape).replace(FIT_MARKER, fit)
