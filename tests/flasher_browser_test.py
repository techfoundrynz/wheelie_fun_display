"""Exercise the browser installer without requesting or opening any USB port."""

import argparse
import hashlib
import json
import os
import threading
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

from playwright.sync_api import sync_playwright


class Handler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if not self.path.startswith("/wheelie_fun_display/"):
            self.send_error(404)
            return
        self.path = self.path[len("/wheelie_fun_display") :]
        super().do_GET()

    def log_message(self, *_):
        pass


def check_staged_assets(site):
    root = (site / "install").resolve()
    catalog = json.loads((root / "catalog.json").read_text(encoding="utf-8"))
    assert catalog["boards"], (
        "Installer must list configured boards even before the first release"
    )
    for board in catalog["boards"]:
        if not board["available"]:
            continue
        manifest_path = (root / board["manifest"]).resolve()
        image = (root / board["download"]).resolve()
        assert manifest_path.is_relative_to(root) and image.is_relative_to(root)
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        assert manifest["version"] == board["version"]
        assert manifest["new_install_prompt_erase"] is True
        assert len(manifest["builds"]) == 1
        build = manifest["builds"][0]
        assert build["chipFamily"] == board["chip_family"]
        assert build["parts"] == [{"path": image.name, "offset": 0}]
        assert image.stat().st_size == board["size"]
        assert hashlib.sha256(image.read_bytes()).hexdigest() == board["sha256"]


def browser_checks(site, executable, output):
    output.mkdir(parents=True, exist_ok=True)
    server = ThreadingHTTPServer(
        ("127.0.0.1", 0), partial(Handler, directory=str(site))
    )
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    base = f"http://127.0.0.1:{server.server_port}/wheelie_fun_display/install/"
    boards = [
        {
            "environment": "board_s3",
            "label": "Waveshare ESP32-S3 AMOLED 2.06",
            "chip_family": "ESP32-S3",
        },
        {
            "environment": "board_p4",
            "label": "GUITION JC4880P443C 4.3",
            "chip_family": "ESP32-P4",
            "compatibility": "For ESP32-P4 revisions below 3.0.",
        },
    ]
    for board in boards:
        board.update(
            {
                "available": True,
                "version": "1.2.3",
                "manifest": "firmware/" + board["environment"] + ".json",
                "download": "firmware/" + board["environment"] + "-factory.bin",
                "sha256": "a" * 64,
            }
        )
    catalog = {
        "version": "1.2.3",
        "channel": "release",
        "release_url": "https://github.com/techfoundrynz/wheelie_fun_display/releases/tag/v1.2.3",
        "boards": boards,
    }
    try:
        with sync_playwright() as playwright:
            launch = {"headless": True}
            if executable:
                launch["executable_path"] = executable
            browser = playwright.chromium.launch(**launch)
            for width in (1440, 390):
                context = browser.new_context(
                    viewport={"width": width, "height": 1000}, device_scale_factor=1
                )
                # This substitute cannot access hardware even if the page regresses.
                context.add_init_script(
                    "window.usbRequests = 0; Object.defineProperty(navigator, 'serial', { configurable: true, value: {requestPort: () => { window.usbRequests++; throw new Error('USB disabled in browser QA'); }}});"
                )
                page = context.new_page()
                errors = []
                page.on(
                    "pageerror", lambda error, errors=errors: errors.append(str(error))
                )
                page.route(
                    "**/install/catalog.json", lambda route: route.fulfill(json=catalog)
                )
                page.goto(base)
                page.wait_for_function(
                    "customElements.get('esp-web-install-button') && !document.querySelector('#install-action').disabled"
                )
                assert page.title() == "Install firmware · Wheelie Fun Display"
                assert (
                    page.locator("#installer")
                    .get_attribute("manifest")
                    .endswith("/firmware/board_s3.json")
                )
                assert page.locator("#firmware-chip").inner_text() == "ESP32-S3"
                assert (
                    page.locator("#firmware-model").inner_text() == boards[0]["label"]
                )
                assert page.locator("#firmware-version").inner_text() == "1.2.3"
                assert page.evaluate(
                    "getComputedStyle(document.body).fontFamily"
                ).startswith("Roboto")
                assert page.evaluate(
                    "document.documentElement.scrollWidth <= innerWidth"
                ), "Horizontal overflow"
                page.screenshot(
                    path=str(output / f"{width}-install.png"), full_page=True
                )
                page.locator("#board-select").select_option("board_p4")
                assert page.locator("#firmware-chip").inner_text() == "ESP32-P4"
                assert page.locator("#compatibility").is_visible()
                assert (
                    page.locator("#installer")
                    .get_attribute("manifest")
                    .endswith("/firmware/board_p4.json")
                )
                assert page.evaluate("window.usbRequests") == 0
                assert not errors, errors
                context.close()

            context = browser.new_context(viewport={"width": 390, "height": 900})
            context.add_init_script(
                "window.usbRequests = 0; Object.defineProperty(navigator, 'serial', { configurable: true, value: {requestPort: () => { window.usbRequests++; throw new Error('USB disabled in browser QA'); }}});"
            )
            page = context.new_page()
            empty = {
                "version": None,
                "channel": "release",
                "release_url": None,
                "boards": [
                    {
                        "environment": "board_s3",
                        "label": "Waveshare ESP32-S3 AMOLED 2.06",
                        "available": False,
                    }
                ],
            }
            page.route(
                "**/install/catalog.json", lambda route: route.fulfill(json=empty)
            )
            page.goto(base)
            page.wait_for_function(
                "document.querySelector('#firmware-status').textContent.includes('No firmware installer')"
            )
            assert page.locator("#install-available").is_hidden()
            assert page.locator("#install-action").is_disabled()
            assert page.evaluate("window.usbRequests") == 0
            page.screenshot(path=str(output / "390-no-release.png"), full_page=True)
            context.close()

            context = browser.new_context(viewport={"width": 1440, "height": 1000})
            context.add_init_script("delete Object.getPrototypeOf(navigator).serial;")
            page = context.new_page()
            page.route(
                "**/install/catalog.json", lambda route: route.fulfill(json=catalog)
            )
            page.goto(base)
            page.wait_for_function(
                "document.querySelector('#installer').hasAttribute('install-unsupported')"
            )
            assert page.locator("[slot=unsupported]").is_visible()
            assert "Web Serial" in page.locator("[slot=unsupported]").inner_text()
            context.close()

            context = browser.new_context(viewport={"width": 390, "height": 900})
            context.add_init_script(
                "Object.defineProperty(window, 'isSecureContext', {value: false});"
                "Object.defineProperty(navigator, 'serial', {value: {requestPort: () => {throw new Error('USB disabled in browser QA');}}});"
            )
            page = context.new_page()
            page.route(
                "**/install/catalog.json", lambda route: route.fulfill(json=catalog)
            )
            page.goto(base)
            page.wait_for_function("!!customElements.get('esp-web-install-button')")
            assert page.locator("[slot=not-allowed]").is_visible()
            assert (
                "HTTPS or localhost" in page.locator("[slot=not-allowed]").inner_text()
            )
            context.close()
            browser.close()
            print(
                "Flasher browser checks: board manifests, versions, shared styling, desktop/mobile, empty release, unsupported browser and secure-context guidance passed; zero USB requests"
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
        "--output", type=Path, default=Path(".cache/pages/flasher-browser-tests")
    )
    args = parser.parse_args()
    check_staged_assets(args.site)
    browser_checks(args.site.resolve(), args.browser_executable, args.output.resolve())


if __name__ == "__main__":
    main()
