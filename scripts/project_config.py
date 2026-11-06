"""Shared repository metadata; supported targets are PlatformIO's default_envs."""

from configparser import ConfigParser
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[1]


def config():
    value = ConfigParser(interpolation=None)
    value.read(ROOT / "platformio.ini")
    return value


def environments():
    return [
        item
        for item in re.split(r"[,\s]+", config()["platformio"]["default_envs"].strip())
        if item
    ]


def version():
    return config()["common"]["custom_firmware_version"].strip()


def option(environment, name):
    """Resolve a profile option through PlatformIO's explicit extends chain."""
    values = config()

    def resolve(section, visited):
        if section in visited:
            raise ValueError("Cyclic PlatformIO inheritance")
        if name in values[section]:
            return values[section][name].strip()
        for parent in re.split(r"[,\s]+", values[section].get("extends", "").strip()):
            if parent:
                result = resolve(parent, visited | {section})
                if result is not None:
                    return result
        return None

    return resolve("env:" + environment, set())


if __name__ == "__main__":
    import sys

    print(json.dumps(environments()) if "--envs" in sys.argv else version())
