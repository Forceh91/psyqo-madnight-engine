# COLBIN File Format Specification

## Changelog

### Version 1
- Initial collision format
- Floor triangles for raycast-based floor detection
- Wall OBBs for SAT-based collision response

---

## Header

| Offset | Size    | Field          | Type     | Description                        |
|--------|---------|----------------|----------|------------------------------------|
| 0x00   | 6 bytes | magic          | char[6]  | Must be `"COLBIN"`                 |
| 0x06   | 1 byte  | version        | uint8_t  | File version (currently 1)         |
| 0x07   | 4 bytes | floorTriCount  | uint32_t | Number of floor triangles          |
| 0x0B   | 4 bytes | wallOBBCount   | uint32_t | Number of wall OBBs                |

---

## Variable-Length Data Sections

| Section       | Type         | Size / Count              | Description                          |
|---------------|--------------|---------------------------|--------------------------------------|
| floorTris     | `FloorTri`   | 32 * floorTriCount        | Floor triangles for raycast          |
| wallOBBs      | `WallOBB`    | 48 * wallOBBCount         | Wall OBBs for SAT collision          |

---

## Types

### FloorTri

Flat or sloped surfaces the player walks on. Used for downward raycast floor detection.
Face normals always point upward (Y component is always positive).

```
struct FloorTri {
    int32_t v0[3];    // vertex 0 (x, y, z) — scaled by 128
    int32_t v1[3];    // vertex 1 (x, y, z) — scaled by 128
    int32_t v2[3];    // vertex 2 (x, y, z) — scaled by 128
    int16_t n[3];     // face normal (x, y, z) — FP12 (scaled by 4096)
    uint8_t _pad[2];  // padding to align to 4 bytes
};
// sizeof(FloorTri) = 42 bytes
```

### WallOBB

Vertical or near-vertical surfaces used for SAT-based push-out collision.
Stored as an Oriented Bounding Box with precomputed axes.

```
struct WallOBB {
    int32_t center[3];      // OBB center (x, y, z) — scaled by 128
    int16_t axes[3][3];     // 3 orthogonal axes — FP12 (scaled by 4096)
                            //   axes[0] = along width
                            //   axes[1] = face normal
                            //   axes[2] = along height
    uint8_t _pad[3][2];     // padding per axis to align to 4 bytes
    int32_t halfExtents[3]; // half-size along each axis — scaled by 128
                            // minimum thickness of 4 units on degenerate axis
    uint32_t flags;         // reserved (0)
};
// sizeof(WallOBB) = 48 bytes
```

---

## Notes

1. **Coordinate system:** Y-up, right-handed. Forward is -Z.
2. **Scale:** All positions are multiplied by 128 (1 Blender unit = 128 engine units).
3. **Fixed point:** Normals and OBB axes use FP12 (int16, scaled by 4096). Positions use int32 scaled by 128.
4. **Floor vs wall classification:** Determined at export time by face normal Y component. `abs(normal.y) >= 0.7` → floor, otherwise wall.
5. **Floor normals:** Always stored pointing upward regardless of face winding. Negated at export if necessary.
6. **OBB thickness:** Degenerate axes (flat planes) are given a minimum half-extent of 4 units to avoid zero-thickness SAT failure.
7. **No padding between sections:** Data is written sequentially with only per-struct padding for alignment.
8. **Player collision:** Player is treated as an AABB. Wall collision uses AABB vs OBB SAT. Floor height uses downward raycast against FloorTris.
