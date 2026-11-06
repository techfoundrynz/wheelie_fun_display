"""Decide whether a GitHub push or manual run should publish firmware."""

from configparser import ConfigParser
from pathlib import Path
import json
import os
import re
import subprocess

from project_config import ROOT


def version_key(value):
    match = re.fullmatch(
        r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)"
        r"(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?"
        r"(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?",
        value,
    )
    if not match:
        raise ValueError("custom_firmware_version must be a semantic version")
    identifiers = match[4].split(".") if match[4] else []
    if any(
        item.isdigit() and len(item) > 1 and item.startswith("0")
        for item in identifiers
    ):
        raise ValueError("Numeric prerelease identifiers cannot have leading zeroes")
    prerelease = tuple(
        (0, int(item)) if item.isdigit() else (1, item) for item in identifiers
    )
    return (*map(int, match.groups()[:3]), not identifiers, prerelease)


def configured_version(source):
    values = ConfigParser(interpolation=None)
    values.read_string(source)
    value = values["common"]["custom_firmware_version"].strip()
    version_key(value)
    return value


def release_plan(
    event, ref_type, ref_name, current, previous=None, manual_release=False
):
    current_key = version_key(current)
    result = {"version": current, "tag": "v" + current, "publish": False}
    if ref_type == "tag":
        if ref_name != result["tag"]:
            raise ValueError("Release tag must match common.custom_firmware_version")
        publish = event == "push" or (event == "workflow_dispatch" and manual_release)
        result.update(
            publish=publish,
            reason="Matching version tag" if publish else "Manual validation only",
        )
    elif ref_name != "main":
        result["reason"] = "Branch releases run only from main"
    elif event == "workflow_dispatch":
        result.update(
            publish=manual_release,
            reason="Manual publication requested"
            if manual_release
            else "Manual validation only",
        )
    elif event != "push":
        result["reason"] = "Not a release event"
    elif previous is None:
        result["reason"] = "Initial branch push; use a manual run for the first release"
    elif current_key > version_key(previous):
        result.update(
            publish=True, reason=f"Firmware version increased: {previous} -> {current}"
        )
    else:
        result["reason"] = "Firmware version did not increase"
    return result


def previous_version(before):
    if not before or set(before) == {"0"}:
        return None
    if not re.fullmatch(r"[0-9a-f]{40,64}", before):
        raise ValueError("Invalid push base commit")
    command = ["git", "show", before + ":platformio.ini"]
    result = subprocess.run(
        command, cwd=ROOT, capture_output=True, text=True, encoding="utf-8"
    )
    if result.returncode:
        # A rewritten branch can leave the push's base outside checkout history.
        subprocess.run(
            ["git", "fetch", "--no-tags", "origin", before], cwd=ROOT, check=True
        )
        result = subprocess.run(
            command,
            cwd=ROOT,
            capture_output=True,
            text=True,
            encoding="utf-8",
            check=True,
        )
    return configured_version(result.stdout)


def main():
    event = os.environ["GITHUB_EVENT_NAME"]
    payload = json.loads(
        Path(os.environ["GITHUB_EVENT_PATH"]).read_text(encoding="utf-8")
    )
    ref_type = os.environ["GITHUB_REF_TYPE"]
    before = payload.get("before", "")
    previous = (
        previous_version(before) if event == "push" and ref_type == "branch" else None
    )
    plan = release_plan(
        event,
        ref_type,
        os.environ["GITHUB_REF_NAME"],
        configured_version((ROOT / "platformio.ini").read_text(encoding="utf-8")),
        previous,
        manual_release=str(
            payload.get("inputs", {}).get("publish_release", False)
        ).lower()
        == "true",
    )
    print(plan["reason"])
    with Path(os.environ["GITHUB_OUTPUT"]).open(
        "a", encoding="utf-8", newline="\n"
    ) as output:
        for name in ("publish", "version", "tag"):
            value = (
                str(plan[name]).lower() if isinstance(plan[name], bool) else plan[name]
            )
            output.write(f"{name}={value}\n")


if __name__ == "__main__":
    main()
