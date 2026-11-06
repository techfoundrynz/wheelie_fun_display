"""Build the shared Slint display as a self-contained browser simulator."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import tomllib
from pathlib import Path

from site_assets import BIKE_MARKER, bike_icon, favicon

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "simulator/web"


def run(args: list[str], **kwargs) -> None:
    subprocess.run(args, check=True, cwd=ROOT, **kwargs)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=ROOT / ".cache/pages/site/simulator"
    )
    parser.add_argument("--wasm-bindgen", default=os.environ.get("WASM_BINDGEN"))
    parser.add_argument(
        "--skip-build", action="store_true", help="Package an existing release WASM"
    )
    args = parser.parse_args()
    output = args.output.resolve()
    if not args.skip_build:
        run(
            [
                "rustup",
                "target",
                "add",
                "wasm32-unknown-unknown",
                "--toolchain",
                "stable",
            ]
        )
        run(
            [
                "cargo",
                "+stable",
                "build",
                "--locked",
                "--manifest-path",
                str(PROJECT / "Cargo.toml"),
                "--target",
                "wasm32-unknown-unknown",
                "--release",
            ]
        )
    lock = tomllib.loads((PROJECT / "Cargo.lock").read_text(encoding="utf-8"))
    version = next(
        package["version"]
        for package in lock["package"]
        if package["name"] == "wasm-bindgen"
    )
    tool_root = ROOT / ".cache/wasm-tools"
    bindgen = args.wasm_bindgen or str(
        tool_root / "bin" / ("wasm-bindgen.exe" if os.name == "nt" else "wasm-bindgen")
    )
    try:
        installed = subprocess.check_output([bindgen, "--version"], text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        installed = ""
    if installed != f"wasm-bindgen {version}":
        if args.wasm_bindgen:
            raise SystemExit(
                f"Expected wasm-bindgen {version}; got {installed or 'missing tool'}"
            )
        run(
            [
                "cargo",
                "+stable",
                "install",
                "wasm-bindgen-cli",
                "--version",
                version,
                "--locked",
                "--root",
                str(tool_root),
                "--target-dir",
                str(ROOT / ".cache/wasm-cli-target"),
            ]
        )
    output.mkdir(parents=True, exist_ok=True)
    # Simulator and docs/flasher share one stylesheet, font and favicon.
    # Copy these for standalone builds as well as the composed Pages artifact.
    common_assets = output.parent / "assets"
    (common_assets / "fonts").mkdir(parents=True, exist_ok=True)
    for name in ("style.css",):
        shutil.copyfile(ROOT / "docs/site" / name, common_assets / name)
    (common_assets / "favicon.svg").write_text(
        favicon(), encoding="utf-8", newline="\n"
    )
    for name in ("Roboto.ttf", "Roboto-OFL.txt"):
        shutil.copyfile(ROOT / "ui/fonts" / name, common_assets / "fonts" / name)
    for asset in (PROJECT / "static").iterdir():
        if asset.is_file():
            if asset.suffix == ".html":
                (output / asset.name).write_text(
                    asset.read_text(encoding="utf-8").replace(BIKE_MARKER, bike_icon()),
                    encoding="utf-8",
                    newline="\n",
                )
            else:
                shutil.copy2(asset, output / asset.name)
    wasm = (
        PROJECT / "target/wasm32-unknown-unknown/release/wheelie_fun_display_web.wasm"
    )
    run(
        [
            bindgen,
            "--target",
            "web",
            "--out-dir",
            str(output / "pkg"),
            "--out-name",
            "wheelie_fun_display",
            str(wasm),
        ]
    )
    metadata = {
        "renderer": "Slint software renderer",
        "ui": "ui/dashboard.slint",
        "simulated_hardware": True,
    }
    (output / "build.json").write_text(
        json.dumps(metadata, indent=2) + "\n", encoding="utf-8", newline="\n"
    )
    (output / ".nojekyll").touch()
    print(output)


if __name__ == "__main__":
    main()
