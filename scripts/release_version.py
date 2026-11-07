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
    event, ref_type, ref_name, current, published=None, manual_release=False
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
    elif published is None or current_key > version_key(published):
        result.update(
            publish=True,
            reason=f"Firmware {current} is newer than published {published}"
            if published is not None
            else f"Firmware {current} has not been published; first release",
        )
    else:
        result["reason"] = "Firmware version is already published or older"
    return result


def published_version(repository):
    # Tags alone do not establish publication: creating a release can fail after
    # its tag was pushed. Include prereleases, but exclude unpublished drafts.
    result = subprocess.run(
        [
            "gh",
            "api",
            "--paginate",
            f"repos/{repository}/releases?per_page=100",
            "--jq",
            ".[] | select(.draft == false) | .tag_name",
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=True,
    )
    versions = []
    for tag in result.stdout.splitlines():
        if not tag.startswith("v"):
            continue
        try:
            version_key(tag[1:])
        except ValueError:
            continue
        versions.append(tag[1:])
    return max(versions, key=version_key, default=None)


def main():
    event = os.environ["GITHUB_EVENT_NAME"]
    payload = json.loads(
        Path(os.environ["GITHUB_EVENT_PATH"]).read_text(encoding="utf-8")
    )
    ref_type = os.environ["GITHUB_REF_TYPE"]
    ref_name = os.environ["GITHUB_REF_NAME"]
    published = (
        published_version(os.environ["GITHUB_REPOSITORY"])
        if event == "push" and ref_type == "branch" and ref_name == "main"
        else None
    )
    plan = release_plan(
        event,
        ref_type,
        ref_name,
        configured_version((ROOT / "platformio.ini").read_text(encoding="utf-8")),
        published,
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
