"""Check release notice preservation and immutable source references."""

import json
import re
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from zipfile import ZipFile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import package_release as package


class ReleaseNoticesTests(unittest.TestCase):
    def test_bundle_preserves_notices_and_source_metadata(self):
        references = {"application": {"commit": "a" * 40, "worktree_dirty": False}}
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            first = package.stage_notices(output, references)
            with ZipFile(output / "licenses.zip") as archive:
                for source in package.notice_files():
                    name = source.relative_to(package.ROOT).as_posix()
                    self.assertEqual(archive.read(name), source.read_bytes())
                self.assertEqual(
                    json.loads(archive.read("source-references.json")), references
                )
            self.assertEqual(
                json.loads((output / "source-references.json").read_text()), references
            )
            self.assertEqual(first, package.stage_notices(output, references))

    def test_ci_repository_overrides_local_remote(self):
        with (
            patch.dict(package.os.environ, {"GITHUB_REPOSITORY": "owner/release"}),
            patch.object(
                package,
                "git_output",
                return_value="https://github.com/stale/remote.git",
            ),
        ):
            self.assertEqual(package.repository_name(), "owner/release")

    def test_remote_sanitization_and_ssh(self):
        with patch.dict(package.os.environ, {"GITHUB_REPOSITORY": ""}):
            with patch.object(
                package, "git_output", return_value="git@github.com:owner/project.git"
            ):
                self.assertEqual(package.repository_name(), "owner/project")
            with patch.object(
                package,
                "git_output",
                return_value="https://credential@github.com/owner/project.git",
            ):
                self.assertEqual(package.repository_name(), package.DEFAULT_REPOSITORY)

    def test_dirty_source_is_identified_as_a_baseline(self):
        def git(*args):
            if args[:2] == ("rev-parse", "HEAD"):
                return "b" * 40
            if args[0] == "status":
                return " M README.md"
            return "https://github.com/owner/project.git"

        with (
            patch.dict(package.os.environ, {"GITHUB_REPOSITORY": ""}),
            patch.object(package, "git_output", side_effect=git),
        ):
            references = package.source_references()
        app = references["application"]
        self.assertTrue(app["worktree_dirty"])
        self.assertEqual(
            app["source_url"], "https://github.com/owner/project/tree/" + "b" * 40
        )
        self.assertEqual(len(references["slint"]["commit"]), 40)
        self.assertIn(references["slint"]["commit"], references["slint"]["source_url"])

    def test_notice_pin_must_match_build_pin(self):
        options = re.sub(
            r"(-DSLINT_PREBUILT_TAG=)\S+",
            r"\1different-tag",
            package.config()["common"]["board_build.cmake_extra_args"],
        )
        with (
            patch.object(
                package,
                "config",
                return_value={"common": {"board_build.cmake_extra_args": options}},
            ),
            self.assertRaisesRegex(ValueError, "match the PlatformIO Slint pin"),
        ):
            package.source_references()


if __name__ == "__main__":
    unittest.main()
