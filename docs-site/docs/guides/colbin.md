---
title: COLBIN Format
sidebar_position: 3
---

# COLBIN

Collision geometry: floor triangles, wall boxes, and a uniform grid over the walls. Produced
by `tools/blender_colbin.py`, either from a hand built `COL` collection or inferred from the
level's own meshes (see [Exporting](#exporting)), consumed by `ColbinManager::LoadColbin`.

Every multi-byte field is little-endian.

## Fixed-point conventions

Two conventions, and which one applies depends on the field, not on the struct it lives in.

| Data | Convention |
|---|---|
| Positions: grid origin, cell size, floor vertices, OBB centre, OBB half extents | x128 integer (128 units per Blender unit, which is 2 m) |
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

What `axes` hold depends on where the box came from. For a face in the `COL` collection they
are the source object's local X, Y and Z in world space, **in that order**. For an inferred
wall they are the wall's long in-plane direction, its other in-plane direction and its
normal. For an inferred prop box they are whichever of the object's axes or the smallest
upright box fits tighter. The exporter does not sort them. Whichever half extent is
numerically smallest is clamped to a minimum of 32 units so that a flat wall still has
volume, but that can be index 0, 1 or 2, and on a face narrower than 64 units the clamped
normal is no longer the smallest. Do not assume the thin axis is `axes[2]`.

`center` is the middle of the box's extents along its axes.

The exporter inserts a wall into every cell its world space footprint (the AABB of the
rotated box on X and Z) overlaps.

## Caveats

- **The engine has no grid lookup.** `gridWidth`, `gridHeight`, `cellSize`, `originX`,
  `originZ` and `gridCells` appear nowhere outside `ColbinManager` itself. The grid is
  parsed and stored and nothing queries it yet. Earlier revisions of this page carried a
  world-position-to-cell code sample; it described intended behaviour, not shipped
  behaviour, and has been removed. The mapping the exporter uses when it populates cells is
  `floor((x - originX) / cellSize)`, clamped to the grid bounds, if you want to implement
  the lookup against it. The tech demo has a small one in `HallScene::CollideHero`
  (`tech-demo/src/hall_scene.cpp`): every cell under the collider's X/Z footprint, each wall
  tested once, pushed out along `CollisionTest::mtv`.
## Exporting

`File > Export > Collision Binary (.colbin)`. The `Source` option picks the geometry:

| Source | Uses |
|---|---|
| `COL` | Every mesh directly in a collection named `COL`. The default when that collection exists. |
| `Selected` | The selected mesh objects, collision inferred from their faces. |
| `Scene` | Every visible mesh object, collision inferred, plus the `COL` collection as hand built extras. The default when there is no `COL` collection. |

Every source reads the evaluated mesh, so modifiers are applied.

`Scene` skips objects in a collection named `NOCOL` and objects with a custom property
`colbin_ignore` set to a true value; `Selected` honours `colbin_ignore` too. Objects in `COL`
are not inferred again.

### Hand built (`COL`)

Each face whose normal, flipped to point along +Y, has a Y of at least `Floor Threshold`
(default 0.7) becomes floor triangles, fan triangulated. That includes ceilings. Every other
face becomes one wall box in the object's own axes. `COL` objects are exported even when
hidden.

### Inferred (`Selected`, `Scene`)

Each loose part of a mesh is looked at on its own:

- **Props.** A part that is closed and fills at least `Prop Fill Ratio` (default 0.6) of
  the box around it becomes that one box, and nothing else: no floor triangles on top.
  A part whose normals point inwards (a room) is never boxed.
- **Everything else** goes face by face. Faces facing up past `Floor Threshold` are floor
  triangles. Faces facing down past it (ceilings, the undersides of shelves) are dropped.
  Faces smaller than one square engine unit (slivers left by cuts) are dropped. The rest are
  wall faces.

With `Merge Walls` on (the default), wall faces from all inferred objects are grouped by
plane, faces in a plane that share a stretch of edge are joined into regions, and each region
becomes the smallest rectangle around it if the faces cover at least `Merge Tolerance`
(default 0.9) of that rectangle. A region that does not is split: a chunk grows from one face
while it still fits, then the next chunk starts. With it off, each wall face gets its own box
lined up with the face's longest edge.

Wall boxes are 64 units thick and centred on the face, so they work for walls seen from both
sides (shelf panels, house walls). A collider stops 32 units in front of the visible surface.

`Min Wall Height` (Blender units, default 0, off) extends every inferred box shorter than it
upward along its most upright axis: its bottom stays where it was and its top rises until the
box is that tall, so a tall player is stopped by a low obstacle instead of pushed up by it.
Hand built `COL` boxes are left alone.

A custom property `colbin_mode` on an object overrides the choice: `box` makes the whole
object one box, `faces` never boxes it.

From a script, after the add-on is registered:

```python
bpy.ops.export.colbin(filepath="LEVEL.COLBIN", source='SCENE', merge_walls=True)
```

The exporter prints one line with the floor triangle count, wall box count, grid size and
file size, and for inferred sources a second line with where the faces went.

## Changelog

- **Version 2**: added the uniform grid over wall OBBs.
- **Version 1**: floor triangles and wall OBBs only.
