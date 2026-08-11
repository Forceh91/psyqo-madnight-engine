---
title: Render
sidebar_position: 3
---

# Render

`src/render/` — camera, the renderer itself, lighting/fog state, colour constants, and 2D clipping helpers.

## Camera

`src/render/camera.hh`

Supports three modes, switchable at runtime:

```cpp
enum CameraMode {
  FIXED,     // camera doesn't move on its own; LookAt still works. Good for cutscenes —
             // recommended to take input control away from the player in this mode.
  FOLLOW,    // orbits a tracked position at a set distance (third-person); LookAt is called
             // automatically each frame.
  FREE_LOOK, // player has full control over rotation (first-person style); you're responsible
             // for updating position yourself as the player moves.
};
```

```cpp
class Camera final {
public:
  Camera();
  Camera(psyqo::Vec3 pos);
  Camera(psyqo::FixedPoint<12> x, psyqo::FixedPoint<12> y, psyqo::FixedPoint<12> z);

  const psyqo::Vec3 pos(void) const;
  const psyqo::Vec3 deltaOffset(void) const; // zero unless in FOLLOW mode
  const psyqo::Vec3 *posPtr(void) const;
  const CameraAngle *angle(void) const;
  const psyqo::Vec3 forwardVector(void) const;
  const psyqo::Vec3 rightVector(void) const;
  const psyqo::Vec3 upVector(void) const;
  const psyqo::Matrix33 &rotationMatrix(void);
  psyqo::Matrix33 inverseRotationMatrix(void);

  void Process(uint32_t deltaTime);
  void SetPosition(psyqo::FixedPoint<> x, psyqo::FixedPoint<> y, psyqo::FixedPoint<> z);
  void SetPosition(psyqo::Vec3 pos);
  void SetAngle(psyqo::Angle x, psyqo::Angle y, psyqo::Angle z);
  void SetAngle(CameraAngle angle);

  void SetFixed(void);
  void SetFixed(psyqo::Vec3 pos);
  void SetFixed(psyqo::Vec3 pos, CameraAngle angle);
  void ClearFixed(void); // returns to the previously set mode

  // pos: pointer to what you want to track (e.g. player position).
  // distance is in metres — keep it small (128px = 1m).
  // forces FOLLOW mode; only takes effect while in that mode.
  void SetFollow(psyqo::Vec3 *pos, psyqo::FixedPoint<> distance);
  void SetFollow(psyqo::Vec3 *pos, psyqo::Vec2 offsetPos, psyqo::FixedPoint<> distance);
  void ClearFollow(void); // stops following, becomes a fixed camera

  void SetFreeLook(void);
  void SetFreeLook(psyqo::Vec3 pos);
  void SetFreeLook(psyqo::Vec3 pos, CameraAngle initialAngle);
  void SetFreeLookMaxAngles(CameraMaxAngle maxAngles); // defaults to ±0.5_pi (±90°)
  void ClearFreeLook(void); // becomes a fixed camera

  void LookAt(const psyqo::Vec3 *target);

  const psyqo::Vec3 SwingTarget(void) const;
  void SetSwingTarget(const psyqo::Vec3 &target);

  const CameraAngle OrbitAngle(void) const;
  void SetOrbitAngle(const CameraAngle &angle);
  void UpdateOrbitAngles(psyqo::Angle xDeltaAmount, psyqo::Angle yDeltaAmount);
  void UpdateOrbitAngles(psyqo::Angle xAmount, psyqo::Angle yAmount, uint32_t deltaTime);
  void ResetOrbitAngles(void);

  void UpdateAngles(psyqo::Angle xDeltaAmount, psyqo::Angle yDeltaAmount, psyqo::Angle zDeltaAmount);
  void UpdateAngles(psyqo::Angle xDeltaAmount, psyqo::Angle yDeltaAmount, psyqo::Angle zDeltaAmount, uint32_t deltaTime);
};
```

- **`FOLLOW` mode + orbit:** set a tracked position with `SetFollow`, then feed player input into `UpdateOrbitAngles` each frame to let the player orbit the camera around that position — this is the standard third-person setup.
- **`FREE_LOOK` mode:** feed input into `UpdateAngles`; you still need to call `SetPosition` yourself as the tracked entity (e.g. the player) moves, since the camera doesn't follow anything automatically in this mode.
- The `deltaTime`-suffixed overloads of `UpdateAngles`/`UpdateOrbitAngles` scale the rotation delta by frame time; the non-`deltaTime` overloads expect an already-scaled delta.
- `SetFixed`/`SetFreeLook`/`SetFollow` are mutually exclusive — switching modes updates `m_cameraMode`, and `ClearFixed` restores whichever mode was active before the most recent `SetFixed` call.

