"""Stage a styled ESP Web Tools installer and verified, same-origin release images."""

from __future__ import annotations

import argparse
import base64
import hashlib
import io
import json
import os
import re
import shutil
import tarfile
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

from project_config import ROOT, environments, option
from site_assets import BIKE_MARKER, bike_icon

REPOSITORY = "techfoundrynz/wheelie_fun_display"
SAFE_HOSTS = {
    "api.github.com",
    "github.com",
    "objects.githubusercontent.com",
    "release-assets.githubusercontent.com",
    "github-releases.githubusercontent.com",
    "registry.npmjs.org",
    "raw.githubusercontent.com",
}


class SafeRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, new_url):
        target = urllib.parse.urlsplit(new_url)
        if target.scheme != "https" or target.hostname not in SAFE_HOSTS:
            raise ValueError("Untrusted firmware download redirect")
        result = super().redirect_request(request, fp, code, message, headers, new_url)
        if target.hostname != urllib.parse.urlsplit(request.full_url).hostname:
            result.remove_header("Authorization")
        return result


def download(url, *, token=None, binary=False):
    parsed = urllib.parse.urlsplit(url)
    if parsed.scheme != "https" or parsed.hostname not in SAFE_HOSTS:
        raise ValueError("Untrusted asset URL")
    headers = {
        "User-Agent": "Wheelie-Fun-Display-Pages",
        "Accept": "application/octet-stream"
        if binary
        else "application/vnd.github+json",
    }
    if token and parsed.hostname == "api.github.com":
        headers["Authorization"] = "Bearer " + token
    request = urllib.request.Request(url, headers=headers)
    with urllib.request.build_opener(SafeRedirect()).open(
        request, timeout=60
    ) as response:
        return response.read()


def verified_npm(entry, cache):
    key = hashlib.sha256(entry["resolved"].encode()).hexdigest() + ".tgz"
    path = cache / key
    raw = path.read_bytes() if path.is_file() else download(entry["resolved"])
    algorithm, encoded = entry["integrity"].split("-", 1)
    if (
        algorithm != "sha512"
        or base64.b64encode(hashlib.sha512(raw).digest()).decode() != encoded
    ):
        raise ValueError("ESP Web Tools package integrity mismatch: " + entry["name"])
    if not path.exists():
        path.write_bytes(raw)
    return raw


