#!/usr/bin/env python3
"""Validate the built docs and exercise desktop/mobile navigation with Playwright."""

from __future__ import annotations

import argparse
import json
import math
import os
import threading
from functools import partial
from html.parser import HTMLParser
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit
from xml.etree import ElementTree


class Document(HTMLParser):
    def __init__(self):
        super().__init__()
        self.links = []
        self.ids = set()
        self.missing_alt = 0

    def handle_starttag(self, tag, attrs):
        data = dict(attrs)
        if "id" in data:
            self.ids.add(data["id"])
        if tag == "img" and "alt" not in data:
            self.missing_alt += 1
        for key in ("href", "src"):
            if key in data:
                self.links.append((tag, data[key]))


class QuietHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, base_path="/wheelie_fun_display/", **kwargs):
        self.base_path = base_path
        super().__init__(*args, **kwargs)

    def do_GET(self):
        if not self.path.startswith(self.base_path):
            self.send_error(404)
            return
        super().do_GET()

    def translate_path(self, path):
        return super().translate_path("/" + path[len(self.base_path) :])

    def log_message(self, *args):
        pass


def check_links(site: Path):
    favicon = ElementTree.parse(site / "assets/favicon.svg").getroot()
    assert favicon.find(".//*[@id='bike-artwork']") is not None
    manifest = json.loads((site / "docs-manifest.json").read_text(encoding="utf-8"))
    documents = {}
    for name in manifest["pages"] + [
        "simulator/index.html",
        "install/index.html",
        "ci/index.html",
    ]:
        parser = Document()
        parser.feed((site / name).read_text(encoding="utf-8"))
        documents[(site / name).resolve()] = parser
        assert not parser.missing_alt, f"Missing image alt text: {name}"
    checked = 0
    for source, document in documents.items():
        for tag, url in document.links:
            parts = urlsplit(url)
            if parts.scheme or parts.netloc:
                continue
            target = (
                (source.parent / unquote(parts.path)).resolve()
                if parts.path
                else source
            )
            assert target.is_relative_to(site), f"Escaping local link: {source}: {url}"
            if target.is_dir() or parts.path.endswith("/"):
                target = target / "index.html"
            if not target.exists() and target == site / "simulator/index.html":
                continue
            assert target.exists(), (
                f"Missing local link: {source.relative_to(site)}: {url}"
            )
            if parts.fragment and target in documents:
                assert unquote(parts.fragment) in documents[target].ids, (
                    f"Missing anchor: {source.relative_to(site)}: {url}"
                )
            checked += 1
    print(
        f"Docs links: {len(documents)} pages, {checked} local references, image alt text checked"
    )
    return manifest


