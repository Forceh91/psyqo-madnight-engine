---
title: Mesh & Animation
sidebar_position: 4
---

# Mesh & Animation

`src/mesh/`, `src/animation/`, and `src/quaternion.hh` — loading `.MESHBIN` files (see the [MESHBIN format spec](../guides/meshbin)), skeletal posing, and `.ANIMBIN` playback.

## MeshManager

`src/mesh/mesh_manager.hh`

Loads and caches up to `MAX_LOADED_MESHES` (250) meshes by name, each with up to `MAX_FACES_PER_MESH` (1000) faces.

```cpp
class MeshManager {
public:
  static psyqo::Coroutine<> LoadMesh(const char *meshName, MeshBin **meshOut);
  static void GetMeshFromName(const char *meshName, MeshBin **meshOut);
  static void UnloadMesh(const char *mesh_name);

  // dump all meshes in memory and start fresh. Used when switching to a loading screen.
  // dangerous — doesn't check if anything is still in use.
  static void Dump(void);
};
```

### MeshBin

The in-memory representation of a loaded `.MESHBIN` file (see the [file format spec](../guides/meshbin) for the on-disk layout):

```cpp
struct MeshBin {
  uint8_t type; // 1 = quads, 2 = tris (unused)

  uint32_t vertexCount, indicesCount, facesCount, normalsCount, uvCount;
  uint8_t hasSkeleton;
  uint8_t numBones;

  psyqo::Vec3 *vertices;
  MeshBinVertexColours *vertexColours;
  MeshBinIndex *vertexIndices;

  psyqo::Vec3 *normals;
  MeshBinIndex *normalIndices;

  psyqo::PrimPieces::UVCoords *uvs;
  MeshBinIndex *uvIndices;

  Skeleton *skeleton;
  uint8_t *boneForVertex;          // vertex index -> bone index
  psyqo::Vec3 *verticesOnBonePos;

  AABBCollision collisionBox;
  BoundingSphere bsphere;
};
```

`GameObject::mesh()` returns a pointer into a manager-owned `MeshBin` — meshes are shared across every `GameObject` that references the same name, not duplicated per-instance.

### Usage

```cpp
MeshBin *crateMesh;
co_await MeshManager::LoadMesh("crate", &crateMesh); // must be co_awaited inside a coroutine

crateGameObject->SetMesh("crate"); // looks it up by name once loaded
```

### Internals

- `LoadMesh` checks `IsMeshLoaded` first, so calling it again with an already-loaded name is cheap — it just hands back the cached pointer instead of re-reading the file.

## Skeleton & SkeletonController

`src/mesh/skeleton/skeleton.hh`

A fixed-size (`MAX_BONES` = 15) bone hierarchy embedded in a skinned `MeshBin`, posed via quaternion rotations.

```cpp
struct SkeletonBoneMatrix {
  psyqo::Matrix33 rotationMatrix;
  psyqo::Vec3 translation;
};

struct SkeletonBone {
  int8_t id;
  int8_t parent;                    // -1 = root
  psyqo::Vec3 localPos;             // relative to parent
  Quaternion localRotation;         // relative to parent
  Quaternion initialLocalRotation;
  SkeletonBoneMatrix localMatrix;   // computed by GameObject::GenerateRotationMatrix
  SkeletonBoneMatrix worldMatrix;   // computed from parent, likewise
  SkeletonBoneMatrix bindPose;      // pose when the skeleton was loaded in
  SkeletonBoneMatrix bindPoseInverse;
  bool isDirty = true;              // set by the animation system when it needs recomputing
  bool hasDoneBindPose = false;
};

struct Skeleton {
  uint8_t numBones;
  SkeletonBone bones[MAX_BONES];
  Animation *animation;
  uint16_t animationCurrentFrame = 0;
};

class SkeletonController {
public:
  static void UpdateSkeletonBoneMatrices(Skeleton *skeleton);
  static void MarkBonesClean(Skeleton *skeleton);
  static void SetAnimation(Skeleton *skeleton, Animation *animation);
  static void PlayAnimation(Skeleton *skeleton, uint32_t deltaTime);
};
```

- `PlayAnimation` advances `animationCurrentFrame` and marks affected bones dirty; `UpdateSkeletonBoneMatrices` then recomputes each dirty bone's local/world matrices (parent-relative, walking up the hierarchy via `parent`).
- `bindPose`/`bindPoseInverse` are captured once when the skeleton is first loaded and used to skin vertices back into their animated position each frame.

### Usage

