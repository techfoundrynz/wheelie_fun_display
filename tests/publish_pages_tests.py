"""Verify persistent site updates and isolated PR preview assembly."""

from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import publish_pages as pages


class PagesPublicationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.site = self.root / "site"
        self.source = self.root / "incoming"
        self.site.mkdir()
        self.source.mkdir()
        (self.site / "index.html").write_text("production", encoding="utf-8")
        (self.source / "index.html").write_text("new site", encoding="utf-8")
        (self.source / "simulator").mkdir()
        (self.source / "simulator/index.html").write_text("simulator", encoding="utf-8")
        (self.source / "ci").mkdir()
        (self.source / "ci/index.html").write_text("gallery", encoding="utf-8")

    def test_production_refresh_keeps_previews_and_domain(self):
        pages.assemble(self.site, self.source, "previews/pr-12/" + "a" * 40)
        (self.site / "CNAME").write_text("example.invalid", encoding="utf-8")
        (self.site / "obsolete.html").write_text("old", encoding="utf-8")
        pages.assemble(self.site, self.source)
        self.assertEqual((self.site / "index.html").read_text(), "new site")
        self.assertTrue((self.site / "previews/pr-12").is_dir())
        self.assertEqual((self.site / "CNAME").read_text(), "example.invalid")
        self.assertFalse((self.site / "obsolete.html").exists())

    def test_preview_is_sandboxed_and_does_not_replace_production(self):
        destination = "previews/pr-12/" + "a" * 40
        pages.assemble(self.site, self.source, destination)
        wrapper = (self.site / destination / "index.html").read_text()
        self.assertIn('sandbox="allow-scripts"', wrapper)
        self.assertNotIn("allow-same-origin", wrapper)
        self.assertIn("serial 'none'", wrapper)
        self.assertIn('src="payload/simulator/"', wrapper)
        self.assertTrue((self.site / destination / "report/index.html").is_file())
        self.assertEqual((self.site / "index.html").read_text(), "production")

    def test_latest_revision_only_and_closed_pr_cleanup(self):
        first = "previews/pr-12/" + "a" * 40
        second = "previews/pr-12/" + "b" * 40
        other = "previews/pr-13/" + "c" * 40
        pages.assemble(self.site, self.source, first)
        pages.assemble(self.site, self.source, other)
        pages.assemble(self.site, self.source, second)
        self.assertFalse((self.site / first).exists())
        self.assertTrue((self.site / second).exists())
        self.assertTrue((self.site / other).exists())
        pages.assemble(self.site, destination="previews/pr-12", remove=True)
        self.assertFalse((self.site / "previews/pr-12").exists())
        self.assertTrue((self.site / other).exists())

    def test_bad_destinations_and_sources_are_rejected(self):
        for destination in (
            "/tmp",
            "../",
            "previews/pr-1/../index.html",
            "previews\\pr-1",
            "previews/pr-0",
        ):
            with self.subTest(destination=destination), self.assertRaises(ValueError):
                pages.destination_path(self.site, destination)
        (self.source / "execute.py").write_text(
            "raise Exception('must not execute')", encoding="utf-8"
        )
        with self.assertRaisesRegex(ValueError, "Unexpected static"):
            pages.assemble(self.site, self.source)

    def test_hardware_downloads_survive_production_and_preview_publication(self):
        cad = self.source / "cad/waveshare-206"
        cad.mkdir(parents=True)
        files = {
            "lower-clamp-cap.step": b"ISO-10303-21;\nEND-ISO-10303-21;\n",
            "assembly.f3d": b"PK\x03\x04native archive",
            "linked-assembly.f3z": b"PK\x03\x04linked archive",
        }
        for name, payload in files.items():
            (cad / name).write_bytes(payload)
        pcb = self.source / "pcb/guition-43"
        pcb.mkdir(parents=True)
        project = b"PK\x03\x04native PCB project"
        (pcb / "Guition_CAN_PCB.epro2").write_bytes(project)
        images = self.source / "assets/images"
        images.mkdir(parents=True)
        animation = b"GIF89a assembly animation"
        (images / "assembly.gif").write_bytes(animation)
        pages.assemble(self.site, self.source)
        destination = "previews/pr-12/" + "a" * 40
        pages.assemble(self.site, self.source, destination)
        for prefix in (self.site, self.site / destination / "payload"):
            self.assertEqual(
                (prefix / "pcb/guition-43/Guition_CAN_PCB.epro2").read_bytes(), project
            )
            self.assertEqual(
                (prefix / "assets/images/assembly.gif").read_bytes(), animation
            )
            for name, payload in files.items():
                with self.subTest(destination=prefix, file=name):
                    self.assertEqual(
                        (prefix / "cad/waveshare-206" / name).read_bytes(), payload
                    )

    def test_preview_cannot_bootstrap_over_an_existing_live_site(self):
        (self.site / "index.html").unlink()
        with self.assertRaisesRegex(ValueError, "initialized"):
            pages.assemble(self.site, self.source, "previews/pr-1/" + "a" * 40)

    def test_webp_survives_production_and_preview_publication(self):
        image = Path("assets/images/ride-portrait.webp")
        payload = b"RIFF\x04\x00\x00\x00WEBP"
        (self.source / image).parent.mkdir(parents=True)
        (self.source / image).write_bytes(payload)
        pages.assemble(self.site, self.source)
        destination = "previews/pr-12/" + "a" * 40
        pages.assemble(self.site, self.source, destination)
        for prefix in (self.site, self.site / destination / "payload"):
            with self.subTest(destination=prefix):
                self.assertEqual((prefix / image).read_bytes(), payload)

    def test_symlink_payload_is_rejected(self):
        outside = self.root / "private.txt"
        outside.write_text("private", encoding="utf-8")
        try:
            (self.source / "link.txt").symlink_to(outside)
        except OSError:
            self.skipTest("Symlink creation unavailable")
        with self.assertRaisesRegex(ValueError, "Unsafe file"):
            pages.validate_source(self.source)

    def test_storage_bootstraps_and_preserves_main_branch(self):
        remote = self.root / "remote.git"
        repo = self.root / "repo"
        runner = self.root / "runner"
        runner.mkdir()
        subprocess.run(
            ["git", "init", "--bare", str(remote)], check=True, capture_output=True
        )
        subprocess.run(
            ["git", "init", "-b", "main", str(repo)], check=True, capture_output=True
        )

        def git(*args):
            return (
                subprocess.check_output(
                    ["git", *args], cwd=repo, stderr=subprocess.DEVNULL
                )
                .decode()
                .strip()
            )

        git("config", "user.name", "Publication test")
        git("config", "user.email", "publication@example.invalid")
        git("config", "commit.gpgsign", "false")
        (repo / "source.txt").write_text(
            "source must remain unchanged", encoding="utf-8"
        )
        git("add", ".")
        git("commit", "-m", "Source")
        head = git("rev-parse", "HEAD")
        git("remote", "add", "origin", str(remote))
        output = self.root / "outputs"
        env = {
            **os.environ,
            "GITHUB_ACTIONS": "true",
            "GITHUB_OUTPUT": str(output),
            "RUNNER_TEMP": str(runner),
        }

        def publish(*args):
            subprocess.run(
                [sys.executable, str(Path(pages.__file__).resolve()), *args],
                cwd=repo,
                env=env,
                capture_output=True,
                text=True,
                check=True,
            )

        publish("--source", str(self.source))
        destination = "previews/pr-12/" + "a" * 40
        publish("--source", str(self.source), "--destination", destination)
        (self.source / "index.html").write_text("updated production", encoding="utf-8")
        publish("--source", str(self.source))
        # Fetch the post-push tip; the local tracking ref is not assumed current.
        git("fetch", "origin", "gh-pages:refs/remotes/origin/gh-pages")
        self.assertEqual(
            git("show", "origin/gh-pages:index.html"), "updated production"
        )
        self.assertIn(
            "sandbox=", git("show", f"origin/gh-pages:{destination}/index.html")
        )
        publish("--destination", "previews/pr-12", "--remove")
        git("fetch", "origin", "gh-pages:refs/remotes/origin/gh-pages")
        self.assertFalse(
            git("ls-tree", "--name-only", "origin/gh-pages", "previews/pr-12")
        )
        self.assertEqual(git("rev-parse", "HEAD"), head)
        self.assertFalse(git("status", "--porcelain"))


if __name__ == "__main__":
    unittest.main()
