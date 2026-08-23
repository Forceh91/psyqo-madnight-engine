---
title: ANIMBIN Format
sidebar_position: 4
---

# ANIMBIN

Skeletal animation clips. Produced by `tools/blender_animbin.py`, parsed by
`AnimationManager::LoadAnimation`, played by `SkeletonController::PlayAnimation`.

Every multi-byte field is little-endian.

## Header

| Field | Type | Bytes | Notes |
|---|---|---|---|
| magic | char[7] | 7 | `ANIMBIN`, no terminator. Mismatch aborts the load. |
| version | uint8_t | 1 | Exporter writes 1. |
| numAnimations | uint8_t | 1 | |

## Animation record

Repeated `numAnimations` times.

| Field | Type | Bytes | Notes |
|---|---|---|---|
| name | char[32] | 32 | Fixed width, null-padded. |
| flags | uint32_t | 4 | Only bit 0 is read anywhere: set means looping. |
| length | uint16_t | 2 | Clip length in frames. See the time base section. |
| numTracks | uint16_t | 2 | |
| numMarkers | uint16_t | 2 | |

Then `numTracks` track records, then `numMarkers` marker records. Markers come after all
tracks, not interleaved.

### Track record

| Field | Type | Bytes | Notes |
|---|---|---|---|
| type | uint8_t | 1 | Parsed, stored, never read. Playback keys off each `Key.keyType` instead. |
| jointId | uint8_t | 1 | Index into the skeleton's bone array. |
| numKeys | uint16_t | 2 | |

### Key record

| Field | Type | Bytes | Notes |
|---|---|---|---|
| frame | uint16_t | 2 | Frame this key applies at. |
| keyType | uint8_t | 1 | 0 = ROTATION, 1 = TRANSLATION. Selects the payload below. |
| value (ROTATION) | int16_t x4 | 8 | Quaternion w, x, y, z in FP12 (x4096). |
| value (TRANSLATION) | int32_t x3 | 12 | Vector x, y, z, copied raw into a `psyqo::Vec3`. |

### Marker record

| Field | Type | Bytes |
|---|---|---|
| name | char[32] | 32 |
| frame | uint16_t | 2 |

## Time base

Frame numbers are **GPU vsync ticks**, not a fixed authoring rate and not milliseconds.
`Renderer::Process` computes `deltaTime` as the difference between successive
`GPU::getFrameCount()` values and `SkeletonController::PlayAnimation` adds that straight to
the current frame with no conversion.

The practical consequence: a clip advances one frame per refresh, so **the same file plays
about 20% faster on NTSC than on PAL**. Your Blender scene frame rate has no effect on
playback speed; it only decides how many keys get baked. If you need consistent timing
across regions you have to scale `deltaTime` by the refresh rate yourself.

## Interpolation

Rotation keys are interpolated with `Slerp`, which despite the name is a normalised linear
interpolation. Its own comment notes it is only correct for small rotations, which is fine
for adjacent keyframes and not fine for large steps. There is no blending between clips:
`SetAnimation` is an instant cut.

Non-looping clips clamp and hold on the last frame rather than stopping.

## Capacity limits

These are the fixed array sizes in `src/animation/animation.hh`.

| Constant | Value | Bounds |
|---|---|---|
| `MAX_ANIMATIONS` | 5 | animations per file |
| `MAX_TRACKS` | 50 | tracks per animation |
| `MAX_KEYS` | 30 | keys per track |
| `MAX_MARKERS` | 5 | markers per animation |

The exporter only clamps against `MAX_KEYS` in `ALL` export mode. `ACTIVE` and `SELECTED`
take whatever frame range you give them, and nothing clamps the track count against the
number of deform bones in your rig. Check your counts before exporting.

## Caveats

- **Translation keys are parsed and never applied.** The format defines them, the reader has
  a full branch for them, and `PlayAnimation` handles only `ROTATION` before its
  `// TODO: translation`. The exporter never emits one either: it bakes
  `channel_types={'ROTATION'}` and writes `keyType = 0` for every key. Animation is
  rotation-only in practice.
- **Markers are parsed and never emitted.** The exporter hardcodes `numMarkers` to 0 and has
  no marker-writing code.
- **`Track.type` and `version` are both dead.** Written, parsed, never consulted.

## Changelog

- **Version 1** (2025-10-11): initial format.
