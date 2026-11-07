#!/usr/bin/env python3
"""Build repository documentation into a portable GitHub Pages artifact."""

from __future__ import annotations

import argparse
import html
import json
import os
import re
import shutil
from pathlib import Path
from urllib.parse import urlsplit, urlunsplit

from site_assets import bike_icon, favicon

try:
    import markdown
except ImportError as exc:
    raise SystemExit(
        "Install docs dependencies: python -m pip install -r requirements-docs.txt"
    ) from exc
ROOT = Path(__file__).resolve().parent.parent
REPO = "https://github.com/techfoundrynz/wheelie_fun_display"
PAGES = [
    (
        "GETTING-STARTED.MD",
        "getting-started",
        "Getting started",
        "Start here",
        "Power up, choose an integration and connect your first bike.",
    ),
    (
        "docs/user-guide.md",
        "guide",
        "Using the display",
        "For riders",
        "Ride, Stats, Tune, gestures and grouped settings.",
    ),
    (
        "docs/connections.md",
        "connections",
        "Connections & proxy",
        "For riders",
        "BLE, CAN, UART and your phone's transparent VESC connection.",
    ),
    (
        "docs/pins-and-locking.md",
        "pins",
        "PINs, patterns & pairing",
        "For riders",
        "Local PINs, patterns, upstream pairing and phone proxy bonds.",
    ),
    (
        "docs/maintenance.md",
        "maintenance",
        "Updates & maintenance",
        "For riders",
        "Install updates, diagnose issues and prepare release artifacts.",
    ),
    (
        "docs/troubleshooting.md",
        "troubleshooting",
        "Troubleshooting",
        "For riders",
        "Find the right next check for connection, touch or update issues.",
    ),
    (
        "docs/hardware-waveshare-206.md",
        "hardware/waveshare-206",
        "Waveshare S3 2.06-inch",
        "Hardware",
        "AMOLED setup, board wiring, orientation and USB recovery.",
    ),
    (
        "docs/hardware-guition-43.md",
        "hardware/guition-43",
        "GUITION P4 4.3-inch",
        "Hardware",
        "P4/C6 board setup, companion firmware and hardware requirements.",
    ),
    (
        "docs/building.md",
        "building",
        "Build & install",
        "For developers",
        "Set up PlatformIO, install firmware and run the shared simulators.",
    ),
    (
        "CONTRIBUTING.md",
        "contributing",
        "Contributing",
        "For developers",
        "Make a focused change and show the evidence that it works.",
    ),
    (
        "docs/architecture.md",
        "architecture",
        "Architecture",
        "For developers",
        "Ownership, integrations, transports and shared UI components.",
    ),
    (
        "docs/performance.md",
        "performance",
        "Performance",
        "For developers",
        "Configuration decisions and the limits of measured results.",
    ),
    (
        "TODO.md",
        "todo",
        "Planned work",
        "For developers",
        "Implementation and hardware acceptance tasks for contributors.",
    ),
    (
        "docs/integrations.md",
        "integrations",
        "Vehicle integrations",
        "Reference",
        "Capabilities and packet formats for WFB, wheel-e and VESC.",
    ),
    (
        "docs/locking.md",
        "locking",
        "Locking internals",
        "Reference",
        "State confirmation, local PIN storage and timeout policy.",
    ),
    (
        "docs/bluetooth.md",
        "bluetooth",
        "Bluetooth internals",
        "Reference",
        "Connection generations, proxy ownership and transport behavior.",
    ),
    (
        "docs/transports.md",
        "transports",
        "Transport internals",
        "Reference",
        "Physical interfaces, native CAN framing and controller discovery.",
    ),
    (
        "docs/wheel-e-protocol.md",
        "reference/wheel-e-protocol",
        "wheel-e protocol notes",
        "Reference",
        "Package identity, modern telemetry, locking and compatibility for wheel-e.",
    ),
    (
        "docs/wheelie-vesc-comms-api.md",
        "reference/vesc-protocol",
        "WFB protocol notes",
        "Reference",
        "VESC transport, telemetry and configuration reference for WFB.",
    ),
    (
        "docs/wheelie-wfb-bike-layer.md",
        "reference/wfb-bike-layer",
        "WFB bike layer",
        "Reference",
        "WFB command layouts and configuration details.",
    ),
    (
        "docs/wheelie-theming-guide.md",
        "reference/theme",
        "Theme guide",
        "Reference",
        "Colours, typography and component styling for the display.",
    ),
    (
        "THIRD_PARTY.md",
        "reference/notices",
        "Third-party notices",
        "Reference",
        "Licenses and notices for bundled components and assets.",
    ),
]
SOURCE_ROUTES = {(ROOT / source).resolve(): route for source, route, *_ in PAGES}
SOURCE_ROUTES[(ROOT / "README.md").resolve()] = ""


