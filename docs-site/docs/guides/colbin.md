---
title: COLBIN Format
sidebar_position: 3
---

# COLBIN

Collision geometry: floor triangles, wall boxes, and a uniform grid over the walls. Produced
by `tools/blender_colbin.py` from a `COL` collection, consumed by
`ColbinManager::LoadColbin`.

Every multi-byte field is little-endian.

## Fixed-point conventions

Two conventions, and which one applies depends on the field, not on the struct it lives in.

| Data | Convention |
|---|---|
| Positions: grid origin, cell size, floor vertices, OBB centre, OBB half extents | x128 integer (128 units per engine metre) |
| Directions: floor normals, OBB axes | FP12 (x4096) |

Both end up in fields whose C++ type is nominally 12-bit fixed point, so the type does not
tell you which convention a field uses. The table below does.

## Header

| Field | Type | Bytes | Notes |
|---|---|---|---|
| magic | char[6] | 6 | `COLBIN`, no terminator. Mismatch aborts the load. |
| version | uint8_t | 1 | Currently 2. |
| floorTriCount | uint32_t | 4 | |
| wallOBBCount | uint32_t | 4 | |

## Grid header

| Field | Type | Bytes | Convention |
|---|---|---|---|
| originX | int32_t | 4 | x128 |
| originZ | int32_t | 4 | x128 |
| cellSize | uint32_t | 4 | x128. Default is 256, i.e. two Blender units. |
| gridWidth | uint16_t | 2 | cell count along X |
| gridHeight | uint16_t | 2 | cell count along Z |

## Grid cells

`gridWidth * gridHeight` cells follow, in **X-major** order: X is the outer loop and Z the
inner, so the cell at `(gx, gz)` is at linear index `gx * gridHeight + gz`.

| Field | Type | Bytes |
|---|---|---|
| count | uint16_t | 2 |
| indices | uint16_t x count | 2 x count |

Each index refers to an entry in the wall OBB array. An empty cell is a `count` of zero
followed by no index bytes at all.

## Floor triangles

`floorTriCount` records, 44 bytes each.

| Field | Type | Bytes | Convention |
|---|---|---|---|
| v0 | int32_t x3 | 12 | x128 |
| v1 | int32_t x3 | 12 | x128 |
| v2 | int32_t x3 | 12 | x128 |
| normal | int16_t x3 | 6 | FP12 |
| padding | 2 zero bytes | 2 | written by the exporter, skipped by the reader |

## Wall OBBs

`wallOBBCount` records, 64 bytes each.

| Field | Type | Bytes | Convention |
|---|---|---|---|
| center | int32_t x3 | 12 | x128 |
| axes[0] | int32_t x3 | 12 | FP12 |
| axes[1] | int32_t x3 | 12 | FP12 |
| axes[2] | int32_t x3 | 12 | FP12 |
| halfExtents | int32_t x3 | 12 | x128 |
| flags | uint32_t | 4 | reserved, always 0 |

`axes` are the source object's local X, Y and Z in world space, **in that order**. The
exporter does not sort them. Whichever half extent is numerically smallest is clamped to a
minimum thickness of 32 units so that a flat wall still has volume, but that can be index 0,
1 or 2 depending on how the source mesh is oriented. Do not assume the thin axis is
`axes[2]`.

## Caveats

- **The engine has no grid lookup.** `gridWidth`, `gridHeight`, `cellSize`, `originX`,
  `originZ` and `gridCells` appear nowhere outside `ColbinManager` itself. The grid is
  parsed and stored and nothing queries it yet. Earlier revisions of this page carried a
  world-position-to-cell code sample; it described intended behaviour, not shipped
  behaviour, and has been removed. The mapping the exporter uses when it populates cells is
  `floor((x - originX) / cellSize)`, clamped to the grid bounds, if you want to implement
  the lookup against it.

## Changelog

- **Version 2**: added the uniform grid over wall OBBs.
- **Version 1**: floor triangles and wall OBBs only.