def browser_checks(site: Path, executable: str | None, output: Path, base_path: str):
    from playwright.sync_api import sync_playwright

    server = ThreadingHTTPServer(
        ("127.0.0.1", 0),
        partial(QuietHandler, directory=str(site), base_path=base_path),
    )
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    url = f"http://127.0.0.1:{server.server_port}" + base_path
    output.mkdir(parents=True, exist_ok=True)
    try:
        with sync_playwright() as p:
            browser = p.chromium.launch(
                headless=True, **({"executable_path": executable} if executable else {})
            )
            errors = []
            page = browser.new_page(
                viewport={"width": 1440, "height": 1050}, device_scale_factor=1
            )
            page.on("pageerror", lambda error: errors.append(str(error)))
            manifest = json.loads(
                (site / "docs-manifest.json").read_text(encoding="utf-8")
            )
            for width, height in [(1440, 1050), (390, 844)]:
                page.set_viewport_size({"width": width, "height": height})
                for route in [""] + [
                    name.removesuffix("index.html")
                    for name in manifest["pages"]
                    if name not in ("index.html", "404.html")
                ]:
                    page.goto(url + route, wait_until="networkidle")
                    assert page.locator("h1").count() == 1, (
                        f"Unexpected primary heading: {route}"
                    )
                    assert page.evaluate(
                        "document.documentElement.scrollWidth <= innerWidth + 1"
                    ), f"Horizontal overflow: {route} at {width}"
                    assert page.evaluate(
                        "[...document.images].every(image=>image.complete && image.naturalWidth>0)"
                    ), f"Unloaded image: {route}"
                    if route in ["", "getting-started/", "building/"]:
                        page.screenshot(
                            path=str(
                                output / f"{width}-{route.strip('/') or 'home'}.png"
                            ),
                            full_page=True,
                        )
                page.goto(url, wait_until="networkidle")
                page.get_by_role("button", name="Search documentation").click()
                page.get_by_role("searchbox", name="Search Wheelie Fun Display").fill(
                    "pairing"
                )
                assert page.locator(".search-results a").count() > 0, (
                    "Search produced no results"
                )
                assert page.locator(".search-results").inner_text().find("PIN") >= 0, (
                    "Expected PIN guide missing from search"
                )
                page.keyboard.press("Escape")
                page.locator(".search-dialog").wait_for(state="hidden")
                if width < 900:
                    button = page.get_by_role("button", name="Open navigation")
                    button.click()
                    assert button.get_attribute("aria-expanded") == "true"
                    page.locator("#site-navigation").get_by_role(
                        "link", name="Getting started", exact=True
                    ).click()
                    assert page.url.endswith("/getting-started/")
                    page.wait_for_load_state("networkidle")
                    page.keyboard.press("/")
                    page.locator(".search-dialog").wait_for(state="visible")
                    page.keyboard.press("Escape")
            # Public preview stays separate from maintainer-only CI evidence.
            for route in ["simulator/", "install/", "ci/"]:
                page.goto(url + route, wait_until="networkidle")
                assert page.locator("h1").count() == 1
                assert page.evaluate(
                    "document.documentElement.scrollWidth <= innerWidth + 1"
                )
                if route == "simulator/":
                    page.wait_for_function(
                        "window.wheelieFunDisplay && document.querySelector('#status').textContent.startsWith('Ready')"
                    )
                    assert "Roboto" in page.evaluate(
                        "getComputedStyle(document.body).fontFamily"
                    )
                    assert page.locator(".brand").inner_text() == "Wheelie Fun Display"
                    assert page.locator(".gallery").count() == 0
                    assert page.locator("a[href*='ci/']").count() == 0
                elif route == "install/":
                    page.locator("#board-select").wait_for(state="visible")
                    assert page.locator(".brand").inner_text() == "Wheelie Fun Display"
                    assert page.locator("#firmware-status").inner_text()
                else:
                    assert page.locator(".gallery img").count() >= 5
                    assert page.evaluate(
                        "[...document.images].every(image => image.complete && image.naturalWidth > 0)"
                    )
                    assert page.request.get(url + "ci/results.json").json()["passed"]
            # All public surfaces share branding, font, colors and the wheelie mark.
            for width in (1440, 390):
                page.set_viewport_size({"width": width, "height": 1000})
                baseline = None
                for route in ("", "simulator/", "install/"):
                    page.goto(url + route, wait_until="networkidle")
                    theme = page.evaluate(
                        """() => {
                            const svg = document.querySelector('.brand > svg');
                            const matrix = svg.querySelector('#bike-artwork').transform.baseVal.consolidate().matrix;
                            return {font: getComputedStyle(document.body).fontFamily,
                                background: getComputedStyle(document.body).backgroundColor,
                                mark: getComputedStyle(svg).transform,
                                artwork: [matrix.a, matrix.b, matrix.c, matrix.d]};
                        }"""
                    )
                    assert "Roboto" in theme["font"]
                    assert theme["mark"] == "none", (
                        "Artwork must not receive a second CSS rotation"
                    )
                    cosine, sine = (
                        math.cos(math.radians(30)),
                        math.sin(math.radians(30)),
                    )
                    assert all(
                        abs(actual - expected) < 0.0001
                        for actual, expected in zip(
                            theme["artwork"],
                            [-cosine, -sine, -sine, cosine],
                            strict=True,
                        )
                    ), "Flip must precede clockwise rotation"
                    baseline = baseline or theme
                    assert theme == baseline, (
                        f"Shared styling differs: {route} at {width}"
                    )
                    assert page.evaluate(
                        "document.documentElement.scrollWidth <= innerWidth + 1"
                    )
                    page.screenshot(
                        path=str(
                            output / f"{width}-shared-{route.strip('/') or 'home'}.png"
                        ),
                        full_page=True,
                    )
            bounds = page.evaluate(
                """async url => {
                    const source = await (await fetch(url)).text();
                    const svg = new DOMParser().parseFromString(source, 'image/svg+xml').documentElement;
                    svg.setAttribute('width', '32'); svg.setAttribute('height', '32');
                    svg.style.cssText = 'position:absolute;left:-100px;top:0;opacity:0';
                    document.body.append(svg);
                    const boxes = [...svg.querySelectorAll('#bike-artwork circle, #bike-artwork path, #bike-artwork rect')].map(shape => {
                        const m = shape.getCTM(), style = getComputedStyle(shape);
                        const stroke = style.stroke === 'none' ? 0 : parseFloat(style.strokeWidth) / 2;
                        const hx = stroke * Math.hypot(m.a, m.c), hy = stroke * Math.hypot(m.b, m.d);
                        if (shape.tagName === 'circle') {
                            const center = new DOMPoint(shape.cx.baseVal.value, shape.cy.baseVal.value).matrixTransform(m);
                            const rx = shape.r.baseVal.value * Math.hypot(m.a, m.c) + hx;
                            const ry = shape.r.baseVal.value * Math.hypot(m.b, m.d) + hy;
                            return [center.x - rx, center.y - ry, center.x + rx, center.y + ry];
                        }
                        const b = shape.getBBox();
                        const points = [[b.x,b.y],[b.x+b.width,b.y],[b.x,b.y+b.height],[b.x+b.width,b.y+b.height]]
                            .map(([x,y]) => new DOMPoint(x,y).matrixTransform(m));
                        return [Math.min(...points.map(p=>p.x))-hx, Math.min(...points.map(p=>p.y))-hy,
                            Math.max(...points.map(p=>p.x))+hx, Math.max(...points.map(p=>p.y))+hy];
                    });
                    svg.remove(); return boxes;
                }""",
                url + "assets/favicon.svg",
            )
            assert bounds and all(
                min(box[:2]) >= 0 and max(box[2:]) <= 32 for box in bounds
            ), ("Favicon artwork or strokes exceed the viewport", bounds)
            assert not errors, "Browser errors: " + repr(errors)
            browser.close()
            print(
                "Docs browser checks: desktop/mobile, overflow, images, navigation, search and keyboard passed"
            )
    finally:
        server.shutdown()
        server.server_close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--site", type=Path, default=Path(".cache/pages/site"))
    parser.add_argument(
        "--browser-executable", default=os.environ.get("PLAYWRIGHT_CHROMIUM_EXECUTABLE")
    )
    parser.add_argument(
        "--output", type=Path, default=Path(".cache/pages/docs-browser-tests")
    )
    parser.add_argument("--links-only", action="store_true")
    parser.add_argument("--base-path", default="/wheelie_fun_display/")
    args = parser.parse_args()
    site = args.site.resolve()
    check_links(site)
    if not args.links_only:
        browser_checks(
            site,
            args.browser_executable,
            args.output.resolve(),
            ("/" + args.base_path.strip("/") + "/")
            if args.base_path.strip("/")
            else "/",
        )


if __name__ == "__main__":
    main()