def escape(value: str) -> str:
    return html.escape(value, quote=True)


def relative(target: str, route: str) -> str:
    value = os.path.relpath(target or ".", Path(route) if route else Path(".")).replace(
        os.sep, "/"
    )
    return "./" if value == "." else value + "/"


def asset(name: str, route: str) -> str:
    return relative("", route) + "assets/" + name


def rewrite_links(body: str, source: Path, route: str) -> str:
    def rewrite(match: re.Match[str]) -> str:
        attr, quote, url = match.groups()
        parts = urlsplit(html.unescape(url))
        # Keep source Markdown useful on GitHub while making downloaded Pages
        # artifacts portable between project-subpath and root hosting.
        if (
            parts.scheme == "https"
            and parts.netloc == "techfoundrynz.github.io"
            and parts.path.rstrip("/") == "/wheelie_fun_display/install"
        ):
            path = relative("install", route)
            return f"{attr}={quote}{escape(urlunsplit(('', '', path, parts.query, parts.fragment)))}{quote}"
        if parts.scheme or parts.netloc or not parts.path:
            return match.group(0)
        target = (source.parent / parts.path).resolve()
        if target in SOURCE_ROUTES:
            path = relative(SOURCE_ROUTES[target], route)
        elif target in {
            (ROOT / name).resolve() for name in ("simulator", "ci", "install")
        }:
            path = relative(target.name, route)
        elif target.is_relative_to((ROOT / "docs/images").resolve()):
            path = asset("images/" + target.name, route)
        elif target.is_relative_to(ROOT):
            path = REPO + "/blob/main/" + target.relative_to(ROOT).as_posix()
        else:
            raise ValueError(f"Documentation link escapes repository: {url}")
        return f"{attr}={quote}{escape(urlunsplit(('', '', path, parts.query, parts.fragment)))}{quote}"

    return re.sub(r"(href|src)=([\"'])(.*?)\2", rewrite, body)


def logo() -> str:
    return bike_icon()


def navigation(route: str, available: list[tuple]) -> str:
    groups: dict[str, list[str]] = {}
    for _, slug, title, group, _ in available:
        current = ' aria-current="page"' if slug == route else ""
        groups.setdefault(group, []).append(
            f'<a href="{relative(slug, route)}"{current}>{escape(title)}</a>'
        )
    groups.setdefault("Start here", []).append(
        f'<a href="{relative("install", route)}">Install firmware</a>'
    )
    return "".join(
        f'<div class="nav-group"><span>{escape(group)}</span>{"".join(links)}</div>'
        for group, links in groups.items()
    )


def layout(
    title: str,
    description: str,
    route: str,
    content: str,
    toc: str,
    available: list[tuple],
    source: str = "",
    home: bool = False,
) -> str:
    section = (
        "Explore"
        if home
        else next((p[3] for p in available if p[1] == route), "Documentation")
    )
    page_title = (
        "Wheelie Fun Display — Ride, connect, make it yours"
        if home
        else title + " · Wheelie Fun Display"
    )
    edit = (
        f'<a class="source-link" href="{REPO}/blob/main/{source}">View source <span aria-hidden="true">↗</span></a>'
        if source
        else ""
    )
    header = (
        ""
        if home
        else f'<header class="article-header"><div class="eyebrow">{escape(section)}</div><h1>{escape(title)}</h1><p>{escape(description)}</p>{edit}</header>'
    )
    return f'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>{escape(page_title)}</title><meta name="description" content="{escape(description)}"><meta name="theme-color" content="#080b0e">
