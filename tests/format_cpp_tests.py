"""Regression coverage for declaration spacing beyond clang-format's options."""

import importlib.util
import unittest
from pathlib import Path

SPEC = importlib.util.spec_from_file_location(
    "format_cpp", Path(__file__).resolve().parents[1] / "scripts" / "format_cpp.py"
)
FORMATTER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FORMATTER)


class DeclarationSpacingTests(unittest.TestCase):
    def test_namespace_and_member_declarations(self):
        source = "namespace bike {\nclass Bike {\npublic:\nvoid ride();\nvoid stop();\n};\n}\n"
        result = FORMATTER.separate_declarations(source)
        self.assertIn("void ride();\n\nvoid stop();\n\n", result)
        self.assertEqual(result, FORMATTER.separate_declarations(result))

    def test_executable_statements_are_unchanged(self):
        source = "void ride() {\nstart();\nstop();\n}\n"
        self.assertEqual(source, FORMATTER.separate_declarations(source))

    def test_comments_remain_attached(self):
        source = "int count;\n// Starts riding.\nvoid ride();\n/* Stops riding. */\nvoid stop();\n"
        expected = "int count;\n\n// Starts riding.\nvoid ride();\n\n/* Stops riding. */\nvoid stop();\n"
        self.assertEqual(expected, FORMATTER.separate_declarations(source))

    def test_multiline_declarations_and_nonfunction_members(self):
        source = "class Bike {\nvoid ride(\nint mode);\nstd::function<void()> callback;\nint count;\n};\n"
        result = FORMATTER.separate_declarations(source)
        self.assertIn(
            "int mode);\n\nstd::function<void()> callback;\nint count;", result
        )

    def test_strings_comments_and_preprocessor_are_unchanged(self):
        source = '#define CALL() do_work();\n// void ride();\nconst char *text = "void ride();";\n'
        self.assertEqual(source, FORMATTER.separate_declarations(source))

    def test_initializers_are_not_function_declarations(self):
        source = "int width = display_width(orientation);\nint height = display_height(orientation);\n"
        self.assertEqual(source, FORMATTER.separate_declarations(source))

    def test_linkage_blocks_and_trailing_return_types(self):
        source = 'extern "C" {\nvoid start();\nvoid stop();\n}\nauto speed() -> int;\nauto limit() -> int;\n'
        result = FORMATTER.separate_declarations(source)
        self.assertIn("void start();\n\nvoid stop();", result)
        self.assertIn("auto speed() -> int;\n\nauto limit() -> int;", result)


if __name__ == "__main__":
    unittest.main()
