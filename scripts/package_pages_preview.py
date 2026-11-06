"""Bind a tested Pages artifact to its exact CI revision."""

import argparse
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--site", type=Path, required=True)
    args = parser.parse_args()
    event = json.loads(
        Path(os.environ["GITHUB_EVENT_PATH"]).read_text(encoding="utf-8")
    )
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    pr = (
        event.get("pull_request")
        if os.environ["GITHUB_EVENT_NAME"] == "pull_request"
        else None
    )
    if pr and commit != pr["head"]["sha"]:
        raise ValueError("Preview must be built from the recorded PR head")
    metadata = {
        "kind": "pr" if pr else "site",
        "commit": commit,
        "pr": pr["number"] if pr else None,
        "repository": os.environ["GITHUB_REPOSITORY"],
        "run_id": int(os.environ["GITHUB_RUN_ID"]),
    }
    (args.site / "preview.json").write_text(
        json.dumps(metadata, indent=2) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
