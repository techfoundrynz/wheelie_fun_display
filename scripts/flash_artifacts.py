"""Create complete ESP Web Tools images from the build's actual flash layout."""

import hashlib
import json
import subprocess
import sys
from pathlib import Path

CHIP_FAMILIES = {"esp32s3": "ESP32-S3", "esp32p4": "ESP32-P4"}
ROLE_FILES = {
    "bootloader": "bootloader.bin",
    "partition-table": "partitions.bin",
    "otadata": "ota_data_initial.bin",
    "app": "firmware.bin",
}


def flash_layout(build: Path):
    build = build.resolve()
    metadata = json.loads((build / "flasher_args.json").read_text(encoding="utf-8"))
    chip = metadata["extra_esptool_args"]["chip"]
    if chip not in CHIP_FAMILIES:
        raise ValueError("Unsupported installer chip: " + chip)
    settings = metadata["flash_settings"]
    if settings["flash_mode"] not in {"dio", "dout", "qio", "qout"}:
        raise ValueError("Unsupported flash mode")
    if settings["flash_freq"] not in {"20m", "26m", "40m", "80m", "120m"}:
        raise ValueError("Unsupported flash frequency")
    if settings["flash_size"] not in {"4MB", "8MB", "16MB", "32MB", "64MB", "128MB"}:
        raise ValueError("Unsupported flash size")
    parts = []
    for role, fallback in ROLE_FILES.items():
        item = metadata[role]
        if str(item.get("encrypted", "false")).lower() != "false":
            raise ValueError("Encrypted images cannot use this browser installer")
        offset = int(item["offset"], 0)
        filename = item["file"]
        if metadata["flash_files"].get(item["offset"]) != filename:
            raise ValueError("Inconsistent flash layout for " + role)
        candidate = (build / filename).resolve()
        if not candidate.is_relative_to(build):
            raise ValueError("Flash image escapes its build directory")
        if not candidate.is_file():
            candidate = build / fallback
        if not candidate.is_file():
            raise ValueError("Missing " + role + " image: " + str(candidate))
        if offset < 0 or not candidate.stat().st_size:
            raise ValueError("Invalid flash image range")
        parts.append((offset, candidate))
    parts.sort()
    flash_bytes = int(settings["flash_size"].removesuffix("MB")) * 1024 * 1024
    for index, (offset, path) in enumerate(parts):
        end = offset + path.stat().st_size
        if end > flash_bytes or (index + 1 < len(parts) and end > parts[index + 1][0]):
            raise ValueError("Overlapping or oversized installer image")
    return chip, settings, parts


def stage_factory(
    build: Path, destination: Path, environment: str, label: str, release_version: str
):
    chip, settings, parts = flash_layout(build)
    filename = environment + "-factory.bin"
    output = destination / filename
    # ESP Web Tools cannot patch flash headers. esptool merges and updates them
    # using the same layout/settings that produced the release application.
    mode = (
        "dio" if settings["flash_mode"] in {"qio", "qout"} else settings["flash_mode"]
    )
    command = [
        sys.executable,
        "-m",
        "esptool",
        "--chip",
        chip,
        "merge-bin",
        "--output",
        str(output),
        "--flash-mode",
        mode,
        "--flash-freq",
        settings["flash_freq"],
        "--flash-size",
        settings["flash_size"],
    ]
    for offset, path in parts:
        command.extend([hex(offset), str(path)])
    subprocess.run(command, check=True)
    compatibility = ""
    sdk_header = build / "config/sdkconfig.h"
    if (
        chip == "esp32p4"
        and sdk_header.is_file()
        and "#define CONFIG_ESP32P4_SELECTS_REV_LESS_V3 1"
        in sdk_header.read_text(encoding="utf-8")
    ):
        compatibility = "For ESP32-P4 revisions below 3.0. Check your board revision before installing."
    return {
        "compatibility": compatibility,
        "environment": environment,
        "label": label,
        "chip_family": CHIP_FAMILIES[chip],
        "version": release_version,
        "file": filename,
        "size": output.stat().st_size,
        "sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
    }
