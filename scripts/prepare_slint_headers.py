"""Stage pinned Slint headers with bounded expired model-listener storage.

The downloaded package stays unchanged. Model peers otherwise retain expired
shared-ownership control blocks indefinitely when a model's data stays static.
"""

import argparse
from pathlib import Path

ORIGINAL = (
    b"void attach_peer(private_api::ModelPeer p) { peers.push_back(std::move(p)); }"
)
REPLACEMENT = b"""void attach_peer(private_api::ModelPeer p) {
        peers.erase(std::remove_if(peers.begin(), peers.end(),
                                   [](const auto &peer) { return peer.expired(); }),
                    peers.end());
        peers.push_back(std::move(p));
    }"""


def stage_headers(source: Path, output: Path) -> None:
    model_header = source / "private/slint_models.h"
    data = model_header.read_bytes()
    if data.count(ORIGINAL) != 1:
        raise RuntimeError("Pinned Slint model header changed; review listener cleanup")
    for path in source.rglob("*"):
        if not path.is_file():
            continue
        target = output / path.relative_to(source)
        contents = path.read_bytes()
        if path == model_header:
            contents = contents.replace(ORIGINAL, REPLACEMENT, 1)
        if not target.exists() or target.read_bytes() != contents:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(contents)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    stage_headers(args.source, args.output)
