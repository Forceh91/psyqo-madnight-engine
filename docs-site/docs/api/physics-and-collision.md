---
title: Physics & Collision
sidebar_position: 5
---

# Physics & Collision

`src/core/collision.hh`, `src/core/raycast.hh` and `src/mesh/colbin_manager.hh` give you overlap tests, raycasting and loading baked `.COLBIN` level collision (see the [COLBIN format spec](../guides/colbin)).

:::caution[The engine provides the tests, not the response]
Nothing in the engine calls `IsAABBCollision`, `IsSATCollision` or `RaycastScene` for you, and nothing reads the floor and grid data a `.COLBIN` loads. Pushing the player out of walls, ground detection and "what am I looking at" prompts are yours to write using these calls.
:::

## Collision

`src/core/collision.hh`, reached as `g_madnightEngine.m_collisionHelper`.

### Box overlap with IsAABBCollision

`IsAABBCollision` is the cheap test. It checks whether two axis-aligned boxes overlap, and ignores rotation. Use it for a quick yes or no on objects that do not rotate.

```cpp
// both boxes must be in the same (world) space
AABBCollision boxA = {pickupPos - halfSize, pickupPos + halfSize};
AABBCollision boxB = {playerPos - halfSize, playerPos + halfSize};
auto &collision = g_madnightEngine.m_collisionHelper;
if (collision.IsAABBCollision(boxA, boxB)) {
  // overlapping
}
```

You build the boxes yourself, in the same space. For objects that rotate, use `IsSATCollision`, which takes the `obb()` an object keeps up to date for you.

### Oriented boxes with IsSATCollision

`IsSATCollision` tests two oriented boxes (`obb()` on a [`GameObject`](./core#gameobject), or a wall from a `.COLBIN`) and takes rotation into account. When they overlap it returns `true` and fills in a `CollisionTest`:

| Field | Meaning |
| --- | --- |
| `mtv` | The smallest move that separates the two boxes. |
| `normal` | The direction of that move. |
| `penetration` | How far they overlap along it. |

To push the first box out of the second, add `mtv` to its position:

```cpp
CollisionTest result;
if (g_madnightEngine.m_collisionHelper.IsSATCollision(player->obb(), wall->obb(), &result)) {
  player->SetPosition(player->pos() + result.mtv);
}
```

The first argument is the box that gets pushed. Swap the arguments and `mtv` points the other way. The `CollisionTest` is only meaningful when the call returns `true`.

It tests 15 axes: each box's three face directions and the nine cross products between them. The axis with the smallest overlap becomes the push-out direction.

`GenerateAABBForMesh` is marked deprecated in the source ("should be done offline, DO NOT USE"). Boxes are baked into the mesh at export time, so use `collisionBox` instead.

`OBB` and `AABBCollision` are described in [Core](./core#collision-types).

## Raycast

`src/core/raycast.hh`, reached as `g_madnightEngine.m_raycast`.

`RaycastScene` fires a ray and reports the first object with a given [tag](./core#gameobjecttag) that it hits. Filtering by tag keeps it cheap and is how you ask something like "what interactable is in front of me".

```cpp
Ray ray = {.origin = camera->pos(), .direction = camera->forwardVector(), .maxDistance = 2.0_ws};
RayHit hit = {0};
if (g_madnightEngine.m_raycast.RaycastScene(ray, GameObjectTag::INTERACTABLE, &hit)) {
  ShowInteractPrompt(hit.object);
}
```

On a hit, `hit.object` is the object, `hit.distance` is how far away it was and `hit.hitPos` is the point in world space. `hit.hit` is also set to `true`.

Limits to know about:

- `maxDistance` must be greater than zero and no more than `maxRayDistance`, which is `10.0_ws` (20 m, or `20 * ONE_METRE`). Anything outside that returns `false` without testing anything.
- The test is against an axis-aligned box, so it is inaccurate for rotated objects (the source says so too). An object with a mesh is tested against the mesh's baked box moved to the object's position, ignoring its rotation. A mesh-less object, such as a trigger, uses its oriented box's extents instead.
- It returns the first object found, not the closest. If two tagged objects line up along the ray, which one you get depends on the order they are stored in.
- It does not hit an object the ray starts inside.
- `direction` should be a normalised vector. The distance is worked out from a normalised copy, but `hitPos` is computed from the vector you passed in.

## ColbinManager

`src/mesh/colbin_manager.hh`, reached as `g_madnightEngine.m_colbinManager`.

Loads one `.COLBIN` collision file for the current level: floor triangles, wall boxes and a grid bucketing the walls. The usual place to load it is in your load queue (see [Loading Assets](../getting-started/loading-assets)). To load it by hand:

```cpp
ColBin *level;
co_await g_madnightEngine.m_colbinManager.LoadColbin("level01.colbin", &level);
```

`walls()` gives you the wall boxes as a span, ready to feed to `IsSATCollision`:

```cpp
for (auto &wall : g_madnightEngine.m_colbinManager.walls()) {
  CollisionTest result;
  if (g_madnightEngine.m_collisionHelper.IsSATCollision(player->obb(), wall, &result)) {
    player->SetPosition(player->pos() + result.mtv);
  }
}
```

Things to know:

- It holds a single collision mesh. Loading another replaces the first, so load a new one only when you change level.
- Loaded data is freed by `Dump()`. The file loader calls it for you when it starts a loading screen that clears pools.
- The floor triangles (`floors`) and the wall grid (`gridCells`) are parsed and stored, but the engine never reads them. Ground detection and grid lookups are yours to write; the [COLBIN spec](../guides/colbin) describes the layout and the exporter's cell numbering if you want to.
