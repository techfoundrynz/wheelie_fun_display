"""Constant-initialize generated Slint font metadata so it stays in flash."""

import argparse
import re
from pathlib import Path

HEADER = '#include "dashboard.h"'
FONT_TYPE = r"slint::cbindgen_private::(?:BitmapGlyph|BitmapGlyphs|BitmapFont)"
FONT_RESOURCE = re.compile(
    rf"^(?:constinit )?const ({FONT_TYPE}) "
    r"(slint_embedded_resource_\w+)(\[\d+\])? = ([\s\S]*?);",
    re.MULTILINE,
)
FONT_DEFINITION = re.compile(rf"^\s*[^;\n]*\b{FONT_TYPE}\s+\w+[^;\n]*=", re.MULTILINE)
RESOURCE_SLICE = re.compile(
    r"slint::private_api::make_slice\(\s*(slint_embedded_resource_\w+)\s*,\s*(\d+)\s*\)"
)
HELPER_SLICE = re.compile(r"wfd_font_slice\(slint_embedded_resource_\w+, \d+\)")
FONT_SLICE_HELPER = """
// Generated resource arrays have static addresses, including empty arrays.
// Avoid dynamic slice initialization and keep immutable font metadata in flash.
template<typename T>
constexpr slint::cbindgen_private::Slice<T> wfd_font_slice(const T *ptr, size_t len) {
    return {const_cast<T *>(ptr), len};
}
"""


def prepare_font_resources(content: str) -> str:
    """Rewrite only recognized static font aggregates; reject generator changes.

    constinit also makes the C++ compiler enforce constant initialization. The
    restricted aggregate grammar rejects dynamic calls before writing the file.
    Texture resources and runtime make_slice expressions are left untouched.
    """
    if content.count(HEADER) != 1:
        raise ValueError("Expected exactly one dashboard.h include")
    definitions = list(FONT_DEFINITION.finditer(content))
    resources = list(FONT_RESOURCE.finditer(content))
    if len(definitions) != len(resources):
        raise ValueError("Unrecognized generated font resource definition")

    def resource(match: re.Match) -> str:
        kind, name, bounds, body = match.groups()
        body = RESOURCE_SLICE.sub(
            lambda item: f"wfd_font_slice({item[1]}, {item[2]})", body
        )
        if "make_slice" in body:
            raise ValueError("Unrecognized font slice initializer: " + name)
        aggregate = HELPER_SLICE.sub("0", body)
        aggregate = aggregate.removeprefix(kind).strip()
        if not aggregate.startswith("{") or not aggregate.endswith("}"):
            raise ValueError("Expected static font aggregate: " + name)
        aggregate = re.sub(r"\.\w+\s*=|\b(?:true|false)\b", "", aggregate)
        if re.search(r"[^\s\d{},.\-+]", aggregate):
            raise ValueError("Unsupported or dynamic font initializer: " + name)
        return f"constinit const {kind} {name}{bounds or ''} = {body};"

    result = FONT_RESOURCE.sub(resource, content)
    if not resources:
        return content
    if FONT_SLICE_HELPER not in result:
        if "wfd_font_slice" in content:
            raise ValueError("Unexpected font slice helper definition")
        result = result.replace(HEADER, HEADER + "\n" + FONT_SLICE_HELPER, 1)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "source", type=Path, help="Generated dashboard.cpp to prepare in place"
    )
    parser.add_argument(
        "--output", type=Path, help="Prepared C++ output; defaults to input"
    )
    args = parser.parse_args()
    content = args.source.read_text(encoding="utf-8")
    result = prepare_font_resources(content)
    output = args.output or args.source
    previous = output.read_text(encoding="utf-8") if output.exists() else None
    if result != previous:
        temporary = output.with_suffix(output.suffix + ".tmp")
        temporary.write_text(result, encoding="utf-8", newline="\n")
        temporary.replace(output)


if __name__ == "__main__":
    main()