### Usage

Third-person orbit camera, driven by the right stick:

```cpp
Camera camera;
camera.SetFollow(player->posPtr(), /*distance*/ 3.0_ws);
Renderer::Instance().SetActiveCamera(&camera);

// per-frame, feeding controller input into the orbit
auto rx = ControllerHelper::GetNormalizedAnalogStickInput(pad, ControllerHelper::RightStickX);
auto ry = ControllerHelper::GetNormalizedAnalogStickInput(pad, ControllerHelper::RightStickY);
camera.UpdateOrbitAngles(ry * ORBIT_SPEED, rx * ORBIT_SPEED, deltaTime);
camera.Process(deltaTime); // recalculates orbit position + LookAt
```

First-person free-look, driven by the same stick input, with you owning position updates:

```cpp
camera.SetFreeLook(player->pos());
camera.UpdateAngles(ry * LOOK_SPEED, rx * LOOK_SPEED, 0, deltaTime);
camera.SetPosition(player->pos()); // camera doesn't follow on its own in this mode
```

A static camera — `FIXED` is the default mode, so a menu/cutscene camera that never moves needs nothing beyond construction and an angle:

```cpp
m_camera = new Camera(psyqo::Vec3{22.5_ws, -1.75_ws, -2.5_ws});
m_camera->SetAngle({0, 1.0_pi, 0}); // face back along Z
instance.SetActiveCamera(m_camera);
```

A third pattern seen in shipped game code: a `FIXED` camera that tracks a target's X/Z position every frame while you drive rotation manually — a lighter-weight alternative to `FOLLOW`/`FREE_LOOK` when you don't need true orbiting, just a camera that stays behind the player and lets them look around:

```cpp
m_camera->SetFixed({0, -1.0_ws, 0}, {0, 0, 0});

// per-frame:
int rotationX = 0, rotationY = 0;
if (input.isButtonPressed(pad1, psyqo::AdvancedPad::L2)) rotationY = -128;
if (input.isButtonPressed(pad1, psyqo::AdvancedPad::R2)) rotationY = 128;

if (ControllerHelper::IsPadAnalog(pad1)) {
    auto rx = ControllerHelper::GetNormalizedAnalogStickInput(pad1, ControllerHelper::RightStickX);
    auto ry = ControllerHelper::GetNormalizedAnalogStickInput(pad1, ControllerHelper::RightStickY);
    // GetNormalizedAnalogStickInput applies no deadzone itself (see Controller) -- thresholded here
    constexpr int deadzone = 16;
    if (ry < -deadzone || ry > deadzone) rotationX = ry;
    if (rx < -deadzone || rx > deadzone) rotationY = rx;
}

auto pos = m_character->posPtr();
m_camera->SetPosition(pos->x, -0.75_ws, pos->z); // follow the player on X/Z every frame

auto angle = m_camera->angle();
auto pitch = angle->x + (rotationX >> 5) * 0.005_pi * deltaTime;
m_camera->SetAngle(eastl::clamp(pitch, -0.3_pi, 0.3_pi), angle->y + (rotationY >> 5) * 0.005_pi * deltaTime, angle->z);
```

### Internals

- `FOLLOW` mode recomputes position via `CalculateOrbitPosition()` every `Process()` call — it derives forward/right/up from `m_orbitAngle` and steps back from the tracked point by `distance`, so orbiting is really just "point the camera, then back up".
- Orbit pitch is clamped tighter than yaw (`±0.21π` vs full `±π`) to stop the camera flipping over the top/bottom of whatever it's orbiting.

## Renderer

`src/render/renderer.hh`

