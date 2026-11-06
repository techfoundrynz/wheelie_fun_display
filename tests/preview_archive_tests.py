"""Archive containment must be checked before privileged preview extraction."""

from pathlib import Path
import stat
import sys
import tempfile
import unittest
from zipfile import ZipFile, ZipInfo

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from download_preview_artifact import extract_preview


class PreviewArchiveTests(unittest.TestCase):
    def test_normal_assets_round_trip(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with ZipFile(root / "preview.zip", "w") as bundle:
                bundle.writestr("simulator/index.html", "preview")
                bundle.writestr("simulator/pkg/app.wasm", b"\x00asm")
            extract_preview(root / "preview.zip", root / "out")
            self.assertEqual(
                (root / "out/simulator/pkg/app.wasm").read_bytes(), b"\x00asm"
            )

    def test_traversal_is_rejected_before_even_safe_entries_are_written(self):
        for name in (
            "../scripts/publish_pages.py",
            "/etc/config",
            "C:/config",
            "..\\config",
            ".git/config",
            "a/../../config",
        ):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                with ZipFile(root / "preview.zip", "w") as bundle:
                    bundle.writestr("safe.txt", "safe")
                    bundle.writestr(name, "unsafe")
                with self.assertRaises(ValueError):
                    extract_preview(root / "preview.zip", root / "out")
                self.assertFalse((root / "out").exists())

    def test_symlink_entries_are_rejected_on_every_platform(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            link = ZipInfo("link.txt")
            link.create_system = 3
            link.external_attr = (stat.S_IFLNK | 0o777) << 16
            with ZipFile(root / "preview.zip", "w") as bundle:
                bundle.writestr(link, "../outside")
            with self.assertRaises(ValueError):
                extract_preview(root / "preview.zip", root / "out")


if __name__ == "__main__":
    unittest.main()
