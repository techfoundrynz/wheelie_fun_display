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
                    == "Connected"
                )
                assert "simulated" in page.locator(".notice").inner_text().lower()
                for field in ["feedback", "device-label", "connection"]:
                    value = page.evaluate(
                        "name => window.wheelieFunDisplay.state(name)", field
                    ).lower()
                    assert not any(
                        word in value for word in ["demo", "simulator", "simulated"]
                    ), (field, value)
                if "page=" not in query:
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
            fresh("?display=guition&orientation=landscape")
            capture("ride-guition-landscape.png")
            fresh("?display=guition")
            capture("ride-guition-portrait.png")
            fresh("?display=guition&orientation=landscape&page=1")
            capture("stats-guition-landscape.png")
            fresh("?integration=wheel-e")
            capture("wheel-e-portrait.png")
            fresh("?page=1")
            capture("stats.png")
            page.evaluate("window.wheelieFunDisplay.scroll(205, 250, -1464)")
            capture("stats-bms.png")
            page.evaluate("window.wheelieFunDisplay.scroll(205, 250, -1512)")
            capture("stats-cells.png")
            fresh("?page=2")
            capture("tune.png")
            page.evaluate("window.wheelieFunDisplay.set_tuning_section(1)")
            capture("tune-wheelie.png")
            page.evaluate("window.wheelieFunDisplay.edit_tuning(5)")
            assert page.evaluate("window.wheelieFunDisplay.state('tuning-editor-open')")
            capture("tune-slider.png")
            page.evaluate("window.wheelieFunDisplay.invoke('tuning-editor-done')")
            page.evaluate("window.wheelieFunDisplay.adjust_tuning(5, 1)")
            page.evaluate("window.wheelieFunDisplay.invoke('tuning-save')")
            assert page.evaluate("window.wheelieFunDisplay.state('confirming')")
            capture("tune-confirmation.png")
            page.evaluate("window.wheelieFunDisplay.invoke('cancel-change')")
            fresh("?page=3")
            page.evaluate("window.wheelieFunDisplay.scroll(205, 250, -360)")
            capture("vehicle-settings.png")
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
            fresh("?page=3")
            page.evaluate("window.wheelieFunDisplay.invoke('pin-setup')")
            assert page.evaluate("window.wheelieFunDisplay.state('pin-open')")
            assert (
                "simulator"
                not in page.evaluate(
                    "window.wheelieFunDisplay.state('pin-message')"
                ).lower()
            )
            capture("pin-entry.png")
            fresh("?page=3")
            page.evaluate("window.wheelieFunDisplay.invoke('pin-setup')")
            page.evaluate("window.wheelieFunDisplay.set_control('unlock-method', 1)")
            assert page.evaluate("window.wheelieFunDisplay.state('pin-pattern')")
            # Exercise the real gesture and confirmation flow for these pictures.
            # The documentation fixture is always the 410x502 portrait layout.
            inset_x = max(
                20, page.evaluate("window.wheelieFunDisplay.state('safe-inset-x')")
            )
            inset_y = max(
                20, page.evaluate("window.wheelieFunDisplay.state('safe-inset-y')")
            )
            region_width = 410 - 2 * inset_x
            side = min(region_width, 502 - 2 * inset_y - 152 - 48)
            left = inset_x + (region_width - side) / 2
            top = inset_y + 152
            for confirmation in range(2):
                for index, node in enumerate([1, 3, 9, 8]):
                    x = left + ((node - 1) % 3 + 0.5) * side / 3
                    y = top + ((node - 1) // 3 + 0.5) * side / 3
                    kind = "down" if index == 0 else "move"
                    page.evaluate(
                        "([kind,x,y]) => window.wheelieFunDisplay.pointer(kind,x,y)",
                        [kind, x, y],
                    )
                if confirmation == 0:
                    capture("pattern-entry.png")
                page.evaluate(
                    "([x,y]) => window.wheelieFunDisplay.pointer('up',x,y)", [x, y]
                )
            assert (
                page.evaluate("window.wheelieFunDisplay.state('proxy-pairing-code')")
                == "123698"
            )
            page.evaluate("window.wheelieFunDisplay.scroll(205, 300, -10000)")
            capture("proxy-pairing-code.png")
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