```cpp
Animation *walkAnim = AnimationManager::GetAnimationFromName("walk");
SkeletonController::SetAnimation(mesh->skeleton, walkAnim);

// per-frame:
SkeletonController::PlayAnimation(mesh->skeleton, deltaTime);
SkeletonController::UpdateSkeletonBoneMatrices(mesh->skeleton);
SkeletonController::MarkBonesClean(mesh->skeleton); // once you're done reading this frame's matrices
```

### Internals

- `SetAnimation` just overwrites the current animation and resets to frame 0 — there's no blending between the old and new animation.
- Only `ROTATION` keys are actually applied — `TRANSLATION` tracks are read but not yet wired up (marked `TODO` in-source).
- Dirty state propagates down the hierarchy: a bone is recomputed if it changed *or* its parent did, so posing the hips also quietly re-dirties everything below it.
- `PlayAnimation`'s `deltaTime` is GPU vsync ticks, not a fixed rate or milliseconds, with no refresh-rate normalization: the same clip plays about 20% faster on NTSC than on PAL. See the [ANIMBIN format spec](../guides/animbin#time-base) for the full breakdown.

## AnimationManager

`src/animation/animation_manager.hh`

```cpp
class AnimationManager final {
public:
  static psyqo::Coroutine<> LoadAnimation(const char *animationsFile);
  static Animation *GetAnimationFromName(const eastl::fixed_string<char, MAX_ANIMATION_NAME_LENGTH> &animationName);
};
```

Loads a whole `.ANIMBIN` file (see the [file format spec](../guides/animbin)) at once — a single `.ANIMBIN` can contain up to `MAX_ANIMATIONS` (5) named animations, retrieved individually afterwards by name.

Like [`ColbinManager`](./physics-and-collision#colbinmanager), this holds one loaded `.ANIMBIN` at a time (a single static `AnimationBin`, not a pool) — loading a new file replaces whatever was loaded before.

### Animation data types

`src/animation/animation.hh`

```cpp
enum KeyType : uint8_t { ROTATION, TRANSLATION };

struct Key {
  union { Quaternion rotation; psyqo::Vec3 translation; };
  uint16_t frame;
  KeyType keyType;
};

struct Track {           // "for this bone, do this"
  uint8_t type;
  uint8_t jointId;
  uint16_t numKeys;
  Key keys[MAX_KEYS];    // MAX_KEYS = 30
};

struct Marker {           // e.g. "play a footstep sound at this frame"
  eastl::fixed_string<char, MAX_MARKER_NAME_LENGTH> name;
  uint16_t frame;
};

struct Animation {
  eastl::fixed_string<char, MAX_ANIMATION_NAME_LENGTH> name;
  uint32_t flags;        // bitfield — looped = 1
  uint16_t length;       // frame count
  uint16_t numTracks;
  Track tracks[MAX_TRACKS];   // MAX_TRACKS = 50
  uint16_t numMarkers;
  Marker markers[MAX_MARKERS]; // MAX_MARKERS = 5
};

struct AnimationBin {
  uint8_t numAnimations;
  Animation animations[MAX_ANIMATIONS]; // MAX_ANIMATIONS = 5
};
```

## Quaternion

`src/quaternion.hh`

GTE-backed quaternion type (`psyqo::GTE::Short` components) used throughout skeletal animation.

```cpp
struct Quaternion {
  psyqo::GTE::Short w{1.0}, x{0.0}, y{0.0}, z{0.0};

  auto operator<=>(const Quaternion &) const = default;

  psyqo::Matrix33 ToRotationMatrix() const;
  void Normalize();
};

Quaternion operator*(const Quaternion &q1, const Quaternion &q2);
Quaternion operator-(const Quaternion &q);

psyqo::GTE::Short DotProduct(const Quaternion &a, const Quaternion &b);

// Small-rotation only for now — fine for animation interpolation.
Quaternion Slerp(const Quaternion &q1, const Quaternion &q2, psyqo::FixedPoint<> factor);

// Rotation quaternion that takes v1 to v2.
Quaternion FindRotationQuat(const psyqo::Vec3 &v1, const psyqo::Vec3 &v2, psyqo::Trig<> &trig);
Quaternion FromEulerAngles(psyqo::Angle pitch, psyqo::Angle yaw, const psyqo::Trig<> &trig);
```

:::caution
`Slerp` isn't slerp. The implementation is a component-wise linear blend of the two quaternions followed by a normalize, with no `acos`/`sin` anywhere in it: it's nlerp. The in-source comment calling it "small rotations only" is describing the accuracy limits of that nlerp approximation, not a spherical interpolation with a reduced range. It's fine for interpolating between adjacent animation keyframes and not fine for arbitrary large-angle rotation blending.
:::

`FindRotationQuat` returns the rotation quaternion that takes `v1` to `v2`. `FromEulerAngles` builds a quaternion from the given pitch and yaw angles.