def stage_web_tools(destination):
    lock = json.loads(
        (ROOT / "docs/flasher/vendor-lock.json").read_text(encoding="utf-8")
    )
    cache = ROOT / ".cache/esp-web-tools"
    cache.mkdir(parents=True, exist_ok=True)
    destination.mkdir(parents=True, exist_ok=True)
    notices = []
    for entry in [lock] + lock["packages"]:
        raw = verified_npm(entry, cache)
        with tarfile.open(fileobj=io.BytesIO(raw), mode="r:gz") as archive:
            license_files = []
            for member in archive.getmembers():
                if not member.isfile():
                    continue
                basename = Path(member.name).name
                if (
                    entry is lock
                    and member.name.startswith("package/dist/web/")
                    and basename.endswith(".js")
                ):
                    if not re.fullmatch(r"[A-Za-z0-9_.-]+\.js", basename):
                        raise ValueError("Invalid vendored JavaScript filename")
                    (destination / basename).write_bytes(
                        archive.extractfile(member).read()
                    )
                if basename.upper().startswith(
                    ("LICENSE", "LICENCE", "COPYING", "COPYRIGHT", "NOTICE")
                ):
                    license_files.append(
                        (
                            member.name,
                            archive.extractfile(member).read().decode("utf-8"),
                        )
                    )
            if not license_files and entry.get("notice_url"):
                notice_path = cache / (entry["notice_sha256"] + ".txt")
                notice = (
                    notice_path.read_bytes()
                    if notice_path.is_file()
                    else download(entry["notice_url"])
                )
                if hashlib.sha256(notice).hexdigest() != entry["notice_sha256"]:
                    raise ValueError("Bundled notice integrity mismatch")
                notice_path.write_bytes(notice)
                license_files.append((entry["notice_url"], notice.decode("utf-8")))
            if not license_files:
                raise ValueError("Missing bundled license text: " + entry["name"])
            notices.append(
                f"{entry['name']} {entry['version']}\n{entry['resolved']}\n"
                + "\n\n".join(name + "\n" + text for name, text in license_files)
            )
    (destination / "THIRD_PARTY_NOTICES.txt").write_text(
        "\n\n".join(notices), encoding="utf-8", newline="\n"
    )
    (destination / "source.json").write_text(
        json.dumps(
            {
                "name": lock["name"],
                "version": lock["version"],
                "commit": lock["git_commit"],
                "integrity": lock["integrity"],
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
        newline="\n",
    )


def validate_installer(item, read_asset, destination):
    environment = item["environment"]
    if not re.fullmatch(r"[a-z0-9_]+", environment):
        raise ValueError("Invalid installer environment")
    name = item["file"]
    if name != environment + "-factory.bin" or item["chip_family"] not in {
        "ESP32-S3",
        "ESP32-P4",
    }:
        raise ValueError("Invalid installer identity")
    if not re.fullmatch(r"[0-9a-f]{64}", item["sha256"]):
        raise ValueError("Invalid installer SHA256")
    raw = read_asset(name)
    if len(raw) != item["size"] or hashlib.sha256(raw).hexdigest() != item["sha256"]:
        raise ValueError("Installer size or SHA256 mismatch: " + name)
    package_id = environment + "-" + item["sha256"]
    destination = destination / package_id
    destination.mkdir(parents=True, exist_ok=True)
    (destination / name).write_bytes(raw)
    manifest = {
        "name": "Wheelie Fun Display",
        "version": item["version"],
        "new_install_prompt_erase": True,
        "new_install_improv_wait_time": 0,
        "builds": [
            {"chipFamily": item["chip_family"], "parts": [{"path": name, "offset": 0}]}
        ],
    }
    manifest_name = environment + ".json"
    (destination / manifest_name).write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n"
    )
    return {
        **item,
        "manifest": "firmware/" + package_id + "/" + manifest_name,
        "download": "firmware/" + package_id + "/" + name,
    }


def stage_installers(
    output, release, read_asset, *, channel, release_url=None, boards=None
):
    catalog = {
        "channel": channel,
        "version": release.get("version"),
        "release_url": release_url,
        "boards": [],
    }
    installers = {item["environment"]: item for item in release.get("installers", [])}
    if boards is None:
        boards = [
            {
                "environment": environment,
                "label": option(environment, "custom_hardware_label") or environment,
            }
            for environment in environments()
        ]
    for target in boards:
        environment = target["environment"]
        if not re.fullmatch(r"[a-z0-9_]+", environment) or not isinstance(
            target["label"], str
        ):
            raise ValueError("Invalid tested installer target")
        board = {
            "environment": environment,
            "label": target["label"],
            "available": False,
        }
        if environment in installers:
            item = installers[environment]
            if item.get("version") != release.get("version"):
                raise ValueError("Installer and release versions differ")
            board.update(validate_installer(item, read_asset, output / "firmware"))
            board["available"] = True
        catalog["boards"].append(board)
    (output / "catalog.json").write_text(
        json.dumps(catalog, indent=2) + "\n", encoding="utf-8", newline="\n"
    )
    return catalog


def github_release(repository, token):
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository):
        raise ValueError("Invalid GitHub repository")
    try:
        release = json.loads(
            download(
                f"https://api.github.com/repos/{repository}/releases/latest",
                token=token,
            )
        )
    except urllib.error.HTTPError as error:
        if error.code == 404:
            return {}, None, lambda name: b""
        raise
    assets = {asset["name"]: asset for asset in release.get("assets", [])}

    def read_asset(name):
        asset = assets.get(name)
        if not asset:
            raise ValueError("Release installer asset is missing: " + name)
        raw = download(asset["url"], token=token, binary=True)
        digest = asset.get("digest")
        if digest and digest != "sha256:" + hashlib.sha256(raw).hexdigest():
            raise ValueError("GitHub release asset digest mismatch: " + name)
        return raw

    if "manifest.json" not in assets:
        return {}, release.get("html_url"), read_asset
    manifest = json.loads(read_asset("manifest.json"))
    if release.get("tag_name", "").removeprefix("v") != manifest.get("version"):
        raise ValueError("Release tag and installer version differ")
    return manifest, release.get("html_url"), read_asset


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--site", type=Path, default=ROOT / ".cache/pages/site")
    parser.add_argument(
        "--release-dir",
        type=Path,
        help="Local release directory for a clearly labelled build preview",
    )
    parser.add_argument(
        "--github-release",
        action="store_true",
        help="Stage the latest published release during the Pages build",
    )
    parser.add_argument(
        "--repository", default=os.environ.get("GITHUB_REPOSITORY", REPOSITORY)
    )
    parser.add_argument(
        "--catalog-only",
        action="store_true",
        help="Refresh release data without replacing tested UI or vendor assets",
    )
    args = parser.parse_args()
    site = args.site.resolve()
    if site == ROOT or not site.is_relative_to(ROOT):
        raise SystemExit(
            "Site output must be a separate directory inside the repository"
        )
    output = site / "install"
    output.mkdir(parents=True, exist_ok=True)
    boards = None
    if args.catalog_only:
        boards = json.loads((output / "catalog.json").read_text(encoding="utf-8"))[
            "boards"
        ]
        firmware = output / "firmware"
        if firmware.exists():
            if firmware.is_symlink() or not firmware.resolve().is_relative_to(
                output.resolve()
            ):
                raise ValueError("Unsafe installer firmware directory")
            shutil.rmtree(firmware)
    else:
        docs_manifest = args.site.resolve() / "docs-manifest.json"
        react_site = (
            docs_manifest.is_file()
            and json.loads(docs_manifest.read_text(encoding="utf-8")).get("framework")
            == "tanstack-start"
        )
        for source in (ROOT / "docs/flasher").iterdir():
            if react_site and source.suffix in {".html", ".js"}:
                continue
            if source.suffix in {".html", ".css", ".js"}:
                if source.suffix == ".html":
                    (output / source.name).write_text(
                        source.read_text(encoding="utf-8").replace(
                            BIKE_MARKER, bike_icon()
                        ),
                        encoding="utf-8",
                        newline="\n",
                    )
                else:
                    shutil.copyfile(source, output / source.name)
        stage_web_tools(output / "vendor/esp-web-tools")
    if args.release_dir:
        directory = args.release_dir.resolve()
        release = json.loads((directory / "manifest.json").read_text(encoding="utf-8"))
        catalog = stage_installers(
            output,
            release,
            lambda name: (directory / name).read_bytes(),
            channel="local",
            boards=boards,
        )
    elif args.github_release:
        release, release_url, reader = github_release(
            args.repository, os.environ.get("GITHUB_TOKEN")
        )
        catalog = stage_installers(
            output,
            release,
            reader,
            channel="release",
            release_url=release_url,
            boards=boards,
        )
    else:
        catalog = stage_installers(
            output, {}, lambda name: b"", channel="release", boards=boards
        )
    print(
        f"Installer page: {output}; {sum(board['available'] for board in catalog['boards'])} released/local board images staged"
    )


if __name__ == "__main__":
    main()
