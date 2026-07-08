# SCENEBIN File Format Specification

## Changelog

### Version 1
- Initial scene manifest format
- Variable-length, type-tagged file entries
- Texture entries carry extra VRAM/CLUT placement data
- `SCENE` (nested scene reference) intentionally **not yet supported** — the
  type field is a `uint8_t` and `LoadFileType::SCENE` is `9999`, which does
  not fit. Deferred until the type field is widened or SCENE is given a
  wire-safe value.

---

## Header

| Offset | Size    | Field      | Type      | Description                          |
|--------|---------|------------|-----------|---------------------------------------|
| 0x00   | 8 bytes | magic      | char[8]   | Must be `"SCENEBIN"` (not null-terminated) |
| 0x08   | 1 byte  | fileCount  | uint8_t   | Number of entries that follow         |

No version byte yet — unlike COLBIN. If the format changes in a
non-backward-compatible way, add one (see COLBIN's header as the pattern to
follow).

---

## Entries

Immediately follows the header. `fileCount` entries, written back-to-back,
no padding between them.

| Field      | Type      | Description                                             |
|------------|-----------|-----------------------------------------------------------|
| type       | uint8_t   | `LoadFileType` — see Types table below                    |
| nameLen    | uint8_t   | Length of `name` in bytes (not including a null terminator; none is written) |
| name       | char[nameLen] | Archive-relative file path, **not null-terminated**    |
| extra      | *(conditional)* | Only present when `type == TEXTURE` — see below     |

### Texture placement (TEXTURE entries only)

Immediately follows `name` when `type == TEXTURE`. 8 bytes, little-endian.

| Field  | Type     | Description                          |
|--------|----------|---------------------------------------|
| vramX  | uint16_t | Texture page VRAM X placement         |
| vramY  | uint16_t | Texture page VRAM Y placement         |
| clutX  | uint16_t | CLUT VRAM X placement                 |
| clutY  | uint16_t | CLUT VRAM Y placement                 |

No other type currently carries extra payload. All four fields share a
`union` on the runtime side (see `LoadQueue` below) so future type-specific
payloads of up to 8 bytes can reuse the same slot without growing every
non-texture entry.

---

## Types

### LoadFileType

```cpp
enum LoadFileType { OBJECT, TEXTURE, MOD_FILE, ANIMATION, COLBIN, VAG, SCENE = 9999 };
```

| Value | Name      | Source extension | Extra payload |
|-------|-----------|-------------------|----------------|
| 0     | OBJECT    | —                 | none           |
| 1     | TEXTURE   | `.TIM`            | vramX, vramY, clutX, clutY |
| 2     | MOD_FILE  | `.MB`             | none           |
| 3     | ANIMATION | —                 | none           |
| 4     | COLBIN    | `.COLBIN` / `.CB` | none           |
| 5     | VAG       | `.VAG`            | none           |
| 9999  | SCENE     | —                 | **not supported by this format yet** |

Source-file type names (human-editable format, see below) are the enum
names lowercased: `object`, `texture`, `mod_file`, `animation`, `colbin`,
`vag`. `scene` is intentionally not accepted by the converter until nested
scene loading is designed.

### LoadQueue (runtime)

```cpp
struct LoadQueue {
    eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN + 1> fileName;
    LoadFileType type;
    union {
        struct {
            uint16_t vramX, vramY, clutX, clutY;
        } texture;
        uint8_t raw[8];
    };
};
```

Only meaningful when `type == TEXTURE`; other entries leave the union
default-initialized to zero and ignore it.

---

## Human-Editable Source Format

Plain text, one entry per line, whitespace-separated fields. Blank lines
are ignored. Lines starting with `#` are comments.

```
# scene manifest — SBSKT court scene
sfx SFX/FCNTNA.VAG
texture TEXTURES/LOGO.TIM 320 0 0 240
texture UI/CS_UI.TIM 320 129 0 241
object MODELS/SBSKT.MB
object MODELS/SCART.MB
```

- Type name is case-insensitive on read, always written lowercase by the
  converter.
- `texture` lines require exactly 4 trailing integer fields (vramX vramY
  clutX clutY). Any other count is a hard error at convert time.
- All other types take no extra fields.
- Paths are archive-relative, matched case-sensitively against the actual
  archive contents by the converter (fails the build if the referenced
  file doesn't exist on disk).

---

## Notes

1. **Little-endian:** all multi-byte fields (`vramX`, `vramY`, `clutX`,
   `clutY`) are packed little-endian, matching the PS1's MIPS target.
2. **No null terminators on disk:** `name` is stored with an explicit
   `nameLen`, not a null-terminated C string. Don't rely on `strlen` when
   parsing — read exactly `nameLen` bytes.
3. **`MAX_ARCHIVE_FILE_NAME_LEN`:** both the converter and the loader must
   agree on this constant. Keep it defined in one place and have the other
   side reference it, rather than hand-duplicating the number.
4. **VRAM/CLUT ranges:** VRAM is 1024×512 pixels; texture page placement
   and CLUT placement should both fall within that range. The converter
   validates this at build time so a bad coordinate is caught before it
   ships, rather than showing up as a corrupted texture on hardware.
5. **SCENE is deferred.** Do not add `scene` entries to source files until
   the type-field width question is resolved — the converter will reject
   the type name outright rather than silently truncating `9999`.
