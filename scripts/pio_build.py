"""PlatformIO must retain the Rust archive's exported C ABI symbols."""

from pathlib import Path
import hashlib
import os
import re
import json

Import("env")
root = Path(env.subst("$PROJECT_DIR"))
build = Path(env.subst("$BUILD_DIR"))
# Hosted Kconfig is also parsed by SCons ldgen, outside CMake's environment.
framework = Path(env.PioPlatform().get_package_dir("framework-espidf"))
version_source = (framework / "tools/cmake/version.cmake").read_text(encoding="utf-8")
sdk_parts = dict(
    re.findall(r"set\(IDF_VERSION_(MAJOR|MINOR)\s+(\d+)\)", version_source)
)
if set(sdk_parts) != {"MAJOR", "MINOR"}:
    raise ValueError("Cannot determine ESP-IDF major/minor version")
sdk_version = sdk_parts["MAJOR"] + "." + sdk_parts["MINOR"]
env["ENV"]["ESP_IDF_VERSION"] = sdk_version
os.environ["ESP_IDF_VERSION"] = sdk_version
archive = build / "slint-prebuilt/current/lib/libslint_cpp.a"
version = os.environ.get(
    "WFD_FIRMWARE_VERSION", env.GetProjectOption("custom_firmware_version")
).removeprefix("v")
if not re.fullmatch(r"\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?(?:\+[A-Za-z0-9.-]+)?", version):
    raise ValueError("Firmware version must be a semantic version")
args = env.BoardConfig().get("build.cmake_extra_args", "")
args = re.sub(r"-DPROJECT_VER=\S+", "-DPROJECT_VER=" + version, args)
hardware_id = env["PIOENV"]
label = env.GetProjectOption("custom_hardware_label")
if not re.fullmatch(r"[a-z0-9_]+", hardware_id) or len("wfd_" + hardware_id) >= 32:
    raise ValueError("Invalid environment identity")
args += " -DWFD_HARDWARE_ID=" + hardware_id
driver = env.GetProjectOption("custom_board_driver")
if not re.fullmatch(r"[a-z0-9_]+", driver):
    raise ValueError("Invalid board driver name")
args += " -DWFD_BOARD_DRIVER=" + driver
args += " -DWFD_DEPENDENCIES_LOCK=config/dependencies." + hardware_id + ".lock"
build.mkdir(parents=True, exist_ok=True)
generated = root / ".pio/generated" / hardware_id
generated.mkdir(parents=True, exist_ok=True)
flash_size = env.BoardConfig().get("upload.flash_size")
if flash_size not in ("16MB", "32MB"):
    raise ValueError("Unsupported flash size in hardware profile")
sdk_defaults = (root / "sdkconfig.defaults").read_text(encoding="utf-8")
sdk_fragments = []
for name in env.GetProjectOption("custom_sdkconfig_defaults", "").split():
    fragment = (root / name).resolve()
    if not fragment.is_relative_to(root.resolve()) or not fragment.is_file():
        raise ValueError("Invalid target SDK defaults fragment: " + name)
    sdk_fragments.append(fragment)
    sdk_defaults += "\n" + fragment.read_text(encoding="utf-8")

sdk_defaults += "\nCONFIG_ESPTOOLPY_FLASHSIZE_" + flash_size + "=y\n"
defaults_path = generated / "sdkconfig.defaults"
if (
    not defaults_path.exists()
    or defaults_path.read_text(encoding="utf-8") != sdk_defaults
):
    defaults_path.write_text(sdk_defaults, encoding="utf-8", newline="\n")
args += " -DSDKCONFIG_DEFAULTS=" + defaults_path.as_posix()
metadata = (
    "#pragma once\n#define HW_TYPE "
    + json.dumps(hardware_id)
    + "\n#define WFD_HARDWARE_LABEL "
    + json.dumps(label)
    + "\n"
)
metadata_path = generated / "hardware_metadata.h"
if not metadata_path.exists() or metadata_path.read_text() != metadata:
    metadata_path.write_text(metadata)
env.BoardConfig().update("build.cmake_extra_args", args)

# PlatformIO uses SCons rather than Ninja to decide when CMake needs to rerun.
# CMAKE_CONFIGURE_DEPENDS alone therefore misses changes to imported UI/fonts.
inputs = sorted(p for p in (root / "ui").rglob("*") if p.is_file())
inputs += [
    root / "firmware/main/CMakeLists.txt",
    root / "firmware/main/idf_component.yml",
    root / "CMakeLists.txt",
    root / "platformio.ini",
    root / "partitions.csv",
    root / "scripts/prepare_slint_fonts.py",
    root / "scripts/pio_toolchain.py",
]
inputs += sdk_fragments
inputs += sorted((root / "firmware/components/slint_runtime").glob("*.cmake"))
inputs += [root / "firmware/components/slint_runtime/CMakeLists.txt"]
digest = hashlib.sha256()
digest.update(version.encode())
for path in inputs:
    digest.update(str(path.relative_to(root)).encode())
    digest.update(path.read_bytes())
stamp = build / "wfd-inputs.sha256"
fingerprint = digest.hexdigest()
build.mkdir(parents=True, exist_ok=True)
if not stamp.exists() or stamp.read_text() != fingerprint:
    (build / "CMakeCache.txt").unlink(missing_ok=True)
env["WFD_INPUT_STAMP"] = str(stamp)
env["WFD_INPUT_FINGERPRINT"] = fingerprint

# sdkconfig.defaults is the maintained source of SDK settings in this project.
defaults = hashlib.sha256(sdk_defaults.encode("utf-8")).hexdigest()
config_stamp = build / "wfd-sdk-defaults.sha256"
if not config_stamp.exists() or config_stamp.read_text() != defaults:
    (root / ("sdkconfig." + env["PIOENV"])).unlink(missing_ok=True)
    (build / "CMakeCache.txt").unlink(missing_ok=True)
    (build / "bootloader/CMakeCache.txt").unlink(missing_ok=True)
env["WFD_SDK_STAMP"] = str(config_stamp)
env["WFD_SDK_FINGERPRINT"] = defaults
link = f"-Wl,--whole-archive {archive.as_posix()} -Wl,--no-whole-archive"
env.Append(
    LINKFLAGS=["${" + repr(link) + " if 'bootloader' not in str(TARGETS[0]) else ''}"]
)
