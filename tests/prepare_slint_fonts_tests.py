"""Check scoped flash placement preparation for generated Slint resources."""

import importlib.util
import unittest
import subprocess
import sys
import tempfile
from pathlib import Path

SPEC = importlib.util.spec_from_file_location(
    "prepare_slint_fonts",
    Path(__file__).resolve().parents[1] / "scripts" / "prepare_slint_fonts.py",
)
FONTS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FONTS)
HEADER = '#include "dashboard.h"\n'
RESOURCE = "slint_embedded_resource_0"
TYPE = "slint::cbindgen_private::"


class FontPreparationTests(unittest.TestCase):
    def test_separate_output_preserves_raw_and_last_good_output(self):
        raw = HEADER + f"const {TYPE}BitmapGlyph {RESOURCE}_glyphs[0] = {{ }};\n"
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "raw.cpp"
            output = Path(directory) / "prepared.cpp"
            source.write_text(raw, encoding="utf-8")
            command = [
                sys.executable,
                str(SPEC.origin),
                str(source),
                "--output",
                str(output),
            ]
            subprocess.run(command, check=True, capture_output=True)
            self.assertEqual(source.read_text(encoding="utf-8"), raw)
            prepared = output.read_text(encoding="utf-8")
            self.assertIn("constinit const", prepared)
            source.write_text(raw.replace("{ }", "dynamic_font()"), encoding="utf-8")
            result = subprocess.run(command, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(output.read_text(encoding="utf-8"), prepared)

    def test_all_font_types_and_empty_arrays(self):
        source = HEADER + (
            f"const {TYPE}BitmapGlyph {RESOURCE}_glyphs[0] = {{ }};\n"
            f"const {TYPE}BitmapGlyphs {RESOURCE}_sets[1] = {{ {{ .pixel_size = 14, "
            f".glyph_data = slint::private_api::make_slice({RESOURCE}_glyphs, 0) }} }};\n"
            f"const {TYPE}BitmapFont {RESOURCE} = {TYPE}BitmapFont {{ "
            f".glyphs = slint::private_api::make_slice({RESOURCE}_sets , 1), .italic = false }};\n"
        )
        result = FONTS.prepare_font_resources(source)
        self.assertEqual(result.count("constinit const"), 3)
        self.assertIn(f"wfd_font_slice({RESOURCE}_glyphs, 0)", result)
        self.assertNotIn("make_slice", result)
        self.assertEqual(result, FONTS.prepare_font_resources(result))

    def test_non_font_resources_and_runtime_slices_unchanged(self):
        source = HEADER + (
            f"const {TYPE}Texture {RESOURCE} = {{ .data = "
            f"slint::private_api::make_slice({RESOURCE}_data, 1) }};\n"
            "auto runtime_slice = slint::private_api::make_slice(pointer, length);\n"
        )
        self.assertEqual(source, FONTS.prepare_font_resources(source))

    def test_rejects_dynamic_slice(self):
        source = HEADER + (
            f"const {TYPE}BitmapGlyph {RESOURCE}[1] = {{ {{ .data = "
            "slint::private_api::make_slice(pointer, length) } };\n"
        )
        with self.assertRaisesRegex(ValueError, "slice initializer"):
            FONTS.prepare_font_resources(source)

    def test_rejects_dynamic_aggregate_and_calls(self):
        for body in ("load_font()", "{ .weight = get_weight() }"):
            with self.subTest(body=body), self.assertRaises(ValueError):
                FONTS.prepare_font_resources(
                    HEADER + f"const {TYPE}BitmapFont {RESOURCE} = {body};\n"
                )

    def test_rejects_unknown_definition_format(self):
        source = HEADER + f"constexpr {TYPE}BitmapGlyph {RESOURCE}[] = {{ }};\n"
        with self.assertRaisesRegex(ValueError, "definition"):
            FONTS.prepare_font_resources(source)

    def test_requires_expected_include(self):
        for source in ("", '#include "other.h"\n', HEADER + HEADER):
            with (
                self.subTest(source=source),
                self.assertRaisesRegex(ValueError, "dashboard.h"),
            ):
                FONTS.prepare_font_resources(source)


if __name__ == "__main__":
    unittest.main()
