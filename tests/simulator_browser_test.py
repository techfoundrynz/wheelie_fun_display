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
            manifest = site / "docs-manifest.json"
            if (
                manifest.is_file()
                and json.loads(manifest.read_text(encoding="utf-8")).get("framework")
                == "tanstack-start"
            ):
                for width in (1440, 390):
                    loading = browser.new_page(
                        viewport={"width": width, "height": 1050}
                    )
                    pending = []
                    loading.route("**/*_bg.wasm", lambda route: pending.append(route))
                    with loading.expect_request("**/*_bg.wasm"):
                        loading.goto(
                            f"http://127.0.0.1:{server.server_port}/wheelie_fun_display/simulator/",
                            wait_until="domcontentloaded",
                        )
                    loading.evaluate("document.fonts.ready")
                    assert loading.locator("#workspace").is_visible(), (
                        "Simulator space must be reserved before WASM is ready"
                    )
                    before = {
                        selector: loading.locator(selector).bounding_box()
                        for selector in ("#workspace", ".display-frame", ".app-footer")
                    }
                    for request in pending:
                        request.continue_()
                    loading.wait_for_function(
                        "window.wheelieFunDisplay && document.querySelector('#status').textContent.startsWith('Ready')"
                    )
                    for selector, initial in before.items():
                        final = loading.locator(selector).bounding_box()
                        assert (
                            initial
                            and final
                            and all(
                                abs(initial[key] - final[key]) < 1
                                for key in ("x", "y", "width", "height")
                            )
                        ), (
                            f"Layout shifted after simulator startup: {selector} at {width}: {initial} -> {final}"
                        )
                    loading.close()
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
                assert (
                    page.evaluate("window.wheelieFunDisplay.state('connection')")
                    == "Connected"
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

            safe_x = max(20, state("safe-inset-x"))
            safe_y = max(12, state("safe-inset-y"))
            page_top = safe_y + 48 + 8

            def tap(x, y):
                canvas.scroll_into_view_if_needed()
                bounds = canvas.bounding_box()
                dimensions = canvas.evaluate("canvas => [canvas.width, canvas.height]")
                page.mouse.click(
                    bounds["x"] + x * bounds["width"] / dimensions[0],
                    bounds["y"] + y * bounds["height"] / dimensions[1],
                )
                page.wait_for_timeout(40)

            def tap_settings():
                width = canvas.evaluate("canvas => canvas.width")
                tap(width - safe_x - 24, safe_y + 24)

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
            assert state("trip") == "12.34 km"
            assert state("odometer") == "12345.6 km"
            page.locator("#connected").uncheck()
            page.wait_for_function(
                "window.wheelieFunDisplay.state('trip') === '--' && window.wheelieFunDisplay.state('odometer') === '--'"
            )
            page.locator("#connected").check()
            page.wait_for_function(
                "window.wheelieFunDisplay.state('trip') === '12.34 km' && window.wheelieFunDisplay.state('odometer') === '12345.6 km'"
            )
            page.locator("#stop").click()
            tap(205, 502 - safe_y - 25)
            page.wait_for_timeout(300)
            assert state("page") == 1
            swipe(300, 200, 155, 200)
            assert state("page") == 2
            swipe(120, 200, 290, 200)
            assert state("page") == 1
            tap_settings()
            page.wait_for_timeout(300)
            assert state("page") == 3
            page.evaluate("window.wheelieFunDisplay.scroll(205, 250, -80)")
            page.wait_for_timeout(300)
            tap(200, page_top + 282 - 80)
            assert state("metric") is False
            page.wait_for_function(
                "window.wheelieFunDisplay.state('trip') === '7.67 mi' && window.wheelieFunDisplay.state('odometer') === '7671.2 mi'"
            )
            assert state("trip") == "7.67 mi"
            assert state("odometer") == "7671.2 mi"
            tap(200, page_top + 354 - 80)
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
            tap_settings()
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
                tap_settings()
                page.wait_for_timeout(300)
                assert state("page") == 3, f"Cog touch must align at {degrees} degrees"
            page.locator("#integration").select_option("2")
            page.wait_for_timeout(100)
            assert state("brand") == "wheel-e"
            page.evaluate("window.wheelieFunDisplay.navigate(2)")
            page.wait_for_timeout(300)
            canvas.screenshot(path=str(args.output / "wheel-e-tune.png"))
            # PIN entry is real shared Slint UI; only pairing/bonding is simulated.
            page.locator("#pairing-demo").click()
            page.wait_for_timeout(100)
            assert state("pairing-open") is True
            canvas.screenshot(path=str(args.output / "pairing.png"))

            def tap_pin(digit, *, locked=False):
                width, height = canvas.evaluate(
                    "canvas => [canvas.width, canvas.height]"
                )
                inset_x = max(20, state("safe-inset-x"))
                inset_y = max(20, state("safe-inset-y"))
                available_width = width - 2 * inset_x
                available_height = height - 2 * inset_y
                if width > height:
                    left = inset_x + available_width * 0.38 + 16
                    top = inset_y
                    key_width = available_width * 0.62 - 16
                    key_height = available_height
                else:
                    left, top = inset_x, inset_y + 152
                    key_width = available_width
                    key_height = available_height - 152 - 48
                if digit == "backspace":
                    column, row = 0, 3
                elif digit == "clear":
                    column, row = 2, 3
                else:
                    column, row = (
                        ((digit - 1) % 3, (digit - 1) // 3) if digit else (1, 3)
                    )
                cell_width = (key_width - 16) / 3
                cell_height = (key_height - 24) / 4
                tap(
                    left + column * (cell_width + 8) + cell_width / 2,
                    top + row * (cell_height + 8) + cell_height / 2,
                )

            for digit in range(1, 4):
                tap_pin(digit)
            tap_pin("backspace")
            assert state("pairing-value") == "12"
            tap_pin("clear")
            assert state("pairing-value") == ""
            for digit in range(1, 6):
                tap_pin(digit)
            assert state("pairing-value") == "12345"
            assert state("pairing-open") is True
            tap_pin(6)
            assert state("pairing-open") is True
            tap(32 + (410 - 64) * 0.75 + 2, 462)
            assert state("pairing-open") is False
            page.evaluate("window.wheelieFunDisplay.invoke('pin-setup')")
            page.wait_for_timeout(100)
            for _ in range(2):
                for digit in range(1, 7):
                    tap_pin(digit)
            assert state("pin-configured") is True and state("pin-open") is False
            page.evaluate("window.wheelieFunDisplay.invoke('screen-lock')")
            page.wait_for_timeout(100)
            assert state("screen-locked") is True
            tap_settings()
            assert state("screen-locked") is True
            for digit in [0, 9, 8]:
                tap_pin(digit, locked=True)
            tap_pin("clear", locked=True)
            assert state("screen-locked") is True and state("pin-value") == ""
            for digit in [6, 5, 4, 3, 2, 1]:
                tap_pin(digit, locked=True)
            assert state("screen-locked") is True and state("pin-value") == ""
            for digit in range(1, 7):
                tap_pin(digit, locked=True)
            assert state("screen-locked") is False
            page.evaluate("window.wheelieFunDisplay.invoke('pin-setup')")
            assert state("pin-pattern") is False
            for digit in range(1, 7):
                tap_pin(digit)
            # The entry-screen switch changes only how the same credential is entered.
            tap(32 + (410 - 64) * 0.25 - 2, 462)
            assert state("pin-pattern") is True

            def draw_pattern(nodes):
                width, height = canvas.evaluate(
                    "canvas => [canvas.width, canvas.height]"
                )
                inset_x = max(20, state("safe-inset-x"))
                inset_y = max(20, state("safe-inset-y"))
                available_width = width - 2 * inset_x
                available_height = height - 2 * inset_y
                left = inset_x
                top = inset_y + 152
                region_width = available_width
                region_height = available_height - 152 - 48
                if width > height:
                    left = inset_x + available_width * 0.38 + 16
                    top = inset_y
                    region_width = available_width * 0.62 - 16
                    region_height = available_height
                side = min(region_width, region_height)
                left += (region_width - side) / 2
                top += (region_height - side) / 2
                for index, node in enumerate(nodes):
                    x = left + ((node - 1) % 3 + 0.5) * side / 3
                    y = top + ((node - 1) // 3 + 0.5) * side / 3
                    kind = "down" if index == 0 else "move"
                    page.evaluate(
                        "([kind, x, y]) => window.wheelieFunDisplay.pointer(kind, x, y)",
                        [kind, x, y],
                    )
                page.evaluate(
                    "([x, y]) => window.wheelieFunDisplay.pointer('up', x, y)", [x, y]
                )
                page.wait_for_timeout(50)

            draw_pattern([1, 3, 9, 8])
            draw_pattern([1, 3, 9, 8])
            assert state("pin-open") is False and state("unlock-method") == 1
            assert state("proxy-pairing-code") == "123698"
            page.evaluate("window.wheelieFunDisplay.invoke('screen-lock')")
            tap(205, 462)  # Use PIN on the locked entry screen.
            assert state("pin-pattern") is False
            for digit in [1, 2, 3, 6, 9, 8]:
                tap_pin(digit, locked=True)
            assert state("screen-locked") is False
            assert state("proxy-pairing-code") == "123698"
            page.evaluate("window.wheelieFunDisplay.invoke('screen-lock')")
            assert state("pin-pattern") is False, (
                "Entry preference must survive reopening"
            )
            tap(205, 462)
            assert state("pin-pattern") is True
            draw_pattern([1, 2, 3, 4, 5, 6])
            assert state("screen-locked") is True and state("pin-value") == ""
            draw_pattern([1, 3, 9, 8])
            assert state("screen-locked") is False
            assert state("proxy-name-locked") is False
            assert state("proxy-name") == "WB Disp"
            page.evaluate("window.wheelieFunDisplay.invoke('edit-proxy-name')")
            assert state("editing") is True
            page.evaluate("window.wheelieFunDisplay.invoke('editor-backspace')")
            page.evaluate("window.wheelieFunDisplay.invoke('editor-done')")
            assert state("proxy-name") == "WB Dis"
            page.select_option("#integration", "1")
            page.wait_for_timeout(100)
            assert state("proxy-name-locked") is True
            assert state("proxy-name") == "WFB"
            page.evaluate("window.wheelieFunDisplay.invoke('edit-proxy-name')")
            assert state("editing") is False
            assert state("proxy-name-preference") == "WB Dis"
            page.select_option("#integration", "2")
            page.wait_for_timeout(100)
            assert state("proxy-name-locked") is False
            assert state("proxy-name") == "WB Dis"
            page.select_option("#integration", "1")
            page.wait_for_timeout(100)
            assert state("proxy-name") == "WFB"
            assert state("proxy-name-preference") == "WB Dis"
            page.locator("#connected").uncheck()
            page.wait_for_timeout(100)
            assert state("proxy-name-locked") is False, (
                "Auto without a verified bike must remain editable"
            )
            assert state("proxy-name") == "WB Dis"
            page.locator("#connected").check()
            page.wait_for_timeout(100)
            assert state("proxy-name") == "WFB"
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
                bounds["x"] + (410 - safe_x - 24) * bounds["width"] / 410,
                bounds["y"] + (safe_y + 24) * bounds["height"] / 502,
            )
            touch_page.wait_for_timeout(300)
            assert touch_page.evaluate("window.wheelieFunDisplay.state('page')") == 3
            mobile.close()
            # Changing hardware size must survive all four runtime rotations.
            page.goto(
                f"http://127.0.0.1:{server.server_port}/wheelie_fun_display/simulator/"
            )
            page.wait_for_function(
                "window.wheelieFunDisplay && document.querySelector('#status').textContent.startsWith('Ready')"
            )
            page.locator("#display-model").select_option("1")
            for rotation in (0, 90, 180, 270):
                page.evaluate("r => window.wheelieFunDisplay.set_rotation(r)", rotation)
                expected = [800, 480] if rotation in (90, 270) else [480, 800]
                page.wait_for_function(
                    "size => { const c = document.querySelector('#display'); return c.width === size[0] && c.height === size[1]; }",
                    arg=expected,
                )
            page.locator("#display-model").select_option("0")
            page.wait_for_function(
                "document.querySelector('#display').width === 502 && document.querySelector('#display').height === 410"
            )
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
            "Ride/Stats/Tune swipes",
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
