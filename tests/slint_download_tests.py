"""Exercise Slint's actual CMake downloader against controlled HTTP failures."""

import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import shutil
import subprocess
import tempfile
import threading
import unittest


@unittest.skipUnless(shutil.which("cmake"), "CMake is required")
class SlintDownloadTests(unittest.TestCase):
    def download(self, statuses, checksum=None):
        payload = b"pinned Slint release fixture"
        requests = []

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                status = statuses[min(len(requests), len(statuses) - 1)]
                requests.append(status)
                self.send_response(status)
                self.end_headers()
                self.wfile.write(payload if status == 200 else b"temporary error")

            def log_message(self, *_):
                pass

        server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        root = Path(directory.name)
        destination = root / "asset"
        helper = (
            Path(__file__).resolve().parents[1]
            / "firmware/components/slint_runtime/download.cmake"
        )
        digest = checksum or hashlib.sha256(payload).hexdigest()
        script = root / "download.cmake"
        script.write_text(
            f'include("{helper.as_posix()}")\n'
            f'slint_download("http://127.0.0.1:{server.server_port}/asset" '
            f'"{destination.as_posix()}" "{digest}")\n',
            encoding="utf-8",
        )
        result = subprocess.run(
            ["cmake", "-P", str(script)], capture_output=True, text=True, timeout=45
        )
        self.assertFalse(destination.with_suffix(".tmp").exists())
        return result, requests, destination, payload

    def test_gateway_failures_retry_and_preserve_verified_bytes(self):
        result, requests, destination, payload = self.download([504, 503, 200])
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(requests, [504, 503, 200])
        self.assertEqual(destination.read_bytes(), payload)

    def test_retry_limit_reports_http_status_without_caching_error(self):
        result, requests, destination, _ = self.download([503])
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(requests, [503] * 4)
        self.assertIn("HTTP/1.0 503", result.stderr)
        self.assertIn("4 attempt(s)", result.stderr)
        self.assertFalse(destination.exists())

    def test_missing_asset_fails_without_retrying(self):
        result, requests, destination, _ = self.download([404])
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(requests, [404])
        self.assertIn("HTTP/1.0 404", result.stderr)
        self.assertFalse(destination.exists())

    def test_wrong_checksum_is_never_cached(self):
        result, requests, destination, _ = self.download([200], "0" * 64)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(requests, [200])
        self.assertIn("checksum mismatch", result.stderr)
        self.assertFalse(destination.exists())


if __name__ == "__main__":
    unittest.main()
