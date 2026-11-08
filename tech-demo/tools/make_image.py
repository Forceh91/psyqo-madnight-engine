#!/usr/bin/env python3
#
# Packs the demo's assets into a raw 2352-byte-sector image with a psyqo-paths
# archive at sector 23, which is where ArchiveHelper looks for one. A PCDRV build
# reads this file straight off the host, so CI can boot the demo without
# building a disc:
#
#   python3 tech-demo/tools/make_image.py tech-demo/assets tech-demo/build/techdemo.bin
#
# It is not a bootable disc (no ISO 9660 volume, no executable on it). For a
# real disc use pcsx-redux's authoring tool with tech-demo/toc.json, which puts
# the same archive at the same sector.
#
# Files are stored uncompressed and named by their path relative to the asset
# directory, e.g. MODELS/HALL.MB.

import os
import struct
import sys

SECTOR = 2352
DATA = 2048
HEADER = 24  # sync + header + subheader of a mode 2 form 1 sector
ARCHIVE_LBA = 23


def djb_hash(name):
    h = 5381
    for c in name.encode():
        h = ((h << 5) + h ^ c) & 0xFFFFFFFFFFFFFFFF
    return h


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: %s <asset dir> <output image>" % sys.argv[0])
    root, output = sys.argv[1:]

    files = []
    for dirpath, _, names in os.walk(root):
        for name in names:
            path = os.path.join(dirpath, name)
            files.append((os.path.relpath(path, root).replace(os.sep, "/"), path))
    files.sort()

    index_sectors = ((len(files) + 1) * 16 + DATA - 1) // DATA
    sector = ARCHIVE_LBA + index_sectors
    entries = []
    blobs = []
    for name, path in files:
        with open(path, "rb") as f:
            data = f.read()
        count = (len(data) + DATA - 1) // DATA
        if len(data) >= 1 << 21 or count >= 1 << 10:
            sys.exit("%s is too big for an archive entry" % name)
        low = len(data)  # decompressed size, no padding
        high = sector | count << 19  # method 0, stored
        entries.append((djb_hash(name), low, high))
        blobs.append(data.ljust(count * DATA, b"\0"))
        sector += count
        print("%-24s %6d bytes at sector %d" % (name, len(data), sector - count))

    entries.sort()
    index = bytearray(b"PSX-ARC1")
    index += struct.pack("<I", len(files))  # 24-bit count, decompression margin 0
    index += struct.pack("<I", sector)
    for h, low, high in entries:
        index += struct.pack("<QII", h, low, high)
    index = bytes(index).ljust(index_sectors * DATA, b"\0")

    payload = b"\0" * (ARCHIVE_LBA * DATA) + index + b"".join(blobs)
    os.makedirs(os.path.dirname(os.path.abspath(output)), exist_ok=True)
    with open(output, "wb") as f:
        for i in range(0, len(payload), DATA):
            f.write(b"\0" * HEADER + payload[i:i + DATA] + b"\0" * (SECTOR - HEADER - DATA))


if __name__ == "__main__":
    main()
