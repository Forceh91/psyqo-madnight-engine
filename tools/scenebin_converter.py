#!/usr/bin/env python3
"""
scenebin_converter.py

Converts a human-editable scene manifest source file into the binary
SCENEBIN format described in SCENEBIN.md.

Source format (plain text, one entry per line):

    # comments and blank lines are ignored
    sfx SFX/FCNTNA.VAG
    texture TEXTURES/LOGO.TIM 320 0 0 240
    texture UI/CS_UI.TIM 320 129 0 241
    object MODELS/SBSKT.MB
    object MODELS/SCART.MB

Usage:
    python scenebin_converter.py source.txt output.bin --asset-root assets/
"""

import argparse
import struct
import sys
from pathlib import Path

# Must match LoadFileType in C++. SCENE (9999) is intentionally omitted --
# the wire format's type field is a uint8_t and can't represent it yet.
TYPE_MAP = {
    "object": 0,
    "texture": 1,
    "mod_file": 2,
    "animation": 3,
    "colbin": 4,
    "vag": 5,
}

MAX_ARCHIVE_FILE_NAME_LEN = 255  # keep in sync with the C++ constant

VRAM_WIDTH = 1024
VRAM_HEIGHT = 512

MAGIC = b"SCENEBIN"


class SourceError(Exception):
    """Raised for problems in the human-editable source file."""


def parse_source(path: Path):
    entries = []

    with path.open("r", encoding="utf-8") as f:
        for lineno, raw_line in enumerate(f, start=1):
            line = raw_line.strip()

            if not line or line.startswith("#"):
                continue

            fields = line.split()
            type_name = fields[0].lower()

            if type_name not in TYPE_MAP:
                raise SourceError(
                    f"line {lineno}: unknown type '{fields[0]}' "
                    f"(expected one of: {', '.join(TYPE_MAP)})"
                )

            if type_name == "scene":
                raise SourceError(
                    f"line {lineno}: 'scene' entries are not supported yet "
                    f"(SCENE doesn't fit the uint8_t type field)"
                )

            if len(fields) < 2:
                raise SourceError(f"line {lineno}: missing file path")

            name = fields[1]
            extra_fields = fields[2:]

            if len(name) > MAX_ARCHIVE_FILE_NAME_LEN:
                raise SourceError(
                    f"line {lineno}: name '{name}' is {len(name)} chars, "
                    f"exceeds MAX_ARCHIVE_FILE_NAME_LEN ({MAX_ARCHIVE_FILE_NAME_LEN})"
                )

            entry = {"type": type_name, "name": name, "lineno": lineno}

            if type_name == "texture":
                if len(extra_fields) != 4:
                    raise SourceError(
                        f"line {lineno}: texture requires 4 fields "
                        f"(vramX vramY clutX clutY), got {len(extra_fields)}"
                    )
                try:
                    vram_x, vram_y, clut_x, clut_y = (int(v) for v in extra_fields)
                except ValueError:
                    raise SourceError(
                        f"line {lineno}: texture coordinates must be integers"
                    )

                for label, val, limit in (
                    ("vramX", vram_x, VRAM_WIDTH),
                    ("clutX", clut_x, VRAM_WIDTH),
                    ("vramY", vram_y, VRAM_HEIGHT),
                    ("clutY", clut_y, VRAM_HEIGHT),
                ):
                    if not (0 <= val < limit):
                        raise SourceError(
                            f"line {lineno}: {label}={val} out of range "
                            f"(expected 0..{limit - 1})"
                        )

                entry["placement"] = (vram_x, vram_y, clut_x, clut_y)
            else:
                if extra_fields:
                    raise SourceError(
                        f"line {lineno}: '{type_name}' takes no extra fields, "
                        f"got {len(extra_fields)}"
                    )

            entries.append(entry)

    if len(entries) > 255:
        raise SourceError(
            f"too many entries ({len(entries)}) — fileCount is a uint8_t, max 255"
        )

    return entries


def validate_paths(entries, asset_root: Path):
    missing = []
    for entry in entries:
        full_path = asset_root / entry["name"]
        if not full_path.exists():
            missing.append(f"line {entry['lineno']}: {full_path}")

    if missing:
        raise SourceError(
            "referenced files not found on disk:\n  " + "\n  ".join(missing)
        )


def pack_binary(entries) -> bytes:
    out = bytearray()
    out += MAGIC
    out += struct.pack("<B", len(entries))

    for entry in entries:
        type_val = TYPE_MAP[entry["type"]]
        name_bytes = entry["name"].encode("ascii")

        out += struct.pack("<BB", type_val, len(name_bytes))
        out += name_bytes

        if entry["type"] == "texture":
            vram_x, vram_y, clut_x, clut_y = entry["placement"]
            out += struct.pack("<HHHH", vram_x, vram_y, clut_x, clut_y)

    return bytes(out)


def main():
    parser = argparse.ArgumentParser(description="Convert a scene manifest source file to SCENEBIN.")
    parser.add_argument("source", type=Path, help="Path to the human-editable source file")
    parser.add_argument("output", type=Path, help="Path to write the binary SCENEBIN file")
    parser.add_argument(
        "--asset-root",
        type=Path,
        default=None,
        help="Root directory to validate referenced paths against (skipped if omitted)",
    )

    args = parser.parse_args()

    try:
        entries = parse_source(args.source)

        if args.asset_root is not None:
            validate_paths(entries, args.asset_root)

        binary = pack_binary(entries)
        args.output.write_bytes(binary)

    except SourceError as e:
        print(f"scenebin_converter: error: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"Wrote {len(entries)} entries to {args.output} ({len(binary)} bytes)")


if __name__ == "__main__":
    main()
