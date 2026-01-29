# MESHBIN File Format Specification

## Changelog

### Version 2 (2025-10-07)
- Implement skeleton data

## Header

| Offset  | Size     | Field    | Type     | Description / Notes         |
|--------|---------|---------|---------|-----------------------------|
| 0x00   | 7 bytes | magic   | char[7] | Must be "MESHBIN"           |
| 0x07   | 1 byte  | version | uint8_t | File version (currently 2) |
| 0x08   | 1 byte    | type        | uint8_t   | Mesh type (1 = quads, 2 = tris) *(unused)* |


## Subheader

| Offset  | Size       | Field        | Type      | Description / Notes                          |
|--------|-----------|-------------|-----------|----------------------------------------------|
| 0x09   | 4 bytes   | vertexCount | uint32_t  | Number of vertices                           |
| 0x0D   | 4 bytes   | indicesCount| uint32_t  | Number of vertex indices                        |
| 0x11   | 4 bytes   | facesCount  | uint32_t  | Number of faces                              |
| 0x15   | 4 bytes   | normalsCount| uint32_t  | Number of normals                            |
| 0x19   | 4 bytes   | uvCount     | uint32_t  | Number of UV coordinates                     |
| 0x1D   | 1 byte    | hasSkeleton        | uint8_t   | Does it have a skeleton? (1 = yes, 0 = no) |
| 0x1E   | 1 byte    | boneCount        | uint8_t   | Mesh type (1 = quads, 2 = tris) *(unused)* |

## Variable-Length Data Sections

| Field / Section       | Type            | Size / Count                       | Description / Notes                         |
|----------------------|----------------|-----------------------------------|---------------------------------------------|
| vertices             | int32_t[3]      | 12 * vertexCount                  | Vertex positions (x, y, z)                  |
| vertexColours        | int8_t[3]      | 3 * vertexCount                   | Vertex colors (r, g, b)                     |
| vertexIndices              | int16_t[4]      | 8 * indicesCount                  | Vertex indices per face                      |
| normals              | int16_t[3]      | 6 * normalsCount                 | Normal vectors                               |
| normalsIndices       | int16_t[4]      | 8 * indicesCount                  | Normal indices per face                      |
| uvCoords            | uint8_t[2]      | 2 * uvCount                        | Texture coordinates (u, v)                  |
| uvIndices           | int16_t[4]      | 8 * indicesCount                  | UV indices per face                          |
| AABBMin           | int16_t[3]      | 6 bytes                  | Min coords (x,y,z) for AABB box |
| AABBMax           | int16_t[3]      | 6 bytes                  | Max coords (x,y,z) for AABB box |
| bones           | `SkeletonBone`      | 24 * boneCount                | Bone data including parent, local pos, and local rotation (quaternion) |
| vertexToBoneID           | uint8_t[vertexCount]      | 1 * vertexCount                | Vertex index to bone ID, array indexes match vertex index

## Types
### SkeletonBone
```
struct SkeletonBone {
  int8_t parent;                           // -1 = root
  psyqo::Vec3 localPos = {0, 0, 0};        // relative to parent
  Quaternion localRotation = {0, 0, 0, 0}; // relative to parent
};
```