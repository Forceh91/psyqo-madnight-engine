#!/usr/bin/env python3
"""Run every scene-manifest example in the docs through the converter.

The docs used to ship a worked example whose very first line used a keyword the
converter does not accept. It was copied into three files and none of them had
ever been run. This catches that.

Any fenced code block in the searched files whose non-comment lines all start
with a keyword the converter knows is treated as a manifest and converted. A
block that fails to convert fails this script.

Usage:
    python3 tools/check_doc_examples.py [paths...]

With no arguments it checks the SCENEBIN guide and this repository's tool
docstrings.
"""

import re
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CONVERTER = REPO / "tools" / "scenebin_converter.py"

DEFAULT_TARGETS = [
    REPO / "docs-site" / "docs" / "guides" / "scenebin.md",
    REPO / "tools" / "scenebin_converter.py",
]

# Keep in sync with TYPE_MAP in scenebin_converter.py.
KEYWORDS = {"object", "texture", "mod_file", "animation", "colbin", "vag"}

FENCE = re.compile(r"^```[a-zA-Z]*\n(.*?)^```", re.MULTILINE | re.DOTALL)


def looks_like_manifest(block):
    """Identify a manifest WITHOUT requiring every keyword to be valid.

    Requiring all keywords to be known would make this blind to the one bug it
    exists to catch: a block containing an invalid keyword would simply not be
    recognised as a manifest and would be skipped rather than failed. So the
    test is structural (every line is `word path ...`) plus at least one line
    using a keyword we know. An unknown keyword then reaches the converter and
    fails there, which is the point.
    """
    lines = [l.strip() for l in block.splitlines()]
    lines = [l for l in lines if l and not l.startswith("#")]
    if not lines:
        return False
    if not all(len(l.split()) >= 2 for l in lines):
        return False
    return any(l.split()[0] in KEYWORDS for l in lines)


def indented_blocks(text):
    """Docstrings indent their examples rather than fencing them."""
    run = []
    for line in text.splitlines():
        if line.startswith("    ") and line.strip():
            run.append(line[4:])
        else:
            if run:
                yield "\n".join(run)
                run = []
    if run:
        yield "\n".join(run)


def blocks_in(path):
    text = path.read_text()
    seen = set()
    for block in FENCE.findall(text):
        seen.add(block)
        yield block
    for block in indented_blocks(text):
        if block not in seen:
            yield block


def main(argv):
    targets = [Path(a) for a in argv[1:]] or DEFAULT_TARGETS
    checked = 0
    failed = 0

    for path in targets:
        if not path.exists():
            print(f"missing: {path}", file=sys.stderr)
            failed += 1
            continue

        for block in blocks_in(path):
            if not looks_like_manifest(block):
                continue
            checked += 1
            rel = path.relative_to(REPO)
            with tempfile.TemporaryDirectory() as tmp:
                src = Path(tmp) / "manifest.txt"
                out = Path(tmp) / "out.scenebin"
                src.write_text(block if block.endswith("\n") else block + "\n")
                result = subprocess.run(
                    [sys.executable, str(CONVERTER), str(src), str(out)],
                    capture_output=True,
                    text=True,
                )
                if result.returncode != 0:
                    failed += 1
                    print(f"FAIL {rel}", file=sys.stderr)
                    print(f"  {result.stderr.strip()}", file=sys.stderr)
                    for line in block.strip().splitlines():
                        print(f"  | {line}", file=sys.stderr)
                else:
                    print(f"ok   {rel} ({out.stat().st_size} bytes)")

    if checked == 0:
        print("no manifest examples found - the extractor is probably broken",
              file=sys.stderr)
        return 1

    print(f"\n{checked} example(s) checked, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
