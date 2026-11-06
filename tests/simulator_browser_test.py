"""Exercise the actual browser/WASM touchscreen, including project Pages paths."""

from __future__ import annotations

import argparse
import json
import threading
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

from playwright.sync_api import sync_playwright


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--site", type=Path, default=Path(".cache/pages/site"))
    parser.add_argument(
        "--output", type=Path, default=Path(".cache/pages/browser-tests")
    )
    parser.add_argument("--browser-executable")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "results.json").unlink(missing_ok=True)
    site = args.site.resolve()
    assert (site / "simulator/pkg/wheelie_fun_display_bg.wasm").is_file(), (
        "Build the WASM simulator first"
    )

    class Handler(SimpleHTTPRequestHandler):
        def translate_path(self, path):
            # A project Pages deployment is below a repository prefix.
            if path.startswith("/wheelie_fun_display/"):
                path = path.removeprefix("/wheelie_fun_display")
            return super().translate_path(path)

        def log_message(self, *_):
            pass

    server = ThreadingHTTPServer(
        ("127.0.0.1", 0), partial(Handler, directory=str(site))
    )
    threading.Thread(target=server.serve_forever, daemon=True).start()
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
            page.on(
                "console",
                lambda message: (
                    errors.append(message.text) if message.type == "error" else None
                ),
            )
            for prefix in ["/", "/wheelie_fun_display/"]:
                response = page.goto(
                    f"http://127.0.0.1:{server.server_port}{prefix}simulator/"
                )
                assert response and response.ok
                page.wait_for_function(
                    "window.wheelieFunDisplay && document.querySelector('#status').textContent.startsWith('Ready')"
                )
                assert not page.evaluate(
                    "window.wheelieFunDisplay.state('documentation-fixture')"
                )
                assert "Demo" in page.evaluate(
                    "window.wheelieFunDisplay.state('connection')"
                )
                assert "simulated" in page.locator(".notice").inner_text().lower()
                installer_link = page.get_by_role(
                    "link", name="Install firmware"
                ).get_attribute("href")
                from urllib.parse import urljoin

                assert page.request.get(urljoin(page.url, installer_link)).ok
            canvas = page.locator("#display")

            def state(name):
                return page.evaluate(
                    "name => window.wheelieFunDisplay.state(name)", name
                )

            def tap(x, y):
                canvas.scroll_into_view_if_needed()
                bounds = canvas.bounding_box()
                dimensions = canvas.evaluate("canvas => [canvas.width, canvas.height]")
                page.mouse.click(
                    bounds["x"] + x * bounds["width"] / dimensions[0],
                    bounds["y"] + y * bounds["height"] / dimensions[1],
                )
                page.wait_for_timeout(40)

            def swipe(x0, y0, x1, y1):
                canvas.scroll_into_view_if_needed()
                bounds = canvas.bounding_box()
                width, height = canvas.evaluate(
                    "canvas => [canvas.width, canvas.height]"
                )
                point = lambda x, y: (
                    bounds["x"] + x * bounds["width"] / width,
                    bounds["y"] + y * bounds["height"] / height,
                )
                page.mouse.move(*point(x0, y0))
                page.mouse.down()
                page.mouse.move(*point(x1, y1), steps=10)
                page.wait_for_timeout(40)
                page.mouse.up()
                page.wait_for_timeout(330)

            assert canvas.evaluate("canvas => canvas.width") == 410
            page.evaluate("window.scrollTo(0, 0)")
            page.screenshot(path=str(args.output / "desktop-site.png"), full_page=True)
            canvas.screenshot(path=str(args.output / "ride-portrait.png"))
            page.locator("#ride").click()
            page.wait_for_timeout(100)
            assert state("speed") == "24.0"
            page.locator("#stop").click()
            tap(205, 445)
            page.wait_for_timeout(300)
            assert state("page") == 1
            swipe(300, 200, 155, 200)
            assert state("page") == 2
            swipe(120, 200, 290, 200)
            assert state("page") == 1
            tap(354, 56)
            page.wait_for_timeout(300)
            assert state("page") == 3
            tap(200, 262)
            assert state("metric") is False
            tap(200, 334)
            assert state("celsius") is False
            assert state("motor") == "108°F"
            page.locator("#orientation").click()
            page.wait_for_timeout(200)
            assert state("landscape") is True
            assert canvas.evaluate("canvas => [canvas.width, canvas.height]") == [
                502,
                410,
            ]
            page.evaluate("window.wheelieFunDisplay.navigate(0)")
            page.wait_for_timeout(300)
            tap(446, 56)
            page.wait_for_timeout(300)
            assert state("page") == 3, "Landscape touch must align with the cog"
            page.evaluate("window.wheelieFunDisplay.navigate(0)")
            page.wait_for_timeout(300)
            canvas.screenshot(path=str(args.output / "ride-landscape.png"))
            for watts, filename in [
                (1033, "power-1033.png"),
                (12000, "power-12000.png"),
                (-12000, "power-regen.png"),
            ]:
                page.evaluate(
                    "watts => window.wheelieFunDisplay.set_control('power', watts)",
                    watts,
                )
                page.wait_for_timeout(100)
                assert state("power") == f"{watts} W"
                canvas.screenshot(path=str(args.output / filename))
            page.evaluate("window.wheelieFunDisplay.set_control('power', 0)")
            for degrees, dimensions in [
                (180, [410, 502]),
                (270, [502, 410]),
                (0, [410, 502]),
            ]:
                page.locator("#orientation").click()
                page.wait_for_timeout(100)
                assert state("rotation") == degrees
                assert (
                    canvas.evaluate("canvas => [canvas.width, canvas.height]")
                    == dimensions
                )
                page.evaluate("window.wheelieFunDisplay.navigate(0)")
                page.wait_for_timeout(300)
                tap(dimensions[0] - 56, 56)
                page.wait_for_timeout(300)
                assert state("page") == 3, f"Cog touch must align at {degrees} degrees"
            page.locator("#integration").select_option("2")
            page.wait_for_timeout(100)
            assert state("brand") == "wheel-e"
            page.evaluate("window.wheelieFunDisplay.navigate(1)")
            page.wait_for_timeout(300)
            canvas.screenshot(path=str(args.output / "wheel-e-tune.png"))
            # PIN entry is real shared Slint UI; only pairing/bonding is simulated.
            page.locator("#pairing-demo").click()
            page.wait_for_timeout(100)
            assert state("pairing-open") is True
            canvas.screenshot(path=str(args.output / "pairing.png"))
            for digit in range(1, 7):
                tap(88 + ((digit - 1) % 3) * 117, 203 + ((digit - 1) // 3) * 54)
            assert state("pairing-mask") == "******"
            tap(322, 365)
            assert state("pairing-open") is False
            page.evaluate("window.wheelieFunDisplay.invoke('pin-setup')")
            page.wait_for_timeout(100)
            for _ in range(2):
                for digit in range(1, 7):
                    tap(88 + ((digit - 1) % 3) * 117, 203 + ((digit - 1) // 3) * 54)
                tap(322, 365)
            assert state("pin-configured") is True and state("pin-open") is False
            page.evaluate("window.wheelieFunDisplay.invoke('screen-lock')")
            page.wait_for_timeout(100)
            assert state("screen-locked") is True
            tap(354, 56)
            assert state("screen-locked") is True
            for digit in range(1, 7):
                tap(88 + ((digit - 1) % 3) * 117, 203 + ((digit - 1) // 3) * 54)
            tap(322, 365)
            assert state("screen-locked") is False
            page.evaluate("window.wheelieFunDisplay.invoke('edit-proxy-name')")
            page.wait_for_timeout(100)
            assert state("editing") is True and state("editor-value") == "WB Disp"
            page.evaluate("window.wheelieFunDisplay.invoke('editor-backspace')")
            page.evaluate("window.wheelieFunDisplay.invoke('editor-done')")
            assert state("editing") is False and state("proxy-name") == "WB Dis"
            page.set_viewport_size({"width": 390, "height": 844})
            page.wait_for_timeout(100)
            assert page.evaluate(
                "document.documentElement.scrollWidth <= window.innerWidth"
            )
            page.evaluate("document.activeElement?.blur(); window.scrollTo(0, 0)")
            page.wait_for_timeout(150)
            page.screenshot(path=str(args.output / "mobile-site.png"), full_page=True)
            mobile = browser.new_context(
                viewport={"width": 390, "height": 844}, is_mobile=True, has_touch=True
            )
            touch_page = mobile.new_page()
            touch_page.on("pageerror", lambda error: errors.append(str(error)))
            touch_page.goto(
                f"http://127.0.0.1:{server.server_port}/wheelie_fun_display/simulator/"
            )
            touch_page.wait_for_function(
                "window.wheelieFunDisplay && document.querySelector('#status').textContent.startsWith('Ready')"
            )
            touch_canvas = touch_page.locator("#display")
            touch_canvas.scroll_into_view_if_needed()
            bounds = touch_canvas.bounding_box()
            touch_page.touchscreen.tap(
                bounds["x"] + 354 * bounds["width"] / 410,
                bounds["y"] + 56 * bounds["height"] / 502,
            )
            touch_page.wait_for_timeout(300)
            assert touch_page.evaluate("window.wheelieFunDisplay.state('page')") == 3
            mobile.close()
            assert not errors, "Browser errors: " + "\n".join(errors)
            browser.close()
    finally:
        server.shutdown()
        server.server_close()
    results = {
        "passed": True,
        "checks": [
            "Root and project-prefix routes",
            "Shared UI software rendering",
            "Telemetry controls",
            "Ride/Tune/Bike swipes",
            "Four-angle rotation and touch mapping",
            "Independent speed and temperature units",
            "Upstream BLE pairing PIN",
            "Bike PIN setup and screen unlock",
            "Proxy name keyboard",
            "Responsive mobile and touch",
        ],
        "screenshots": [
            "desktop-site.png",
            "ride-portrait.png",
            "ride-landscape.png",
            "power-1033.png",
            "power-12000.png",
            "power-regen.png",
            "wheel-e-tune.png",
            "pairing.png",
            "mobile-site.png",
        ],
    }
    (args.output / "results.json").write_text(
        json.dumps(results, indent=2) + "\n", encoding="utf-8", newline="\n"
    )
    print(
        "Browser simulator: project path, rendering, telemetry, swipes, touch rotation, units, pairing, PIN and responsive shell passed"
    )


if __name__ == "__main__":
    main()
