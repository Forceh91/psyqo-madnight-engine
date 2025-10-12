# ANIMBIN File Format Specification

## Changelog

### Version 1 (2025-10-11)
- Initial animation format
- Supports multiple animations, tracks per bone, and frame markers
- Fixed-point math for rotations (quaternions) and translations (Vec3)

---

## Header

| Offset  | Size     | Field         | Type       | Description / Notes                          |
|--------|---------|--------------|-----------|----------------------------------------------|
| 0x00   | 7 bytes | magic        | char[7]   | Must be `"ANIMBIN"`                           |
| 0x07   | 1 byte  | version      | uint8_t   | File version (currently 1)                   |
| 0x08   | 1 byte | numAnimations| uint8_t  | Number of animations in file                 |

---

## Animation Header (per animation)

| Offset (relative) | Size     | Field        | Type       | Description / Notes                            |
|-----------------|---------|-------------|-----------|-----------------------------------------------|
| 0x00            | 32 bytes| name        | char[32]  | Name of animation, null-terminated if shorter |
| 0x20            | 4 bytes | flags       | uint32_t  | Bitfield flags (e.g., looped = 1)             |
| 0x24            | 2 bytes | length      | uint16_t  | Number of frames                               |
| 0x26            | 2 bytes | numTracks   | uint16_t  | Number of tracks                               |
| 0x28            | 2 bytes | numMarkers  | uint16_t  | Number of frame markers                        |

---

## Track (per bone track)

| Offset (relative) | Size     | Field      | Type       | Description / Notes                          |
|-----------------|---------|-----------|-----------|----------------------------------------------|
| 0x00            | 1 byte  | type      | uint8_t   | Track type: 0 = rotation, 1 = translation   |
| 0x01            | 1 byte  | jointId   | uint8_t   | Index of the bone/joint this track affects  |
| 0x02            | 2 bytes | numKeys   | uint16_t  | Number of keyframes in this track           |

---

## Key (per track key)

| Offset (relative) | Size     | Field        | Type      | Description / Notes                           |
|-----------------|---------|-------------|----------|-----------------------------------------------|
| 0x00            | 2 bytes | frame       | uint16_t | Frame number of this key                      |
| 0x02            | 1 byte  | keyType     | uint8_t  | 0 = rotation, 1 = translation                 |
| 0x03            | 8 or 12 bytes | value   | Quaternion (8 bytes) or Vec3 (12 bytes) | Rotation or translation (fixed-point FP12) |

- **Quaternion:** `[w, x, y, z]` each `int16_t` FP12 → total 8 bytes  
- **Vec3:** `[x, y, z]` each `int32_t` FP12 → total 12 bytes

---

## Marker (per animation)

| Offset (relative) | Size     | Field  | Type           | Description / Notes                          |
|-----------------|---------|--------|---------------|-----------------------------------------------|
| 0x00            | 32 bytes| name   | char[32]      | Marker name                                   |
| 0x20            | 2 bytes | frame  | uint16_t      | Frame this marker occurs                       |

---

## Notes

1. **Fixed-point FP12:** All quaternions are stored as 16-bit integers (FP12). Translations (Vec3) are stored as 32-bit FP12.  
2. **Track ordering:** Tracks are stored sequentially after the animation header. Each track contains all its keys in sequence.  
3. **Markers:** Optional, can be zero-length. Stored after tracks in the animation.  
4. **No padding:** All offsets are tightly packed; structures are written sequentially.  
