"""Keep clean PlatformIO builds on the board-selected compiler architecture."""

from pathlib import Path
import runpy
from types import MethodType
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/pio_toolchain.py"


class BuildEnvironment(dict):
    def __init__(self, prefix):
        super().__init__()
        self.prefix = prefix
        self.build_calls = 0

    def BoardConfig(self):
        return {"build.toolchain": self.prefix}

    def Replace(self, **values):
        self.update(values)

    def AddMethod(self, method, name):
        setattr(self, name, MethodType(method, self))

    def BuildProgram(self):
        self.build_calls += 1
        return dict(self)


class BoardToolchainTests(unittest.TestCase):
    def configure(self, prefix):
        env = BuildEnvironment(prefix)
        runpy.run_path(str(SCRIPT), init_globals={"Import": lambda _: None, "env": env})
        # Simulate the platform builder's later, incorrect architecture assignment.
        env.Replace(CC="xtensa-esp32p4-elf-gcc", CXX="xtensa-esp32p4-elf-g++")
        return env

    def test_p4_tools_are_selected_before_framework_build(self):
        env = self.configure("riscv32-esp")
        snapshot = env.BuildProgram()
        self.assertEqual(snapshot["CC"], "riscv32-esp-elf-gcc")
        self.assertEqual(snapshot["CXX"], "riscv32-esp-elf-g++")
        self.assertEqual(snapshot["AR"], "riscv32-esp-elf-gcc-ar")
        self.assertEqual(snapshot["SIZETOOL"], "riscv32-esp-elf-size")
        self.assertEqual(env.build_calls, 1)

    def test_ordinary_board_keeps_platform_selection(self):
        env = self.configure(None)
        self.assertEqual(env.BuildProgram()["CC"], "xtensa-esp32p4-elf-gcc")
        self.assertEqual(env.build_calls, 1)


if __name__ == "__main__":
    unittest.main()
