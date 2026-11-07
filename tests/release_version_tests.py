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
        self.assertTrue(
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

    def test_failed_bump_then_fix_remains_eligible_until_published(self):
        # Neither the failed bump nor a subsequent fix advances publication.
        for _ in range(2):
            plan = release.release_plan("push", "branch", "main", "0.1.1", "0.1.0")
            self.assertTrue(plan["publish"])
        self.assertFalse(
            release.release_plan("push", "branch", "main", "0.1.1", "0.1.1")["publish"]
        )

    def test_published_versions_are_semantically_sorted_and_validated(self):
        result = subprocess.CompletedProcess(
            [],
            0,
            stdout="v0.9.0\nv0.10.0-rc.1\nv0.10.0\nv0.10.0-beta.2\n"
            "not-a-version\n0.99.0\nv01.0.0\n",
        )
        with patch.object(release.subprocess, "run", return_value=result) as run:
            self.assertEqual(release.published_version("owner/repo"), "0.10.0")
        command = run.call_args.args[0]
        self.assertIn("--paginate", command)
        self.assertIn("repos/owner/repo/releases?per_page=100", command)
        self.assertIn(".[] | select(.draft == false) | .tag_name", command)
        self.assertTrue(run.call_args.kwargs["check"])

    def test_no_published_releases(self):
        with patch.object(
            release.subprocess,
            "run",
            return_value=subprocess.CompletedProcess([], 0, stdout=""),
        ):
            self.assertIsNone(release.published_version("owner/repo"))

    def test_release_lookup_failure_does_not_become_first_release(self):
        with (
            patch.object(
                release.subprocess,
                "run",
                side_effect=subprocess.CalledProcessError(1, ["gh", "api"]),
            ),
            self.assertRaises(subprocess.CalledProcessError),
        ):
            release.published_version("owner/repo")

    def test_main_fix_push_uses_publication_not_previous_push_version(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "platformio.ini").write_text(
                "[common]\ncustom_firmware_version=0.1.1\n", encoding="utf-8"
            )
            event = root / "event.json"
            event.write_text('{"before": "' + "a" * 40 + '"}', encoding="utf-8")
            output = root / "output"
            environment = {
                "GITHUB_EVENT_NAME": "push",
                "GITHUB_EVENT_PATH": str(event),
                "GITHUB_REF_TYPE": "branch",
                "GITHUB_REF_NAME": "main",
                "GITHUB_REPOSITORY": "owner/repo",
                "GITHUB_OUTPUT": str(output),
            }
            with (
                patch.object(release, "ROOT", root),
                patch.dict(release.os.environ, environment),
                patch.object(
                    release, "published_version", return_value="0.1.0"
                ) as published,
            ):
                release.main()
            published.assert_called_once_with("owner/repo")
            self.assertEqual(
                output.read_text(encoding="utf-8"),
                "publish=true\nversion=0.1.1\ntag=v0.1.1\n",
            )

    def test_pr_and_manual_validation_do_not_query_releases(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "platformio.ini").write_text(
                "[common]\ncustom_firmware_version=0.1.1\n", encoding="utf-8"
            )
            event = root / "event.json"
            event.write_text("{}", encoding="utf-8")
            for event_name in ("pull_request", "workflow_dispatch"):
                output = root / event_name
                environment = {
                    "GITHUB_EVENT_NAME": event_name,
                    "GITHUB_EVENT_PATH": str(event),
                    "GITHUB_REF_TYPE": "branch",
                    "GITHUB_REF_NAME": "main",
                    "GITHUB_OUTPUT": str(output),
                }
                with (
                    patch.object(release, "ROOT", root),
                    patch.dict(release.os.environ, environment),
                    patch.object(release, "published_version") as published,
                ):
                    release.main()
                published.assert_not_called()
                self.assertIn("publish=false", output.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
