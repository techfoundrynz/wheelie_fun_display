"""Keep clean PlatformIO builds on the board-selected compiler architecture."""

from pathlib import Path
import runpy
from types import MethodType
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/pio_toolchain.py"


class BoardConfig(dict):
    def get(self, key, default=...):
        if key not in self and default is ...:
            raise KeyError(key)
        return super().get(key, default)


class Platform:
    def get_package_dir(self, name):
        return "/packages/" + name


class BuildEnvironment(dict):
    def __init__(self, prefix):
        super().__init__()
        self.prefix = prefix
        self.build_calls = 0
        self.fail = False
        self.platform = Platform()

    def BoardConfig(self):
        return BoardConfig(
            {"build.toolchain": self.prefix, "build.mcu": "esp32p4"}
            if self.prefix
            else {"build.mcu": "esp32s3"}
        )

    def PioPlatform(self):
        return self.platform

    def Replace(self, **values):
        self.update(values)

    def Append(self, **values):
        for key, value in values.items():
            self.setdefault(key, []).extend(value)

    def AddMethod(self, method, name):
        setattr(self, name, MethodType(method, self))

    def BuildProgram(self):
        self.build_calls += 1
        if self.fail:
            raise RuntimeError("framework configuration failed")
        return dict(
            self,
            linker_toolchain=self.platform.get_package_dir("toolchain-xtensa-esp-elf"),
        )


class BoardToolchainTests(unittest.TestCase):
    def configure(self, prefix):
        env = BuildEnvironment(prefix)
        runpy.run_path(str(SCRIPT), init_globals={"Import": lambda _: None, "env": env})
        # Simulate the platform builder's later, incorrect architecture assignment.
        env.Replace(CC="xtensa-esp32p4-elf-gcc", CXX="xtensa-esp32p4-elf-g++")
        env.Replace(ELF2BINFLAGS=["--flash-mode", "dio"])
        return env

    def test_p4_tools_are_selected_before_framework_build(self):
        env = self.configure("riscv32-esp")
        snapshot = env.BuildProgram()
        self.assertEqual(
            snapshot["linker_toolchain"], "/packages/toolchain-riscv32-esp"
        )
        self.assertEqual(snapshot["CC"], "riscv32-esp-elf-gcc")
        self.assertEqual(snapshot["CXX"], "riscv32-esp-elf-g++")
        self.assertEqual(snapshot["AR"], "riscv32-esp-elf-gcc-ar")
        self.assertEqual(snapshot["SIZETOOL"], "riscv32-esp-elf-size")
        self.assertEqual(
            snapshot["ELF2BINFLAGS"],
            ["--flash-mode", "dio", "$WFD_IMAGE_REVISION_FLAGS"],
        )
        self.assertEqual(env.build_calls, 1)
        self.assertEqual(
            env.platform.get_package_dir("toolchain-xtensa-esp-elf"),
            "/packages/toolchain-xtensa-esp-elf",
        )

    def test_package_lookup_is_restored_after_configuration_failure(self):
        env = self.configure("riscv32-esp")
        env.fail = True
        with self.assertRaisesRegex(RuntimeError, "framework configuration failed"):
            env.BuildProgram()
        self.assertEqual(
            env.platform.get_package_dir("toolchain-xtensa-esp-elf"),
            "/packages/toolchain-xtensa-esp-elf",
        )

    def test_ordinary_board_keeps_platform_selection(self):
        env = self.configure(None)
        self.assertEqual(env.BuildProgram()["CC"], "xtensa-esp32p4-elf-gcc")
        self.assertEqual(env["ELF2BINFLAGS"], ["--flash-mode", "dio"])
        self.assertEqual(env.build_calls, 1)
        self.assertEqual(
            env.platform.get_package_dir("toolchain-xtensa-esp-elf"),
            "/packages/toolchain-xtensa-esp-elf",
        )


if __name__ == "__main__":
    unittest.main()
