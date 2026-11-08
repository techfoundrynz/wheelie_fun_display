"""Publish static Pages content without executing PR code or deleting previews."""

import argparse
import html
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from site_assets import bike_icon


def destination_path(site, destination):
    if not re.fullmatch(r"previews/pr-[1-9][0-9]*(?:/[0-9a-f]{40})?", destination):
        raise ValueError("Invalid PR preview destination")
    target = (site / destination).resolve()
    if not target.is_relative_to(site.resolve()) or target == site.resolve():
        raise ValueError("Destination escapes the Pages tree")
    return target


def remove_inside(path, site):
    if (
        path.is_symlink()
        or not path.resolve().is_relative_to(site.resolve())
        or path.resolve() == site.resolve()
    ):
        raise ValueError("Unsafe Pages removal")
    if path.is_dir():
        shutil.rmtree(path)
    elif path.exists():
        path.unlink()


def validate_source(source):
    allowed = {
        ".html",
        ".js",
        ".css",
        ".json",
        ".png",
        ".gif",
        ".jpg",
        ".jpeg",
        ".svg",
        ".wasm",
        ".ttf",
        ".md",
        ".txt",
        ".ts",
        ".ico",
        ".woff",
        ".woff2",
        ".map",
        ".webmanifest",
        ".bin",
        ".zip",
        ".step",
        ".f3d",
        ".f3z",
        ".epro2",
    }
    total = 0
    for entry in source.rglob("*"):
        relative = entry.relative_to(source)
        if (
            entry.is_symlink()
            or not entry.resolve().is_relative_to(source.resolve())
            or ".git" in relative.parts
        ):
            raise ValueError("Unsafe file in Pages source")
        if entry.is_file():
            if entry.suffix.lower() not in allowed and entry.name != ".nojekyll":
                raise ValueError(f"Unexpected static file: {relative}")
            total += entry.stat().st_size
    if total > 1024 * 1024 * 1024:
        raise ValueError("Pages source exceeds 1 GiB")


def preview_wrapper(destination, report=False):
    number, commit = destination.split("/")[1:]
    root = "../../../../" if report else "../../../"
    source = "../payload/ci/" if report else "payload/simulator/"
    other = "../" if report else "report/"
    label = "Simulator" if report else "Browser test gallery"
    return f'''<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="robots" content="noindex"><title>Wheelie Fun Display PR preview</title><link rel="icon" href="{root}assets/favicon.svg"><link rel="stylesheet" href="{root}assets/style.css"><style>body{{margin:0;height:100dvh;display:flex;flex-direction:column}}header{{display:flex;align-items:center;flex-wrap:wrap;gap:16px;padding:12px 20px}}header .brand{{margin-right:auto}}iframe{{border:0;width:100%;flex:1;min-height:0}}.preview-note{{color:#bbbbbb;font-size:13px}}</style></head><body><header><a class="brand" href="{root}">{bike_icon()}<span>Wheelie Fun Display</span></a><span class="preview-note">{html.escape(number)} · {commit[:8]} · isolated preview</span><a href="{other}">{label}</a></header><iframe title="PR simulator preview" src="{source}" sandbox="allow-scripts" allow="serial 'none'; usb 'none'; bluetooth 'none'; camera 'none'; microphone 'none'; geolocation 'none'" referrerpolicy="no-referrer"></iframe></body></html>'''


