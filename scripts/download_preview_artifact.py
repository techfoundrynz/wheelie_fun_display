"""Validate an untrusted artifact archive before writing any of its files."""

import argparse
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import tempfile
from zipfile import ZipFile


def extract_preview(archive, output):
    if output.exists() and any(output.iterdir()):
        raise ValueError("Artifact destination must be empty")
    with ZipFile(archive) as bundle:
        entries = bundle.infolist()
        if (
            len(entries) > 100_000
            or sum(item.file_size for item in entries) > 1024 * 1024 * 1024
        ):
            raise ValueError("Preview archive exceeds publication limits")
        for item in entries:
            name = item.filename
            parts = name.rstrip("/").split("/")
            mode = stat.S_IFMT(item.external_attr >> 16)
            if (
                not name
                or "\\" in name
                or ":" in name
                or PurePosixPath(name).is_absolute()
                or any(part in ("", ".", "..", ".git") for part in parts)
                or mode not in (0, stat.S_IFREG, stat.S_IFDIR)
                or not (output / name).resolve().is_relative_to(output.resolve())
            ):
                raise ValueError("Unsafe path or file type in preview archive")
        output.mkdir(parents=True, exist_ok=True)
        bundle.extractall(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-id", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    repository = os.environ["GITHUB_REPOSITORY"]
    if args.artifact_id < 1 or not re.fullmatch(
        r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository
    ):
        parser.error("Invalid artifact identity")
    with tempfile.TemporaryDirectory(prefix="wfd-preview-download-") as directory:
        archive = Path(directory) / "preview.zip"
        with archive.open("wb") as output:
            subprocess.run(
                [
                    "gh",
                    "api",
                    f"repos/{repository}/actions/artifacts/{args.artifact_id}/zip",
                ],
                stdout=output,
                check=True,
                timeout=180,
            )
        extract_preview(archive, args.output.resolve())


if __name__ == "__main__":
    main()
