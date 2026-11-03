---
title: Mesh & Animation
sidebar_position: 4
---

# Mesh & Animation

`src/mesh/` loads `.MESHBIN` models and holds their skeletons. `src/animation/` loads `.ANIMBIN` clips to play on those skeletons. `src/quaternion.hh` is the rotation type they share.

All three managers here are members of `g_madnightEngine`: `m_meshManager`, `m_animationManager` and `m_skeletonController`.

## MeshManager

`src/mesh/mesh_manager.hh`

Loads `.MESHBIN` files (see the [MESHBIN guide](../guides/meshbin)) and keeps them by name. You normally load meshes with an `OBJECT` entry in a load queue, then attach one to a game object by the same name:

```cpp
eastl::vector<LoadQueue> files = {
    {.name = "MODELS/STREET.MB", .type = LoadFileType::OBJECT},
};
co_await g_madnightEngine.HardLoadingScreen(eastl::move(files), &gameplayScene);

street->SetMesh("MODELS/STREET.MB");
```

This is the pattern `src/game.cpp` uses. Every game object that uses the same name shares one copy of the mesh.

To load one yourself from a coroutine:

```cpp
MeshBin *crate = nullptr;
co_await g_madnightEngine.m_meshManager.LoadMesh("MODELS/CRATE.MB", &crate);
if (crate == nullptr) {
  // missing, invalid, too big, or all 250 slots are taken
}
```

Loading a name that is already loaded hands back the existing mesh without reading the file again. Always pass a real pointer as the second argument.

`GetMeshFromName` looks up a loaded mesh and sets the pointer to `nullptr` if there is none:

```cpp
MeshBin *street = nullptr;
g_madnightEngine.m_meshManager.GetMeshFromName("MODELS/STREET.MB", &street);
```

The fields you are likely to read are `collisionBox` (the mesh's min/max box), `bsphere` (the bounding sphere the renderer culls with), `hasSkeleton` and `skeleton`.

Limits:

- Up to 250 meshes loaded at once (`MAX_LOADED_MESHES`).
- A mesh must have fewer than 1000 faces (`MAX_FACES_PER_MESH`). A mesh with 1000 or more is rejected.
- A skinned mesh can have up to 15 bones (`MAX_BONES`). More than that and the mesh is rejected.

### Unloading

`UnloadMesh` frees one mesh by name. A hard load (`HardLoadingScreen`) frees all of them.

```cpp
g_madnightEngine.m_meshManager.UnloadMesh("MODELS/CRATE.MB");
```

`UnloadMesh` does not check whether a game object is still using the mesh. Destroy or re-mesh those objects first.

## AnimationManager

`src/animation/animation_manager.hh`

Loads an `.ANIMBIN` file (see the [ANIMBIN guide](../guides/animbin)), which holds up to 5 named clips. Load it with an `ANIMATION` queue entry, then fetch a clip by name:

```cpp
eastl::vector<LoadQueue> files = {
    {.name = "MODELS/HERO.MB", .type = LoadFileType::OBJECT},
    {.name = "ANIMS/HERO.ANIMBIN", .type = LoadFileType::ANIMATION},
};
co_await g_madnightEngine.HardLoadingScreen(eastl::move(files), &gameplayScene);

Animation *walk = g_madnightEngine.m_animationManager.GetAnimationFromName("walk");
```

`GetAnimationFromName` returns `nullptr` if no clip has that name. The name is the clip name inside the file, not the file name.

Limits:

- Only one `.ANIMBIN` is held at a time. Loading another replaces every clip from the first, and an `Animation *` you kept from before now points at the new file's data. Put every clip you need at the same time in one file.
- A file that is found but rejected leaves no clips loaded. A file that is missing leaves the old ones in place.
- A hard load does not clear animations.
- Per clip: up to 50 tracks, 30 keys per track and 5 markers. A file over any of these is rejected. The [ANIMBIN guide](../guides/animbin#capacity-limits) has the details.

## Skeleton & SkeletonController

`src/mesh/skeleton/skeleton.hh`

A skinned mesh carries a `Skeleton`, reached through the mesh. To play a clip on it, give the clip to `SetAnimation`:

```cpp
auto *hero = g_madnightEngine.m_gameObjectManager.GetGameObjectByName("hero");
Animation *walk = g_madnightEngine.m_animationManager.GetAnimationFromName("walk");
g_madnightEngine.m_skeletonController.SetAnimation(hero->mesh()->skeleton, walk);
```

That is all you need to do. The renderer advances the clip, poses the bones and moves the vertices every frame for each skinned object it draws, so you do not call `PlayAnimation`, `UpdateSkeletonBoneMatrices` or `MarkBonesClean` yourself.

`SetAnimation` starts the new clip from frame 0 straight away. There is no blending between clips. `SetAnimation` with a `nullptr` skeleton or clip does nothing, so it is safe to call before checking the lookup.

To see where a clip is, read `skeleton->animationCurrentFrame`. `skeleton->animation` is the clip that is playing.

Limits:

- Clips advance by display refreshes, so the same clip plays about 20% faster on NTSC than on PAL. See [Time base](../guides/animbin#time-base).
- A looping clip (flag bit 0 set) wraps to the start. Any other clip stops on its last frame.
- Only rotation keys move bones. Translation keys are loaded but not applied.
- The skeleton belongs to the mesh, not the game object. Every object using the same skinned mesh shows the same pose and shares one current clip, and each visible one advances it, so two visible copies play twice as fast.
- An object that is off screen is not drawn, so its clip does not advance while it is off screen.

## Quaternion

`src/quaternion.hh`

The rotation type used for bone rotations and animation keys. Its `w`, `x`, `y` and `z` are `psyqo::FixedPoint<>`, and a default `Quaternion` is the identity rotation.

```cpp
Quaternion turn = FromEulerAngles(0.0_pi, 0.5_pi, 0.0_pi, g_madnightEngine.m_trig);
psyqo::Matrix33 m = turn.ToRotationMatrix();
```

`FromEulerAngles` takes pitch, yaw and optionally roll. Multiply two quaternions with `*` to combine rotations, and use `Normalize()` to keep one unit length after a lot of maths. `DotProduct` and unary `-` are also there.

`Slerp(a, b, t)` blends from `a` to `b` as `t` goes from 0 to 1.

:::caution[`Slerp` is a normalised linear blend]
Despite the name it does not do a spherical interpolation. It is accurate for the small steps between neighbouring animation keys and drifts for large angles.
:::
