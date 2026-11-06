"""Package browser check evidence separately from the public simulator."""

from __future__ import annotations

import argparse
import html
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--site", type=Path, default=ROOT / ".cache/pages/site")
    parser.add_argument(
        "--results", type=Path, default=ROOT / ".cache/pages/browser-tests"
    )
    args = parser.parse_args()
    results = json.loads((args.results / "results.json").read_text(encoding="utf-8"))
    assert results["passed"], "Only a successful browser run can produce the CI gallery"
    output = args.site / "ci"
    output.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(args.results / "results.json", output / "results.json")
    cards = []
    for filename in results["screenshots"]:
        assert Path(filename).name == filename and (args.results / filename).is_file()
        shutil.copyfile(args.results / filename, output / filename)
        label = html.escape(Path(filename).stem.replace("-", " ").capitalize())
        cards.append(
            f'<figure><a href="{filename}"><img src="{filename}" alt="{label}"></a><figcaption>{label}</figcaption></figure>'
        )
    checks = "".join(f"<li>{html.escape(check)}</li>" for check in results["checks"])
    document = f"""<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Browser checks · Wheelie Fun Display</title><link rel="stylesheet" href="../simulator/style.css"><style>main{{max-width:1200px}}.gallery{{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:24px}}figure{{margin:0;background:#141414;padding:14px;border:1px solid #333;border-radius:14px}}img{{max-width:100%;height:auto;max-height:600px;display:block;margin:auto}}figcaption{{padding-top:14px;color:#aaa}}li{{margin:10px 0}}</style></head><body class="simulator-page"><header class="site-header"><a class="brand" href="../">Wheelie Fun Display</a><nav><a href="../maintenance/">Maintenance</a><a href="../simulator/">Interactive simulator</a></nav></header><main class="simulator-main"><p class="eyebrow">CI BROWSER EVIDENCE</p><h1>Touchscreen checks</h1><p>Captured from this build's shared Slint UI. The browser exercises the actual WebAssembly touchscreen with simulated hardware. These pictures are review evidence, not hardware acceptance results or pixel-baseline comparisons.</p><p><a href="results.json">Machine-readable results</a></p><ul>{checks}</ul><section class="gallery" aria-label="Browser screenshots">{"".join(cards)}</section></main></body></html>"""
    (output / "index.html").write_text(document, encoding="utf-8", newline="\n")
    print(f"Packaged CI browser evidence in {output}")


if __name__ == "__main__":
    main()
