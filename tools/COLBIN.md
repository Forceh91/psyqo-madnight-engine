# COLBIN File Format Specification

## Changelog

### Version 2
- Added spatial grid for broad phase collision culling
- Grid header added after main counts
- Grid data written before floor tris and wall OBBs

### Version 1
- Initial collision format
- Floor triangles for raycast-based floor detection
- Wall OBBs for SAT-based collision response

---

## Header

| Offset | Size    | Field          | Type                       | Description                        |
|--------|---------|----------------|----------------------------|------------------------------------|
| 0x00   | 6 bytes | magic          | fixed_string<char, 6>      | Must be `"COLBIN"`                 |
| 0x06   | 1 byte  | version        | uint8_t                    | File version (currently 2)         |
| 0x07   | 4 bytes | floorTriCount  | uint32_t                   | Number of floor triangles          |
| 0x0B   | 4 bytes | wallOBBCount   | uint32_t                   | Number of wall OBBs                |

---

## Grid Header

Immediately follows the main header.

| Offset | Size    | Field        | Type     | Description                                      |
|--------|---------|--------------|----------|--------------------------------------------------|
| +0x00  | 4 bytes | originX      | int32_t  | Grid origin X — scaled by 128                   |
| +0x04  | 4 bytes | originZ      | int32_t  | Grid origin Z — scaled by 128                   |
| +0x08  | 4 bytes | cellSize     | uint32_t | Cell size in engine units — scaled by 128        |
| +0x0C  | 2 bytes | gridWidth    | uint16_t | Number of cells in X                             |
| +0x0E  | 2 bytes | gridHeight   | uint16_t | Number of cells in Z                             |

---

## Variable-Length Data Sections

Written in this order after the grid header:

| Section       | Type           | Description                                          |
|---------------|----------------|------------------------------------------------------|
| gridCells     | `GridCell[][]` | gridWidth × gridHeight cells, X-major order          |
| floorTris     | `FloorTri[]`   | floorTriCount floor triangles for raycast            |
| wallOBBs      | `OBB[]`        | wallOBBCount OBBs for SAT collision                  |

---

## Types

### GridCell

Variable length. Each cell contains a count followed by wall OBB indices.

```cpp
struct GridCell {
    uint16_t count;           // number of wall indices in this cell
    uint16_t indices[count];  // indices into wallOBBs array
};
```

A wall OBB may appear in multiple cells if its AABB spans multiple cells.

### FloorTri

Flat or sloped surfaces the player walks on. Used for downward raycast floor detection.
Face normals always point upward (Y component is always positive).

```cpp
struct FloorTri {
    psyqo::Vec3 v0;   // vertex 0 (x, y, z) — scaled by 128
    psyqo::Vec3 v1;   // vertex 1 (x, y, z) — scaled by 128
    psyqo::Vec3 v2;   // vertex 2 (x, y, z) — scaled by 128
    int16_t n[3];     // face normal (x, y, z) — FP12 (scaled by 4096)
                      // no explicit padding — psyqo::Vec3 is 3x int32_t = 12 bytes each
};
```

### OBB

Vertical or near-vertical surfaces used for SAT-based push-out collision.
Stored as an Oriented Bounding Box with precomputed axes.

```cpp
struct OBB {
    psyqo::Vec3 center;      // OBB center (x, y, z) — scaled by 128
    psyqo::Vec3 axes[3];     // 3 orthogonal axes — FP12 (scaled by 4096), int32 each
                             //   axes[0] = along width
                             //   axes[1] = along height
                             //   axes[2] = face normal (thinnest axis)
    psyqo::Vec3 halfExtents; // half-size along each axis — scaled by 128
                             // minimum thickness of 32 units on face normal axis
    uint32_t flags = 0;      // reserved (0)
};
```

### ColBin (runtime)

```cpp
struct ColBin {
    struct Header {
        eastl::fixed_string<char, 6> magic; // "COLBIN"
        uint8_t version;                    // 2
        uint32_t floorTriCount;
        uint32_t wallOBBCount;
    };

    Header header;
    FloorTri *floors;
    OBB *walls;
};
```

---

## Runtime Grid Lookup

To get candidate wall OBBs for a given player position:

```c
int cell_x = (player_x - grid.originX) / grid.cellSize;
int cell_z = (player_z - grid.originZ) / grid.cellSize;
cell_x = clamp(cell_x, 0, grid.gridWidth  - 1);
cell_z = clamp(cell_z, 0, grid.gridHeight - 1);
GridCell *cell = &grid.cells[cell_x][cell_z];
// test only cell->indices[0..count-1] against player OBB
```

---

## Notes

1. **Coordinate system:** Y-up, right-handed. Forward is -Z.
2. **Scale:** All positions are multiplied by 128 (1 Blender unit = 128 engine units).
3. **Fixed point:** Normals and OBB axes use FP12 (int32, scaled by 4096). Positions use int32 scaled by 128.
4. **Floor vs wall classification:** Determined at export time by face normal Y component. `abs(normal.y) >= 0.7` → floor, otherwise wall.
5. **Floor normals:** Always stored pointing upward regardless of face winding.
6. **OBB face normal axis:** Always the thinnest axis, guaranteed to be `axes[2]` / `halfExtents[2]`. Minimum thickness of 32 units.
7. **Grid cell size:** Tunable at export time. Default 256 engine units (~2 Blender units). Smaller = more cells, fewer walls per cell. Should be larger than maximum per-substep player movement to ensure no wall is skipped.
8. **No padding between sections:** Data is written sequentially with only per-struct padding for alignment.
9. **Player collision:** Player is treated as an AABB. Wall collision uses AABB vs OBB SAT. Floor height uses downward raycast against FloorTris.