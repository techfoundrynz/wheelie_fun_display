"""Exercise release gating without publishing tags or firmware."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import release_version as release


class ReleaseVersionTests(unittest.TestCase):
    def test_real_bump_publishes_matching_tag(self):
        plan = release.release_plan("push", "branch", "main", "0.2.0", "0.1.0")
        self.assertTrue(plan["publish"])
        self.assertEqual(plan["tag"], "v0.2.0")

    def test_unchanged_and_decreased_versions_skip(self):
        for current in ("0.1.0", "0.0.9", "0.1.0+build.2"):
            with self.subTest(current=current):
                self.assertFalse(
                    release.release_plan("push", "branch", "main", current, "0.1.0")[
                        "publish"
                    ]
                )

    def test_config_formatting_and_other_options_do_not_change_version(self):
        first = release.configured_version(
            "[common]\ncustom_firmware_version=0.1.0\nother=old\n"
        )
        second = release.configured_version(
            "[common]\n# comment\ncustom_firmware_version = 0.1.0\nother=new\n"
        )
        self.assertFalse(
            release.release_plan("push", "branch", "main", second, first)["publish"]
        )

    def test_prerelease_precedence(self):
        values = [
            "1.0.0-alpha",
            "1.0.0-alpha.1",
            "1.0.0-beta.2",
            "1.0.0-beta.11",
            "1.0.0-rc.1",
            "1.0.0",
        ]
        self.assertEqual(sorted(values, key=release.version_key), values)

    def test_invalid_versions_are_rejected(self):
        for value in (
            "v1.2.3",
            "1.2",
            "01.2.3",
            "1.2.3-01",
            "1.2.3\nother=true",
            "1.2.3-",
            "1.2.3+",
        ):
            with self.subTest(value=value), self.assertRaises(ValueError):
                release.version_key(value)

    def test_manual_run_and_initial_push(self):
        self.assertTrue(
            release.release_plan(
                "workflow_dispatch", "branch", "main", "0.1.0", manual_release=True
            )["publish"]
        )
        self.assertFalse(
            release.release_plan("push", "branch", "main", "0.1.0")["publish"]
        )
        self.assertFalse(
            release.release_plan("push", "branch", "feature", "0.2.0", "0.1.0")[
                "publish"
            ]
        )

    def test_manual_runs_never_publish_without_the_checkbox(self):
        self.assertFalse(
            release.release_plan("workflow_dispatch", "branch", "main", "0.1.0")[
                "publish"
            ]
        )
        self.assertFalse(
            release.release_plan("workflow_dispatch", "tag", "v0.1.0", "0.1.0")[
                "publish"
            ]
        )
        self.assertTrue(
            release.release_plan(
                "workflow_dispatch", "tag", "v0.1.0", "0.1.0", manual_release=True
            )["publish"]
        )

    def test_tag_must_match_config(self):
        self.assertTrue(
            release.release_plan("push", "tag", "v0.2.0", "0.2.0")["publish"]
        )
        with self.assertRaisesRegex(ValueError, "must match"):
            release.release_plan("push", "tag", "v0.3.0", "0.2.0")

    def test_push_base_covers_all_commits_not_just_last_parent(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def git(*args):
                return (
                    subprocess.check_output(
                        ["git", *args], cwd=root, stderr=subprocess.DEVNULL
                    )
                    .decode()
                    .strip()
                )

            git("init")
            git("config", "commit.gpgsign", "false")
            git("config", "user.name", "Release test")
            git("config", "user.email", "release@example.invalid")
            ini = root / "platformio.ini"
            ini.write_text(
                "[common]\ncustom_firmware_version=0.1.0\n", encoding="utf-8"
            )
            git("add", ".")
            git("commit", "-m", "Base")
            before = git("rev-parse", "HEAD")
            ini.write_text(
                "[common]\ncustom_firmware_version=0.2.0\n", encoding="utf-8"
            )
            git("commit", "-am", "Bump")
            (root / "notes.txt").write_text("Later commit", encoding="utf-8")
            git("add", ".")
            git("commit", "-m", "More work")
            with patch.object(release, "ROOT", root):
                previous = release.previous_version(before)
            plan = release.release_plan("push", "branch", "main", "0.2.0", previous)
            self.assertTrue(plan["publish"])


if __name__ == "__main__":
    unittest.main()
