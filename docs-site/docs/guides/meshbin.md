---
title: MESHBIN Format
sidebar_position: 2
---

# MESHBIN

Mesh geometry, optionally with a skeleton. Produced by `tools/obj-to-meshbin.py`, consumed by
`MeshManager::LoadMesh`.

Every field is little-endian. The reader `memcpy`s field by field rather than casting the
buffer to a struct, so there is no implicit padding anywhere: the wire layout is exactly the
sum of the field widths below.

## Fixed-point conventions

Three different conventions appear in this file and they are not interchangeable.

| Data | Convention | Meaning |
|---|---|---|
| Positions (vertices, AABB, bounding sphere, bone `localPos`) | x128 integer | 128 units = one engine metre, matching `ONE_METRE` in `world_defs.hh`. The exporter multiplies by `ONE_ENGINE_METRE = 128`. |
| Bone `localRotation` | FP12 (x4096) | A `psyqo::GTE::Short`, i.e. `FixedPoint<12, int16_t>`. |
| UVs | raw texel bytes | 0-255, no scaling. |

Position values are copied straight into the `.value` of a `psyqo::Vec3`, whose nominal
precision is 12 bits. The reader never rescales, so the x128 convention is what actually
round-trips. Do not assume a position field behaves like a 12-bit fixed-point number just
because its C++ type says so: `FixedPoint::integer()` on one of these divides by 4096, not
128, and will return 0 for anything under 32 metres.

## Header

| Offset | Field | Type | Bytes | Notes |
|---|---|---|---|---|
| 0x00 | magic | char[7] | 7 | `MESHBIN`, no terminator. Mismatch aborts the load. |
| 0x07 | version | uint8_t | 1 | The exporter emits **3**. See the version gating below. |
| 0x08 | type | uint8_t | 1 | Parsed and discarded. `MeshBin::type` is never assigned and nothing in the engine reads it. The exporter always writes 1. |
| 0x09 | vertexCount | uint32_t | 4 | |
| 0x0D | indicesCount | uint32_t | 4 | Number of face index groups. The exporter derives this from the same counter as `facesCount`, so in practice the two are always equal. |
| 0x11 | facesCount | uint32_t | 4 | Rejected if `>= MAX_FACES_PER_MESH` (1000). |
| 0x15 | normalsCount | uint32_t | 4 | |
| 0x19 | uvCount | uint32_t | 4 | |
| 0x1D | hasSkeleton | uint8_t | 1 | **Version 2 and later only.** |
| 0x1E | numBones | uint8_t | 1 | **Version 2 and later only.** |

The two skeleton bytes are read only when `version > 1`. For a version 0 or 1 file they are
absent from the stream entirely and every subsequent offset shifts down by two.

## Body

In wire order after the header.

| Field | Type | Size | Present when |
|---|---|---|---|
| vertices | int32_t x3 per vertex | 12 x vertexCount | always |
| vertexColours | uint8_t x3 per vertex | 3 x vertexCount | always |
| vertexIndices | int16_t x4 per entry | 8 x indicesCount | always |
| normals | int16_t x3 per normal | 6 x normalsCount | always |
| normalIndices | int16_t x4 per entry | 8 x indicesCount | always |
| uvs | uint8_t x2 per uv | 2 x uvCount | always |
| uvIndices | int16_t x4 per entry | 8 x indicesCount | always |
| collisionBox.min | int16_t x3 | 6 | always |
| collisionBox.max | int16_t x3 | 6 | always |
| boundingSphere.centre | int16_t x3 | 6 | **version 3+** |
| boundingSphere.radius | int32_t | 4 | **version 3+** |
| bones | 21 bytes per bone | 21 x numBones | version 2+ and `hasSkeleton` |
| boneForVertex | uint8_t per vertex | 1 x vertexCount | version 2+ and `hasSkeleton` |

### Faces

Every index group is four `int16_t`, whether the face is a quad or a triangle. A triangle
sets the **second** slot to `-1` and carries its three indices in slots 1, 3 and 4. The
renderer selects a quad or a triangle primitive by testing that slot.

### Vertex colours

Three unsigned bytes. The exporter writes the OBJ vertex-colour extension values when
present and `(128, 128, 128)` when not. It never emits a sentinel, so there is no way to
express "this mesh has no colours"; an absent colour is mid grey.

### Bones

21 bytes each, not 24:

| Field | Type | Bytes |
|---|---|---|
| parent | int8_t | 1 |
| localPos | int32_t x3 | 12 |
| localRotation | int16_t x4 (w, x, y, z) | 8 |

`parent` is `-1` for a root bone. `MAX_BONES` is 15; see the caveats below.

### Bounding sphere

Version 3 added it. The reader adds `6 * 128` to whatever radius the file supplies, as
slack. If the field is absent or zero, the reader falls back to deriving a radius from the
AABB. In practice the fallback is unreachable because the exporter always writes an explicit
sphere.

## Caveats

- **Skeletons are capped at `MAX_BONES` (15) bones.** A file cannot declare more; this is a
  fixed capacity of the format, not a validation step.

## Changelog

- **Version 3** (2026-04-21): added the bounding sphere.
- **Version 2** (2025-10-07): added `hasSkeleton`, `numBones`, the bone table and
  `boneForVertex`.
- **Version 1**: initial format.
