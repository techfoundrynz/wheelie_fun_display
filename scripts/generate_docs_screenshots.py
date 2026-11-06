"""Render documentation pictures from the current shared Slint/WASM UI.

Generated pictures live in .cache, never in the source tree. Build the browser
simulator first; the Pages job does both steps before assembling documentation.
"""

from __future__ import annotations

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import threading

from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--site", type=Path, default=ROOT / ".cache/pages/site")
    parser.add_argument(
        "--output", type=Path, default=ROOT / ".cache/pages/docs-images"
    )
    parser.add_argument("--browser-executable")
    args = parser.parse_args()
    site = args.site.resolve()
    assert (site / "simulator/pkg/wheelie_fun_display_bg.wasm").is_file(), (
        "Build the browser simulator first"
    )
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "manifest.json").unlink(missing_ok=True)

    class Handler(SimpleHTTPRequestHandler):
        def log_message(self, *_):
            pass

    server = ThreadingHTTPServer(
        ("127.0.0.1", 0), partial(Handler, directory=str(site))
    )
    threading.Thread(target=server.serve_forever, daemon=True).start()
    images = []
    errors = []
    try:
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch(
                headless=True, executable_path=args.browser_executable
            )
            page = browser.new_page(
                viewport={"width": 1280, "height": 1080}, device_scale_factor=1
            )
            page.on("pageerror", lambda error: errors.append(str(error)))
            base = f"http://127.0.0.1:{server.server_port}/simulator/"

            def fresh(query=""):
                page.goto(base + (query + "&" if query else "?") + "preview=docs")
                page.wait_for_function(
                    "window.wheelieFunDisplay && document.querySelector('#status').textContent.startsWith('Ready')"
                )
                page.wait_for_timeout(350)
                assert page.evaluate(
                    "window.wheelieFunDisplay.state('documentation-fixture')"
                )
                assert page.evaluate("window.wheelieFunDisplay.state('connected')")
                assert (
                    page.evaluate("window.wheelieFunDisplay.state('connection')")
                    == "Connected / BLE"
                )
                assert "simulated" in page.locator(".notice").inner_text().lower()
                for field in ["feedback", "device-label", "connection"]:
                    value = page.evaluate(
                        "name => window.wheelieFunDisplay.state(name)", field
                    ).lower()
                    assert not any(
                        word in value for word in ["demo", "simulator", "simulated"]
                    ), (field, value)
                if not query or "orientation=" in query or "integration=" in query:
                    assert (
                        page.evaluate("window.wheelieFunDisplay.state('speed')")
                        == "24.6"
                    )
                    assert (
                        0.4
                        < page.evaluate(
                            "window.wheelieFunDisplay.state('speed-progress')"
                        )
                        < 0.5
                    )

            def capture(name):
                page.wait_for_timeout(350)  # Let shared page transitions finish.
                canvas = page.locator("#display")
                assert canvas.evaluate("c => c.width > 0 && c.height > 0")
                canvas.screenshot(path=str(args.output / name), animations="disabled")
                images.append(name)

            fresh()
            capture("ride-portrait.png")
            fresh("?orientation=landscape")
            capture("ride-landscape.png")
            fresh("?integration=wheel-e")
            capture("wheel-e-portrait.png")
            fresh("?page=1")
            capture("tune.png")
            fresh("?page=2")
            capture("bike.png")
            fresh("?page=3")
            capture("settings-410x502.png")
            fresh("?page=4")
            capture("connections-settings.png")
            page.evaluate("window.wheelieFunDisplay.invoke('wifi-manual')")
            capture("wifi-keyboard.png")
            fresh("?page=5")
            capture("updates.png")
            fresh("?page=6")
            capture("about.png")
            fresh()
            page.evaluate("window.wheelieFunDisplay.show_pairing_demo()")
            assert page.evaluate("window.wheelieFunDisplay.state('pairing-open')")
            capture("ble-pairing.png")
            fresh("?page=2")
            page.evaluate("window.wheelieFunDisplay.invoke('pin-setup')")
            assert page.evaluate("window.wheelieFunDisplay.state('pin-open')")
            assert (
                "simulator"
                not in page.evaluate(
                    "window.wheelieFunDisplay.state('pin-message')"
                ).lower()
            )
            capture("pin-entry.png")
            assert not errors, "Browser errors: " + "\n".join(errors)
            browser.close()
    finally:
        server.shutdown()
        server.server_close()
    # Commit a manifest only after every capture succeeds, so the docs builder
    # cannot silently publish a mixture of old and newly rendered pictures.
    (args.output / "manifest.json").write_text(
        json.dumps(
            {
                "renderer": "shared Slint software renderer",
                "fixture": "illustrative connected values; no physical device connected",
                "images": images,
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
        newline="\n",
    )
    print(
        f"Generated {len(images)} current-UI documentation screenshots in {args.output}"
    )


if __name__ == "__main__":
    main()
