#!/usr/bin/env python3
"""Compatibility entry point for the TypeScript/TanStack documentation build."""

import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / ".cache/pages/site")
    parser.add_argument(
        "--images", type=Path, default=ROOT / ".cache/pages/docs-images"
    )
    args = parser.parse_args()
    corepack = shutil.which("corepack")
    if not corepack:
        raise SystemExit(
            "Install Node.js 24 and Corepack, then run pnpm install in website/"
        )
    subprocess.run(
        [
            corepack,
            "pnpm",
            "build",
            "--output",
            str(args.output.resolve()),
            "--images",
            str(args.images.resolve()),
        ],
        cwd=ROOT / "website",
        check=True,
    )


if __name__ == "__main__":
    main()