def assemble(site, source=None, destination=None, remove=False):
    site.mkdir(parents=True, exist_ok=True)
    if source:
        validate_source(source)
    if destination:
        target = destination_path(site, destination)
        if remove:
            remove_inside(target, site)
        else:
            if not (site / "index.html").is_file():
                raise ValueError(
                    "Production Pages must be initialized by a successful main CI run first"
                )
            if len(destination.split("/")) != 3:
                raise ValueError("Preview publication requires an exact revision")
            parent = target.parent
            remove_inside(parent, site)
            shutil.copytree(source, target / "payload")
            (target / "index.html").write_text(
                preview_wrapper(destination), encoding="utf-8"
            )
            (target / "report").mkdir()
            (target / "report/index.html").write_text(
                preview_wrapper(destination, True), encoding="utf-8"
            )
            commit = destination.split("/")[-1]
            (parent / "index.html").write_text(
                f'<!doctype html><meta charset="utf-8"><meta name="robots" content="noindex"><meta http-equiv="refresh" content="0;url=./{commit}/"><a href="./{commit}/">Open latest simulator preview</a>',
                encoding="utf-8",
            )
    else:
        for entry in site.iterdir():
            if entry.name not in (".git", "previews", "CNAME", ".nojekyll"):
                remove_inside(entry, site)
        for entry in source.iterdir():
            if entry.name in (".git", "previews"):
                continue
            target = site / entry.name
            if entry.is_dir():
                shutil.copytree(entry, target, dirs_exist_ok=True)
            else:
                shutil.copy2(entry, target)
    (site / ".nojekyll").touch()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path)
    parser.add_argument("--destination")
    parser.add_argument("--remove", action="store_true")
    args = parser.parse_args()
    if os.environ.get("GITHUB_ACTIONS") != "true":
        parser.error("Publication runs only in the trusted Actions deployment job")
    if not args.remove and (not args.source or not args.source.is_dir()):
        parser.error("--source must be a built static directory")
    if args.remove and not args.destination:
        parser.error("--remove requires a destination")
    site = Path(
        tempfile.mkdtemp(prefix="wfd-pages-", dir=os.environ.get("RUNNER_TEMP"))
    )
    git = lambda *values, **kwargs: subprocess.run(
        ["git", *values], check=True, **kwargs
    )
    remote = subprocess.run(
        ["git", "ls-remote", "--exit-code", "origin", "refs/heads/gh-pages"],
        capture_output=True,
    )
    if remote.returncode == 0:
        git(
            "fetch",
            "--no-tags",
            "origin",
            "gh-pages:refs/remotes/origin/gh-pages",
            "--depth=1",
        )
        revision = "origin/gh-pages"
    elif remote.returncode == 2 and args.remove:
        with Path(os.environ["GITHUB_OUTPUT"]).open("a", encoding="utf-8") as output:
            output.write("deploy=false\n")
        return
    elif remote.returncode == 2 and args.destination:
        raise ValueError(
            "Production Pages must be initialized by the main CI pipeline first"
        )
    elif remote.returncode == 2:
        tree = subprocess.check_output(["git", "mktree"], input=b"").decode().strip()
        revision = (
            subprocess.check_output(
                [
                    "git",
                    "-c",
                    "user.name=github-actions[bot]",
                    "-c",
                    "user.email=41898282+github-actions[bot]@users.noreply.github.com",
                    "commit-tree",
                    tree,
                    "-m",
                    "Initialize Pages storage",
                ]
            )
            .decode()
            .strip()
        )
    else:
        remote.check_returncode()
    git("worktree", "add", "--detach", str(site), revision)
    assemble(
        site,
        args.source.resolve() if args.source else None,
        args.destination,
        args.remove,
    )
    git("-C", str(site), "add", "--all")
    if subprocess.run(
        ["git", "-C", str(site), "diff", "--cached", "--quiet"]
    ).returncode:
        git(
            "-C",
            str(site),
            "-c",
            "user.name=github-actions[bot]",
            "-c",
            "user.email=41898282+github-actions[bot]@users.noreply.github.com",
            "commit",
            "-m",
            "Update generated Pages content",
        )
        git("-C", str(site), "push", "origin", "HEAD:refs/heads/gh-pages")
    with Path(os.environ["GITHUB_OUTPUT"]).open("a", encoding="utf-8") as output:
        output.write(f"site={site.as_posix()}\n")


if __name__ == "__main__":
    main()
