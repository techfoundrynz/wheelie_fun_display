"""Format first-party C/C++ and separate function declarations with blank lines.

clang-format handles definitions and namespaces. Its declaration spacing is not
configurable, so this wrapper adds gaps around function declarations at namespace
and class scope, using tokens to exclude comments, strings and function bodies.
"""

import argparse
import difflib
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIRS = ("firmware/main", "firmware/components/bike", "tests")
SUFFIXES = {".c", ".cpp", ".h", ".hpp"}
TOKEN = re.compile(
    r'//[^\n]*|/\*.*?\*/|R"([^ ()\\\t\r\n]*)\(.*?\)\1"|'
    r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|'
    r"^[ \t]*\#(?:[^\n\\]|\\[^\n]|\\\n)*|[A-Za-z_]\w*|\S",
    re.DOTALL | re.MULTILINE,
)
DECLARATION_END = re.compile(
    r"\)\s*(?:(?:const|volatile|override|final|noexcept)\s*|&&?\s*)*"
    r"(?:-\s*>\s*[^;]+)?(?:=\s*(?:0|default|delete)\s*)?;$"
)


def separate_declarations(source: str) -> str:
    """Add declaration gaps only in namespace/record scopes, never executable code."""
    scopes = [True]
    statement = []
    boundaries = set()
    for match in TOKEN.finditer(source):
        token = match.group()
        if token.startswith(("//", "/*", "#")):
            continue
        if token == "{":
            prefix = " ".join(value for value, _ in statement)
            record = re.search(
                r'\b(?:namespace|class|struct|union)\b|\bextern\s+"C"', prefix
            )
            scopes.append(scopes[-1] and bool(record) and "(" not in prefix)
            statement = []
        elif token == "}":
            if len(scopes) > 1:
                scopes.pop()
            statement = []
        elif token == ";":
            statement.append((token, match.start()))
            text = " ".join(value for value, _ in statement)
            depth = 0
            initializer = False
            for index, (value, _) in enumerate(statement):
                if value == "(":
                    depth += 1
                elif value == ")":
                    depth -= 1
                elif value == "=" and not depth:
                    previous = statement[index - 1][0] if index else ""
                    following = statement[index + 1][0]
                    special_member = (
                        following in {"0", "default", "delete"}
                        and statement[index + 2][0] == ";"
                    )
                    initializer |= previous != "operator" and not special_member
            if scopes[-1] and not initializer and DECLARATION_END.search(text):
                # Access specifiers are not part of the following declaration.
                start = statement[0][1]
                for index, (value, _) in enumerate(statement[:-1]):
                    if (
                        value == ":"
                        and index
                        and statement[index - 1][0]
                        in {"public", "private", "protected"}
                    ):
                        start = statement[index + 1][1]
                start_line = source.count("\n", 0, start)
                # Keep documentation attached to its declaration.
                lines = source[:start].splitlines()
                while start_line and lines[start_line - 1].lstrip().startswith("//"):
                    start_line -= 1
                if start_line and lines[start_line - 1].rstrip().endswith("*/"):
                    start_line -= 1
                    while start_line and "/*" not in lines[start_line]:
                        start_line -= 1
                boundaries.add(start_line)
                boundaries.add(source.count("\n", 0, match.end()) + 1)
            statement = []
        else:
            statement.append((token, match.start()))
    lines = source.splitlines()
    output = []
    for index, line in enumerate(lines):
        if index in boundaries and line.strip() and output and output[-1].strip():
            output.append("")
        output.append(line)
    return "\n".join(output) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--clang-format", default="clang-format")
    args = parser.parse_args()
    changed = False
    for directory in SOURCE_DIRS:
        for path in sorted((ROOT / directory).rglob("*")):
            if path.suffix not in SUFFIXES:
                continue
            original = path.read_text(encoding="utf-8")
            result = subprocess.run(
                [args.clang_format, "--style=file", f"--assume-filename={path}"],
                input=original,
                text=True,
                encoding="utf-8",
                capture_output=True,
                check=True,
            )
            formatted = separate_declarations(result.stdout)
            if formatted == original:
                continue
            changed = True
            if args.check:
                print(
                    "".join(
                        difflib.unified_diff(
                            original.splitlines(keepends=True),
                            formatted.splitlines(keepends=True),
                            fromfile=str(path.relative_to(ROOT)),
                            tofile="formatted",
                        )
                    ),
                    end="",
                )
            else:
                path.write_text(formatted, encoding="utf-8", newline="\n")
                print(path.relative_to(ROOT))
    return int(changed and args.check)


if __name__ == "__main__":
    raise SystemExit(main())
