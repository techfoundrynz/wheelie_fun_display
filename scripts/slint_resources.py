"""Prepare generated fonts before compilation and reject writable linked metadata."""

from pathlib import Path
import json
import re
import subprocess
from elftools.elf.elffile import ELFFile

Import("env")
root = Path(env.subst("$PROJECT_DIR"))
build = Path(env.subst("$BUILD_DIR"))
generated = build / "esp-idf/main/generated"
source = generated / "dashboard.cpp"

# SCons can reuse CMake metadata after a failed configure. Always prepare the
# matching raw snapshot published with dashboard.h before scanning/compiling.
# CMake fingerprints all three outputs and repairs mismatches on configuration.
subprocess.run(
    [
        env.subst("$PYTHONEXE"),
        str(root / "scripts/prepare_slint_fonts.py"),
        str(generated / "dashboard-raw.cpp"),
        "--output",
        str(source),
    ],
    check=True,
)


for stamp_key, value_key in (
    ("WFD_INPUT_STAMP", "WFD_INPUT_FINGERPRINT"),
    ("WFD_SDK_STAMP", "WFD_SDK_FINGERPRINT"),
):
    Path(env[stamp_key]).write_text(env[value_key], encoding="utf-8")


def check_fonts(target, source, env):
    generated_cpp = generated / "dashboard.cpp"
    names = set(
        re.findall(
            r"^constinit const slint::cbindgen_private::(?:BitmapGlyph|BitmapGlyphs|BitmapFont) (slint_embedded_resource_\w+)",
            generated_cpp.read_text(encoding="utf-8"),
            re.MULTILINE,
        )
    )
    if not names:
        raise RuntimeError("No prepared Slint font resources found")
    sdk = json.loads((build / "config/sdkconfig.json").read_text(encoding="utf-8"))
    boundaries = (
        {"_instruction_reserved_end", "_rodata_reserved_end"}
        if sdk.get("IDF_TARGET") == "esp32p4" and sdk.get("SPIRAM_XIP_FROM_PSRAM")
        else set()
    )
    with Path(str(target[0])).open("rb") as stream:
        elf = ELFFile(stream)
        symbols = {
            symbol.name: symbol
            for symbol in elf.get_section_by_name(".symtab").iter_symbols()
            if symbol.name in names or symbol.name in boundaries
        }
        if not names | boundaries <= symbols.keys():
            raise RuntimeError("Linked Slint font metadata is missing")
        # Reject stale XiP layouts that leave executable bytes in the writable gap.
        for name in boundaries:
            if symbols.pop(name)["st_value"] % sdk["SOC_CPU_PMP_REGION_GRANULARITY"]:
                raise RuntimeError(f"XiP PMP boundary is unaligned: {name}")
        for name, symbol in symbols.items():
            section = elf.get_section(symbol["st_shndx"])
            start = section["sh_addr"]
            end = start + section["sh_size"]
            if section.name != ".flash.rodata" or not (
                start <= symbol["st_value"]
                and symbol["st_value"] + symbol["st_size"] <= end
            ):
                raise RuntimeError(
                    f"Slint font metadata must be flash mapped: {name} in {section.name}"
                )
        size = sum(symbol["st_size"] for symbol in symbols.values())
    print(
        f"Slint font placement verified: {len(names)} resources, {size} bytes in flash"
    )


env.AddPostAction(
    "$BUILD_DIR/${PROGNAME}.elf",
    env.VerboseAction(check_fonts, "Checking Slint font placement"),
)
