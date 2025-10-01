# MESHBIN File Format Specification

## Header

| Offset  | Size     | Field    | Type     | Description / Notes         |
|--------|---------|---------|---------|-----------------------------|
| 0x00   | 7 bytes | magic   | char[7] | Must be "MESHBIN"           |
| 0x07   | 1 byte  | version | uint8_t | File version (currently 1) |
| 0x08   | 1 byte    | type        | uint8_t   | Mesh type (1 = quads, 2 = tris) *(unused)* |


## Subheader

| Offset  | Size       | Field        | Type      | Description / Notes                          |
|--------|-----------|-------------|-----------|----------------------------------------------|
| 0x09   | 4 bytes   | vertexCount | uint32_t  | Number of vertices                           |
| 0x0D   | 4 bytes   | indicesCount| uint32_t  | Number of vertex indices                        |
| 0x11   | 4 bytes   | facesCount  | uint32_t  | Number of faces                              |
| 0x15   | 4 bytes   | normalsCount| uint32_t  | Number of normals                            |
| 0x19   | 4 bytes   | uvCount     | uint32_t  | Number of UV coordinates                     |

## Variable-Length Data Sections

| Field / Section       | Type            | Size / Count                       | Description / Notes                         |
|----------------------|----------------|-----------------------------------|---------------------------------------------|
| vertices             | int32_t[3]      | 12 * vertexCount                  | Vertex positions (x, y, z)                  |
| vertexColours        | int8_t[3]      | 3 * vertexCount                   | Vertex colors (r, g, b)                     |
| vertexIndices              | int16_t[4]      | 8 * indicesCount                  | Vertex indices per face                      |
| normals              | int32_t[3]      | 12 * normalsCount                 | Normal vectors                               |
| normalsIndices       | int16_t[4]      | 8 * indicesCount                  | Normal indices per face                      |
| uvCoords            | uint8_t[2]      | 2 * uvCount                        | Texture coordinates (u, v)                  |
| uvIndices           | int16_t[4]      | 8 * indicesCount                  | UV indices per face                          |
