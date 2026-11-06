"""Exercise the hosted PR wrapper with a real opaque-origin WASM frame."""

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import re
import sys
import tempfile
import threading
import time

from playwright.sync_api import expect, sync_playwright

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from publish_pages import assemble


class PagesHandler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path.endswith("/report/"):
            markup = (Path(self.translate_path(self.path)) / "index.html").read_bytes()
            split = markup.index(b"<iframe")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(markup)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            # Commit navigation before the iframe exists, reproducing the CI race.
            self.wfile.write(markup[:split])
            self.wfile.flush()
            time.sleep(0.25)
            self.wfile.write(markup[split:])
            return
        super().do_GET()

    def log_message(self, *_):
        pass

    def end_headers(self):
        # GitHub Pages serves these headers too; module/fonts fetch from null origin.
        self.send_header("Access-Control-Allow-Origin", "*")
        super().end_headers()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--site", type=Path, required=True)
    parser.add_argument("--browser-executable")
    args = parser.parse_args()
    source = args.site.resolve()
    for required in ("simulator/index.html", "ci/index.html"):
        if not (source / required).is_file():
            parser.error(
                f"Build the simulator and package its browser gallery first: missing {required}"
            )
    output = Path(".cache/pages/preview-browser-tests")
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as directory:
        site = Path(directory)
        assemble(site, source)
        destination = "previews/pr-12/" + "a" * 40
        assemble(site, source, destination)
        server = ThreadingHTTPServer(
            ("127.0.0.1", 0), partial(PagesHandler, directory=str(site))
        )
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            with sync_playwright() as playwright:
                browser = playwright.chromium.launch(
                    executable_path=args.browser_executable
                )
                for width in (1440, 390):
                    page = browser.new_page(viewport={"width": width, "height": 1000})
                    errors = []
                    page.on("pageerror", lambda error: errors.append(str(error)))
                    page.goto(f"http://127.0.0.1:{server.server_port}/{destination}/")
                    preview = page.frame_locator('iframe[title="PR simulator preview"]')
                    expect(preview.locator("#status")).to_have_text(
                        re.compile(r"^Ready"), timeout=30_000
                    )
                    # Readiness above guarantees an attached browsing context.
                    frame = (
                        page.locator('iframe[title="PR simulator preview"]')
                        .element_handle()
                        .content_frame()
                    )
                    assert frame is not None
                    frame.wait_for_function(
                        "window.wheelieFunDisplay && document.querySelector('#status').textContent.startsWith('Ready')"
                    )
                    assert frame.evaluate(
                        "() => { try { return !parent.document; } catch (error) { return error.name === 'SecurityError'; } }"
                    )
                    assert frame.evaluate(
                        "async () => { try { await navigator.serial.getPorts(); return false; } catch (error) { return ['SecurityError', 'TypeError'].includes(error.name); } }"
                    )
                    canvas = frame.locator("#display")
                    canvas.scroll_into_view_if_needed()
                    bounds = canvas.bounding_box()
                    w, h = canvas.evaluate("node => [node.width, node.height]")
                    canvas.click(
                        position={
                            "x": (w - 56) * bounds["width"] / w,
                            "y": 56 * bounds["height"] / h,
                        }
                    )
                    frame.wait_for_function(
                        "String(window.wheelieFunDisplay.state('page')) === '3'"
                    )
                    page.screenshot(path=str(output / f"{width}-hosted-simulator.png"))
                    page.get_by_role("link", name="Browser test gallery").click()
                    preview.get_by_role("heading", name="Touchscreen checks").wait_for()
                    page.screenshot(path=str(output / f"{width}-hosted-gallery.png"))
                    page.get_by_role("link", name="Simulator", exact=True).click()
                    expect(preview.locator("#status")).to_have_text(
                        re.compile(r"^Ready"), timeout=30_000
                    )
                    assert not errors, errors
                    page.close()
                browser.close()
        finally:
            server.shutdown()
            server.server_close()
            thread.join()
    print(
        "Hosted previews: WASM, touch, report link and parent/serial isolation passed at desktop/mobile widths"
    )


if __name__ == "__main__":
    main()
