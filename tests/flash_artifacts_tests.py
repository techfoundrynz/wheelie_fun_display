"""Check cross-chip flash layouts and browser installer integrity boundaries."""

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import build_flash_site as site
import flash_artifacts as artifacts


def build_metadata(root, chip="esp32p4"):
    roles = {
        "bootloader": (
            "0x2000" if chip == "esp32p4" else "0x0",
            "bootloader/bootloader.bin",
        ),
        "partition-table": ("0x8000", "partition_table/partition-table.bin"),
        "otadata": ("0x10000", "ota_data_initial.bin"),
        "app": ("0x20000", "project-name.bin"),
    }
    metadata = {
        "extra_esptool_args": {"chip": chip},
        "flash_settings": {
            "flash_mode": "qio",
            "flash_size": "16MB",
            "flash_freq": "80m",
        },
        "flash_files": {offset: filename for offset, filename in roles.values()},
    }
    for role, (offset, filename) in roles.items():
        metadata[role] = {"offset": offset, "file": filename, "encrypted": "false"}
        (root / artifacts.ROLE_FILES[role]).write_bytes(role.encode() * 16)
    (root / "flasher_args.json").write_text(json.dumps(metadata), encoding="utf-8")
    return metadata


class FlashLayoutTests(unittest.TestCase):
    def test_factory_merge_uses_native_layout_and_patches_qio_for_web_tools(self):
        for chip, boot_offset in (("esp32s3", "0x0"), ("esp32p4", "0x2000")):
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                build_metadata(root, chip)

                def merge(command, *, check, chip=chip, boot_offset=boot_offset):
                    self.assertTrue(check)
                    self.assertEqual(command[command.index("--chip") + 1], chip)
                    self.assertEqual(command[command.index("--flash-mode") + 1], "dio")
                    self.assertEqual(command[command.index("--flash-size") + 1], "16MB")
                    self.assertEqual(command[-8], boot_offset)
                    self.assertEqual(command[-2], "0x20000")
                    Path(command[command.index("--output") + 1]).write_bytes(b"merged")

                with patch.object(artifacts.subprocess, "run", side_effect=merge):
                    item = artifacts.stage_factory(
                        root, root, "test_board", "Board", "1.2.3"
                    )
                self.assertEqual(item["chip_family"], artifacts.CHIP_FAMILIES[chip])
                self.assertEqual(item["sha256"], hashlib.sha256(b"merged").hexdigest())

    def test_native_bootloader_offsets_and_pio_app_fallback(self):
        for chip, offset in (("esp32s3", 0), ("esp32p4", 0x2000)):
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                build_metadata(root, chip)
                actual_chip, settings, parts = artifacts.flash_layout(root)
                self.assertEqual(actual_chip, chip)
                self.assertEqual(parts[0][0], offset)
                self.assertEqual(parts[-1], (0x20000, root / "firmware.bin"))
                self.assertEqual(settings["flash_size"], "16MB")

    def test_missing_images_and_inconsistent_metadata_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            metadata = build_metadata(root)
            (root / "firmware.bin").unlink()
            with self.assertRaisesRegex(ValueError, "Missing app"):
                artifacts.flash_layout(root)
            (root / "firmware.bin").write_bytes(b"application")
            metadata["app"]["offset"] = "0x8000"
            (root / "flasher_args.json").write_text(
                json.dumps(metadata), encoding="utf-8"
            )
            with self.assertRaisesRegex(ValueError, "Inconsistent"):
                artifacts.flash_layout(root)

    def test_traversal_and_overlap_cannot_enter_a_factory_image(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            metadata = build_metadata(root)
            metadata["app"]["file"] = "../outside.bin"
            metadata["flash_files"]["0x20000"] = "../outside.bin"
            (root / "flasher_args.json").write_text(
                json.dumps(metadata), encoding="utf-8"
            )
            with self.assertRaisesRegex(ValueError, "escapes"):
                artifacts.flash_layout(root)
            build_metadata(root)
            (root / "bootloader.bin").write_bytes(b"x" * 0x7000)
            with self.assertRaisesRegex(ValueError, "Overlapping"):
                artifacts.flash_layout(root)

    def test_unsupported_or_encrypted_targets_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            metadata = build_metadata(root)
            metadata["app"]["encrypted"] = "true"
            (root / "flasher_args.json").write_text(
                json.dumps(metadata), encoding="utf-8"
            )
            with self.assertRaisesRegex(ValueError, "Encrypted"):
                artifacts.flash_layout(root)
            build_metadata(root, "unrelated-chip")
            with self.assertRaisesRegex(ValueError, "Unsupported installer chip"):
                artifacts.flash_layout(root)


class BrowserManifestTests(unittest.TestCase):
    def installer(
        self, environment="board_p4", chip="ESP32-P4", content=b"factory test image"
    ):
        return {
            "environment": environment,
            "label": "Test display",
            "chip_family": chip,
            "version": "1.2.3",
            "file": environment + "-factory.bin",
            "size": len(content),
            "sha256": hashlib.sha256(content).hexdigest(),
        }

    def test_each_manifest_has_only_its_selected_chip_and_merged_offset(self):
        content = b"factory test image"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for environment, family in (
                ("board_s3", "ESP32-S3"),
                ("board_p4", "ESP32-P4"),
            ):
                item = self.installer(environment, family, content)
                staged = site.validate_installer(
                    item, lambda _: content, root / "firmware"
                )
                manifest = json.loads(
                    (root / staged["manifest"]).read_text(encoding="utf-8")
                )
                self.assertEqual(
                    manifest["builds"],
                    [
                        {
                            "chipFamily": family,
                            "parts": [{"path": item["file"], "offset": 0}],
                        }
                    ],
                )
                self.assertTrue(manifest["new_install_prompt_erase"])
                self.assertEqual(manifest["new_install_improv_wait_time"], 0)
                self.assertIn(item["sha256"], staged["manifest"])

    def test_corruption_or_wrong_version_never_produces_an_available_catalog(self):
        content = b"factory test image"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            item = self.installer(content=content)
            with self.assertRaisesRegex(ValueError, "SHA256"):
                site.validate_installer(item, lambda _: b"corrupt", root)
            self.assertFalse(list(root.rglob("*.json")))
            with (
                patch.object(site, "environments", return_value=["board_p4"]),
                patch.object(site, "option", return_value="Test"),
                self.assertRaisesRegex(ValueError, "versions differ"),
            ):
                site.stage_installers(
                    root,
                    {"version": "9.9.9", "installers": [item]},
                    lambda _: content,
                    channel="release",
                )
            self.assertFalse((root / "catalog.json").exists())

    def test_first_release_and_missing_board_are_explicitly_unavailable(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with (
                patch.object(site, "environments", return_value=["board_p4"]),
                patch.object(site, "option", return_value="Test display"),
            ):
                result = site.stage_installers(
                    root,
                    {},
                    lambda _: self.fail("No asset should be read"),
                    channel="release",
                )
            self.assertFalse(result["boards"][0]["available"])
            self.assertIsNone(result["version"])


class CatalogRefreshTests(unittest.TestCase):
    def test_refresh_keeps_tested_ui_vendor_and_target_list(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "site/install"
            (output / "vendor").mkdir(parents=True)
            (output / "firmware").mkdir()
            (output / "index.html").write_text("tested UI", encoding="utf-8")
            (output / "vendor/tool.js").write_text("tested vendor", encoding="utf-8")
            (output / "firmware/old.bin").write_bytes(b"unpublished")
            (output / "catalog.json").write_text(
                json.dumps(
                    {
                        "boards": [
                            {"environment": "tested_board", "label": "Tested target"}
                        ]
                    }
                ),
                encoding="utf-8",
            )
            with (
                patch.object(site, "ROOT", root),
                patch.object(
                    sys,
                    "argv",
                    [
                        "build_flash_site.py",
                        "--site",
                        str(root / "site"),
                        "--github-release",
                        "--catalog-only",
                    ],
                ),
                patch.object(
                    site, "github_release", return_value=({}, None, lambda _: b"")
                ),
                patch.object(site, "stage_web_tools") as vendor,
            ):
                site.main()
            vendor.assert_not_called()
            self.assertEqual((output / "index.html").read_text(), "tested UI")
            self.assertEqual((output / "vendor/tool.js").read_text(), "tested vendor")
            self.assertFalse((output / "firmware/old.bin").exists())
            catalog = json.loads((output / "catalog.json").read_text())
            self.assertEqual(
                [item["environment"] for item in catalog["boards"]], ["tested_board"]
            )
            self.assertEqual(catalog["channel"], "release")


if __name__ == "__main__":
    unittest.main()