Singleton owning the GPU ordering tables, per-frame bump allocator, and all high-level draw calls. Built around `psyqo::OrderingTable` (depth-sorted draw commands) and a per-frame [`psyqo::BumpAllocator`](https://github.com/grumpycoders/pcsx-redux) (no per-frame heap allocation — everything is carved out of a fixed 125,000-byte arena, doubled across the two frame buffers).

```cpp
class Renderer final {
public:
  static void Init(psyqo::GPU &gpuInstance);
  static Renderer &Instance();

  void StartScene(void);
  void VRamUpload(const uint16_t *data, int16_t x, int16_t y, int16_t width, int16_t height);

  // must be called once per scene frame; returns the delta time.
  // if deltaTime comes back 0, it's recommended to early-return rather than render.
  uint32_t Process(void);

  void Render(void);
  void Render(uint32_t deltaTime);
  void Clear(psyqo::Color clearColour = Lighting::instance().m_fogColour);
  void RenderLoadingScreen(uint16_t loadPercentage);
  void RenderSprite(const TimFile *tim, const psyqo::Rect rect, const psyqo::PrimPieces::UVCoords uv);

  void SetActiveCamera(Camera *camera);
  const Camera* ActiveCamera(void) const;

  void SetFogColour(const psyqo::Color &colour);
  const bool& IsSimpleFogEnabled(void) const;

  psyqo::GPU &GPU();
  psyqo::Font<100> *SystemFont();
};
```

Key constants (`src/render/renderer.hh`):

| Constant | Value | Purpose |
|---|---|---|
| `ORDERING_TABLE_SIZE` | 10,000 | Depth buckets in each frame's ordering table |
| `FULL_FOG_DISTANCE` | 3,500 | Screen-space Z past which fog is fully opaque |
| `NEAR_FOG_DISTANCE` | 2,000 | Screen-space Z where fog starts blending in |
| `BUMP_ALLOCATOR_BYTES` | 125,000 | Per-frame draw-command arena (×2 for double buffering) |
| `SUBDIVISION_DISTANCE` | 750 | View-space distance beyond which large textured quads/tris get subdivided to reduce perspective warping |

**Typical per-frame flow:** call `Process()` to get `deltaTime`, `Clear()`/`StartScene()`, update and render your game objects/scene, then `Render(deltaTime)` to flush the ordering table to the GPU. `GameObjectManager`'s renderable-objects list (see [Core](./core#gameobjectmanager)) drives what `Renderer` actually draws each frame; visibility is culled per-object against the camera via `IsGameObjectVisible` internally, using each object's bounding sphere/AABB.

Fog is GTE-accelerated: `ApplyFogToColourGTE` uses the GTE's depth-cueing (`dpcs`) and perspective (`rtps`) registers rather than doing the lerp on the CPU per-vertex, and colour work is deferred until after visibility culling so only visible faces pay the cost.

### Usage

```cpp
void GameplayScene::start(StartReason reason) {
  Renderer::Instance().StartScene();
  m_camera = new Camera();
  Renderer::Instance().SetActiveCamera(m_camera);
}

void GameplayScene::frame() {
  auto &renderInstance = Renderer::Instance();
  uint32_t deltaTime = renderInstance.Process();
  if (deltaTime == 0) return; // nothing to do this call

  m_camera->Process(deltaTime);
  renderInstance.Render(deltaTime); // draws every renderable game object, billboard, and particle
}
```

### Internals

- `Process()` diffs `m_gpu.getFrameCount()` against the last call and returns 0 if nothing's changed yet — that's the "early return on 0" the header comment recommends, and it's how the engine avoids doing GTE/render work more than once per actual display refresh.
- `Render()` walks game objects, then billboards, then particles, all against the *same* per-frame ordering table — draw order between those three categories is fixed, not something you control per-call.

## Lighting

`src/render/lighting.hh`

Singleton holding the current scene's ambient colour and fog state. The renderer reads from it; you configure it through `Renderer::SetFogColour` rather than touching `Lighting` directly, so the renderer stays the single point of contact for render state.

```cpp
class Lighting {
public:
  static Lighting &instance();

  psyqo::Color m_ambient = {128, 128, 128};
  psyqo::Color m_fogColour = DEFAULT_CLEAR_COLOR; // clear colour IS fog colour
  bool m_isSimpleFogEnabled = false;

  void EnableSimpleFog(void);
  void DisableSimpleFog(void);
  void SetAmbient(psyqo::Color colour);
  void SetFogColour(psyqo::Color colour);
};
```

```cpp
Lighting::instance().SetAmbient({80, 80, 100});   // dim, slightly blue ambient
Renderer::Instance().SetFogColour({20, 20, 30});  // also updates the GTE's far-colour registers
```

## Colour constants

`src/render/colour.hh`

```cpp
static constexpr psyqo::Color COLOUR_WHITE  = {.r = 255, .g = 255, .b = 255};
static constexpr psyqo::Color COLOUR_YELLOW = {.r = 255, .g = 255, .b = 0};
```

Used as UI defaults (e.g. `TextHUDElement`'s default colour, `MenuItem`'s default/selected text colour).

## Clipping

`src/render/clip.hh`

Free functions used internally by the renderer's subdivision logic to test screen-space primitives against the clip rectangle before they're pushed to the ordering table.

```cpp
// Returns non-zero if triangle (v0, v1, v2) is outside `clip`.
int tri_clip(const psyqo::Rect *clip, psyqo::Vertex *v0, psyqo::Vertex *v1, psyqo::Vertex *v2);

// Returns non-zero if quad (v0, v1, v2, v3) is outside `clip`.
int quad_clip(const psyqo::Rect *clip, psyqo::Vertex *v0, psyqo::Vertex *v1, psyqo::Vertex *v2, psyqo::Vertex *v3);
```