<link rel="icon" href="{asset("favicon.svg", route)}" type="image/svg+xml"><link rel="stylesheet" href="{asset("style.css", route)}">
<script defer src="{asset("search-index.js", route)}"></script><script defer src="{asset("site.js", route)}"></script></head>
<body data-site-root="{relative("", route)}" class="{"home-page" if home else "docs-page"}">
<a class="skip-link" href="#content">Skip to content</a>
<header class="site-header"><a class="brand" href="{relative("", route)}">{logo()}<span>Wheelie Fun Display</span><span class="docs-label">Docs</span></a>
<div class="header-actions"><a class="installer-link" href="{relative("install", route)}">Install firmware</a><a class="simulator-link" href="{relative("simulator", route)}">Try simulator <span aria-hidden="true">↗</span></a><button class="search-trigger" type="button" aria-label="Search documentation"><span aria-hidden="true">⌕</span><span>Search</span><kbd>/</kbd></button><button class="menu-trigger" type="button" aria-label="Open navigation" aria-controls="site-navigation" aria-expanded="false">☰</button></div></header>
<button class="nav-overlay" type="button" aria-label="Close navigation"></button>
<div class="site-layout"><aside class="sidebar" id="site-navigation" aria-label="Documentation navigation"><a class="overview-link" href="{relative("", route)}">Overview</a>{navigation(route, available)}<a class="github-link" href="{REPO}">Source on GitHub <span aria-hidden="true">↗</span></a></aside>
<main id="content" tabindex="-1" class="{"home-main" if home else "article-main"}">{header}{content}<footer class="page-footer"><span>Built for VESC bikes. Open source.</span><span><a href="{REPO}/blob/main/LICENSE">GPL-3.0-only</a> · <a href="{relative("reference/notices", route)}">Notices</a></span></footer></main>
<aside class="page-toc" aria-label="On this page">{"<span>On this page</span>" + toc if toc else ""}</aside></div>
<dialog class="search-dialog" aria-label="Search documentation"><form method="dialog"><button class="close-search" aria-label="Close search">×</button></form><label for="docs-search">Search Wheelie Fun Display</label><input id="docs-search" type="search" placeholder="Connection, PIN, build…" autocomplete="off"><div class="search-results" aria-live="polite"></div><p class="search-hint">Search the guides and implementation notes. Press Esc to close.</p></dialog>
</body></html>'''


def home_content() -> str:
    return """<section class="hero"><div class="hero-copy"><div class="eyebrow"><span class="status-dot"></span> Touch display · VESC integrations</div><h1>Your ride.<br><span>A clearer view.</span></h1><p>A responsive bike display for Wheelie Fun Bike, wheel-e and VESC. Learn the controls, connect your bike and build the firmware.</p><div class="hero-actions"><a class="button primary" href="install/">Install firmware <span aria-hidden="true">→</span></a><a class="button secondary" href="simulator/">Open simulator</a></div><div class="hero-tags"><span>ESP32-S3 / P4</span><span>Slint UI</span><span>GPL-3.0</span></div></div><figure class="hero-preview"><div class="preview-glow"></div><img class="portrait-preview" src="assets/images/ride-portrait.png" width="410" height="502" alt="Portrait ride dashboard showing simulated speed and telemetry"><figcaption>Shared UI preview · simulated values</figcaption></figure></section>
<section class="intro-cards" aria-label="Choose a guide"><a class="intro-card" href="guide/"><span class="card-number">01 / RIDE</span><h2>Know your display</h2><p>Three main pages, fluid gestures, clear telemetry and settings that stay with you.</p><span class="card-link">Explore the interface →</span></a><a class="intro-card" href="connections/"><span class="card-number">02 / CONNECT</span><h2>Choose your connection</h2><p>BLE by default. Add CAN or UART for your hardware, with a transparent phone proxy.</p><span class="card-link">Connect a bike →</span></a><a class="intro-card" href="building/"><span class="card-number">03 / BUILD</span><h2>Build the firmware</h2><p>Reproducible firmware builds, shared components and checks grounded in evidence.</p><span class="card-link">Build and contribute →</span></a></section>
<section class="landscape-section"><div><div class="eyebrow">Made for the screen you have</div><h2>Portrait or landscape.<br>One familiar interface.</h2><p>Change orientation at runtime. Touch mapping, rounded-corner spacing and reusable controls follow the layout.</p><a class="text-link" href="guide/">See everyday controls →</a></div><figure><img src="assets/images/ride-landscape.png" width="502" height="410" alt="Landscape dashboard with simulated ride values"><figcaption>Landscape software preview</figcaption></figure></section>
<section class="supported-section"><div class="eyebrow">Start with supported hardware</div><h2>Two displays. One shared interface.</h2><p>Waveshare ESP32-S3 AMOLED 2.06 pairs a 410 × 502 CO5300 panel with FT3168 touch. GUITION JC4880P443C uses a 480 × 800 ST7701S panel, GT911 touch and ESP32-P4 with a C6 radio companion. Choose the exact model when installing; board capabilities and optional wiring live in its PlatformIO profile.</p><div class="supported-links"><a href="getting-started/">First connection →</a><a href="install/">Install firmware →</a><a href="hardware/waveshare-206/">Waveshare board setup →</a><a href="hardware/guition-43/">GUITION board setup →</a><a href="maintenance/">Maintaining the display →</a></div></section>"""


def build(output: Path, images: Path) -> None:
    output = output.resolve()
    if output == ROOT or not output.is_relative_to(ROOT):
        raise SystemExit(
            "Documentation output must be a separate directory inside the repository"
        )
    output.mkdir(parents=True, exist_ok=True)
    assets = output / "assets"
    assets.mkdir(exist_ok=True)
    for name in ("style.css", "site.js"):
        shutil.copyfile(ROOT / "docs/site" / name, assets / name)
    (assets / "favicon.svg").write_text(favicon(), encoding="utf-8", newline="\n")
    if not (images / "manifest.json").is_file():
        raise SystemExit(
            "Generate documentation screenshots first: python scripts/generate_docs_screenshots.py"
        )
    manifest = json.loads((images / "manifest.json").read_text(encoding="utf-8"))
    for name in manifest["images"]:
        if Path(name).name != name or not (images / name).is_file():
            raise SystemExit(
                f"Missing or invalid generated documentation image: {name}"
            )
    source_images = ROOT / "docs/images"
    if source_images.is_dir():
        shutil.copytree(source_images, assets / "images", dirs_exist_ok=True)
    (assets / "images").mkdir(exist_ok=True)
    for name in ["manifest.json"] + manifest["images"]:
        shutil.copyfile(images / name, assets / "images" / name)
    fonts = assets / "fonts"
    fonts.mkdir(exist_ok=True)
    shutil.copyfile(ROOT / "ui/fonts/Roboto.ttf", fonts / "Roboto.ttf")
    shutil.copyfile(ROOT / "ui/fonts/Roboto-OFL.txt", fonts / "Roboto-OFL.txt")
    available = [page for page in PAGES if (ROOT / page[0]).is_file()]
    current_pages = {"index.html", "404.html"} | {
        page[1] + "/index.html" for page in available
    }
    previous_manifest = output / "docs-manifest.json"
    if previous_manifest.is_file():
        previous_pages = json.loads(previous_manifest.read_text(encoding="utf-8"))[
            "pages"
        ]
        for name in set(previous_pages) - current_pages:
            stale = (output / name).resolve()
            if not stale.is_relative_to(output) or stale.suffix != ".html":
                raise SystemExit(f"Invalid previous documentation page: {name}")
            stale.unlink(missing_ok=True)
            if (
                stale.parent != output
                and stale.parent.is_dir()
                and not any(stale.parent.iterdir())
            ):
                stale.parent.rmdir()
    # Earlier previews copied native renderer captures into the source tree.
    # Remove only those retired generated names, retaining other source assets.
    for name in (
        "dashboard-320x480.png",
        "dashboard-410x502.png",
        "dashboard-502x410.png",
    ):
        if name not in manifest["images"] and not (source_images / name).is_file():
            (assets / "images" / name).unlink(missing_ok=True)
    search = []
    for source, route, title, _, description in available:
        source_path = ROOT / source
        text = re.sub(
            r"^# [^\n]+\n+", "", source_path.read_text(encoding="utf-8"), count=1
        )
        converter = markdown.Markdown(
            extensions=["extra", "toc", "sane_lists"],
            extension_configs={"toc": {"toc_depth": "2-3"}},
        )
        body = rewrite_links(converter.convert(text), source_path, route)
        body = re.sub(
            r"(<table>.*?</table>)",
            r'<div class="table-scroll" tabindex="0">\1</div>',
            body,
            flags=re.DOTALL,
        )
        destination = output / route / "index.html"
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(
            layout(
                title,
                description,
                route,
                '<article class="md-content">' + body + "</article>",
                converter.toc,
                available,
                source,
            ),
            encoding="utf-8",
            newline="\n",
        )
        searchable = re.sub(r"<[^>]+>", " ", body)
        search.append(
            {
                "title": title,
                "url": route + "/",
                "description": description,
                "text": html.unescape(re.sub(r"\s+", " ", searchable))[:7000],
            }
        )
    (output / "index.html").write_text(
        layout(
            "Wheelie Fun Display",
            "Guides, simulator and development notes for the VESC bike display.",
            "",
            home_content(),
            "",
            available,
            home=True,
        ),
        encoding="utf-8",
        newline="\n",
    )
    (assets / "search-index.js").write_text(
        "window.WFD_DOCS_SEARCH=" + json.dumps(search, ensure_ascii=False) + ";\n",
        encoding="utf-8",
        newline="\n",
    )
    (output / ".nojekyll").write_text("", encoding="utf-8")
    (output / "404.html").write_text(
        layout(
            "Page not found",
            "This link does not point to a documentation page.",
            "",
            '<article class="md-content"><p>Return to <a href="./">the documentation overview</a> or use Search to find the guide.</p></article>',
            "",
            available,
        ),
        encoding="utf-8",
        newline="\n",
    )
    manifest = {
        "pages": ["index.html", "404.html"] + [p[1] + "/index.html" for p in available],
        "simulator": "simulator/",
        "installer": "install/",
        "sources": len(available),
    }
    (output / "docs-manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n"
    )
    print(f"Built {len(available)} documentation pages + overview into {output}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / ".cache/pages/site")
    parser.add_argument(
        "--images",
        type=Path,
        default=ROOT / ".cache/pages/docs-images",
        help="Screenshots generated from the current shared Slint browser simulator",
    )
    args = parser.parse_args()
    build(args.output, args.images)


if __name__ == "__main__":
    main()
