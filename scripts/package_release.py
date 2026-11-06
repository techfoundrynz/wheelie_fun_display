"""Stage supported OTA images with notices and source-reference artifacts."""

import hashlib
import json
import os
import re
import shutil
import subprocess
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

from flash_artifacts import stage_factory
from project_config import ROOT, config, environments, option, version

DEFAULT_REPOSITORY = "techfoundrynz/wheelie_fun_display"


def git_output(*arguments):
    try:
        return subprocess.check_output(
            ["git", "-C", str(ROOT), *arguments], text=True, stderr=subprocess.DEVNULL
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def repository_name():
    candidate = os.environ.get("GITHUB_REPOSITORY", "")
    if re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", candidate):
        return candidate
    remote = git_output("remote", "get-url", "origin") or ""
    match = re.fullmatch(
        r"(?:https://github\.com/|git@github\.com:)([A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+?)(?:\.git)?/?",
        remote,
    )
    return match[1] if match else DEFAULT_REPOSITORY


def source_references():
    commit = git_output("rev-parse", "HEAD")
    status = git_output("status", "--porcelain", "--untracked-files=normal")
    repository = "https://github.com/" + repository_name()
    notices = (ROOT / "THIRD_PARTY.md").read_text(encoding="utf-8")
    match = re.search(
        r"\*\*Slint\*\*.*?\]\(https://github\.com/([^/]+/[^/]+)/releases/tag/([^)]+)\), commit `([0-9a-f]{40})`",
        notices,
    )
    if not match:
        raise ValueError(
            "Record the pinned Slint repository, tag and commit in THIRD_PARTY.md"
        )
    slint_repository, slint_tag, slint_commit = match.groups()
    options = config()["common"]["board_build.cmake_extra_args"]
    for name, expected in (("REPO", slint_repository), ("TAG", slint_tag)):
        pin = re.search(r"-DSLINT_PREBUILT_" + name + r"=([^\s]+)", options)
        if not pin or pin[1] != expected:
            raise ValueError("Update THIRD_PARTY.md to match the PlatformIO Slint pin")
    return {
        "application": {
            "repository": repository,
            "commit": commit,
            "worktree_dirty": status != "" if status is not None else None,
            "source_url": f"{repository}/tree/{commit}" if commit else repository,
        },
        "slint": {
            "repository": "https://github.com/" + slint_repository,
            "tag": slint_tag,
            "commit": slint_commit,
            "source_url": f"https://github.com/{slint_repository}/tree/{slint_commit}",
        },
    }


def notice_files():
    files = {
        ROOT / "LICENSE",
        ROOT / "THIRD_PARTY.md",
        ROOT / "ui/assets/LUCIDE-LICENSE",
    }
    files.update((ROOT / "ui/fonts").glob("*OFL*.txt"))
    for entry in (ROOT / "firmware").rglob("LICENSE*"):
        if entry.is_file():
            files.add(entry)
        elif entry.is_dir():
            files.update(path for path in entry.rglob("*") if path.is_file())
    for path in files:
        if not path.is_file():
            raise ValueError(f"Missing release notice: {path}")
    return sorted(files)


def stage_notices(destination, references):
    encoded = (json.dumps(references, indent=2) + "\n").encode("utf-8")
    (destination / "source-references.json").write_bytes(encoded)
    entries = {
        str(path.relative_to(ROOT)).replace("\\", "/"): path.read_bytes()
        for path in notice_files()
    }
    entries["source-references.json"] = encoded
    entries["README.txt"] = (
        b"Project and bundled asset/component notices for Wheelie Fun Display.\n"
        b"Dependencies retain their respective licenses.\n"
        b"See source-references.json for the application and pinned Slint source snapshots.\n"
        b"A dirty application worktree means its commit identifies a baseline, not all local changes.\n"
    )
    archive = destination / "licenses.zip"
    with ZipFile(archive, "w", compression=ZIP_DEFLATED, compresslevel=9) as bundle:
        for name, content in sorted(entries.items()):
            entry = ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            entry.create_system = 3
            entry.external_attr = 0o100644 << 16
            entry.compress_type = ZIP_DEFLATED
            bundle.writestr(entry, content, compresslevel=9)
    return {
        "file": archive.name,
        "sha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
    }


def main():
    destination = ROOT / ".pio/releases"
    destination.mkdir(parents=True, exist_ok=True)
    manifest = {
        "version": os.environ.get("WFD_FIRMWARE_VERSION", version()).removeprefix("v"),
        "images": [],
        "installers": [],
        "source_references": "source-references.json",
        "notices": stage_notices(destination, source_references()),
    }
    # Retired targets must not survive as accidental release assets.
    supported = {
        name
        for environment in environments()
        for name in (environment + ".bin", environment + "-factory.bin")
    }
    for stale in destination.glob("*.bin"):
        if stale.name not in supported:
            stale.unlink()
    for environment in environments():
        source = ROOT / ".pio/build" / environment / "firmware.bin"
        if not source.is_file():
            raise SystemExit(f"Missing {source}; run pio run first")
        manifest["installers"].append(
            stage_factory(
                source.parent,
                destination,
                environment,
                option(environment, "custom_hardware_label") or environment,
                manifest["version"],
            )
        )
        image = source.read_bytes()
        name = environment + ".bin"
        shutil.copyfile(source, destination / name)
        manifest["images"].append(
            {
                "environment": environment,
                "file": name,
                "size": len(image),
                "sha256": hashlib.sha256(image).hexdigest(),
            }
        )
    (destination / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n"
    )
    print(destination)


if __name__ == "__main__":
    main()
