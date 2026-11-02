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
auto rx = g_madnightEngine.m_controllerHelper.GetNormalizedAnalogStickInput(pad, ControllerHelper::RightStickX);
auto ry = g_madnightEngine.m_controllerHelper.GetNormalizedAnalogStickInput(pad, ControllerHelper::RightStickY);
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

auto rx = g_madnightEngine.m_controllerHelper.GetNormalizedAnalogStickInput(pad1, ControllerHelper::RightStickX);
auto ry = g_madnightEngine.m_controllerHelper.GetNormalizedAnalogStickInput(pad1, ControllerHelper::RightStickY);
// GetNormalizedAnalogStickInput already checks the pad is connected/analog but does not provide any deadzone checks
// `ANALOG_STICK_DEADZONE`, `_X` and `_Y` are provided as sane starting values for a game to apply itself, deliberately,
// so the threshold stays the game's decision rather than the engine's.
if (rx != 0 || ry != 0) {
    rotationX = ry;
    rotationY = rx;
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
  void Clear(psyqo::Color clearColour = Lighting::instance().GetFogColour());
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

| Constant               | Value   | Purpose                                                                                                                                                                        |
| ---------------------- | ------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `ORDERING_TABLE_SIZE`  | 10,000  | Depth buckets in each frame's ordering table                                                                                                                                   |
| `FULL_FOG_DISTANCE`    | 3,500   | Screen-space Z past which fog is fully opaque, 2D sprite particles only (see [Fog on billboards and particles](#fog-on-billboards-and-particles))                              |
| `NEAR_FOG_DISTANCE`    | 2,000   | Screen-space Z where fog starts blending in, 2D sprite particles only (see [Fog on billboards and particles](#fog-on-billboards-and-particles))                                |
| `BUMP_ALLOCATOR_BYTES` | 125,000 | Per-frame draw-command arena (×2 for double buffering)                                                                                                                         |
| `SUBDIVISION_DISTANCE` | 750     | View-space distance closer than which large textured quads/tris get subdivided to reduce perspective warping (`renderer.cpp` subdivides when `zIndex <= SUBDIVISION_DISTANCE`) |

**Typical per-frame flow:** call `StartScene()` once from your scene's `start()`, then each frame call `Process()` to get `deltaTime`, update your game objects/scene, then `Render(deltaTime)` to flush the ordering table to the GPU. `Render()` clears the screen itself (see [Clear colour](#clear-colour)), so `Clear()` is only needed in frames that draw without it. `Renderer` draws whatever `GameObjectManager::GetActiveGameObjects` returns each frame (see [Core](./core#gameobjectmanager) on the renderable-objects subset, which currently isn't wired up to anything); visibility is culled per-object against the camera via `IsGameObjectVisible` internally, using each object's bounding volume.

Ambient colour, the clear colour and fog are all applied by the renderer but configured through `Lighting`; see [Lighting](#lighting) and [Fog](#fog) below.

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
- `Render()` walks game objects, then billboards, then particles, all against the _same_ per-frame ordering table — draw order between those three categories is fixed, not something you control per-call.
- `RenderSprite` draws a single 2D sprite, a region of a `TimFile` given by `uv` and placed at `rect`, straight to the screen, outside the ordering table. Up to 40 `RenderSprite` calls are supported per frame: `m_tpages`/`m_sprites` are fixed 40-entry arrays, indexed by `m_currentSpriteFragment`, which resets once per frame inside `Process()`.

:::caution[Bump allocator exhaustion is unguarded outside the subdivision path]
`OrderingTable::insert` clamps silently to a valid bucket in its default `Safe` mode. `BumpAllocator::allocateFragment`, used for the per-frame 125,000-byte arena, behaves the opposite way in its default `Safe` mode: it calls `Kernel::assert` and hard-crashes once the arena is exhausted. `SubdivideTexturedQuad`/`SubdivideTexturedTriangle` check `remaining()` first and fall back to inserting the polygon unsubdivided when there isn't room; the plain per-object, per-billboard, and per-particle fragment allocations elsewhere in `RenderGameObjects`/`RenderBillboards`/`RenderParticles` don't, so a scene that overflows the arena outside the subdivision path will assert.
:::

:::note[Removed]
`RenderLoadingScreen` is gone — the built-in loading scene now just calls `Clear()` and prints its own percentage text via `SystemFont()->chainprintf(...)` directly, rather than the renderer owning a dedicated loading-screen draw call. See [Helpers → FileLoader](./helpers#fileloader) for the loading flow this feeds into.
:::

## Lighting

`src/render/lighting.hh`

`Lighting` is a singleton, reached through `Lighting::instance()`, that holds the settings the renderer reads while it draws: the ambient colour, the fog colour (which is also the colour the screen is cleared to), and which fog mode is on. None of it belongs to a scene, so whatever you set stays set across scene switches until you change it.

### Ambient colour

The ambient colour scales the colour of everything the renderer draws: every vertex colour on a mesh face, and the colour of every billboard and particle, is multiplied by it before it reaches the GPU. Each channel works as a scale where 128 means "leave it alone", so:

- the default, `{128, 128, 128}`, draws everything exactly as authored;
- `{64, 64, 64}` draws everything at half brightness;
- `{96, 96, 128}` darkens red and green but keeps blue, for a cool night-time cast.

It is a flat scale, not a light. There's no direction and no per-object setting, so every face in the scene is scaled by the same amount. Use it to set the mood of a level (a dark cave, a red alarm state) or to fade a scene in or out.

The renderer reads it every time it draws, so you can change it whenever you like, including in the middle of a scene, and the change shows up on the next frame. There's no `Renderer` wrapper for it; set it on `Lighting` directly:

```cpp
auto &lighting = Lighting::instance();
lighting.SetAmbientColour({96, 96, 128}); // night: darker, with a blue cast

// later, back to the colours as authored
lighting.SetAmbientColour({128, 128, 128});
```

The cost is a multiply and a shift per colour channel on the CPU, for every vertex colour drawn. It's applied whether fog is on or not.

### Clear colour

There is no separate background colour. At the start of every `Render()` the screen is cleared to the fog colour, whether fog is turned on or not, so with fog off the fog colour is simply your background colour. It starts out black.

To change it, set the fog colour:

```cpp
Renderer::Instance().SetFogColour({40, 60, 120}); // a blue sky behind everything, no fog involved
```

`Render()` does its own clearing, so a frame that calls it never needs `Clear()`, and a colour passed to `Clear()` in the same frame won't show, because `Render()` reuses the same clear command with the fog colour. `Clear()` is for frames that draw without `Render()`, the way the built-in loading scene clears the screen and then prints its progress text. With no argument it clears to the fog colour; pass a colour to use a different one for that frame:

```cpp
Renderer::Instance().Clear();          // fog colour
Renderer::Instance().Clear({0, 0, 0}); // black, this frame only
```

### Reference

```cpp
class Lighting {
public:
  static Lighting &instance();

  const bool &IsSimpleFogEnabled(void) const;
  void EnableSimpleFog(void);
  void DisableSimpleFog(void);

  const bool &IsAdvancedFogEnabled(void) const;
  void EnableAdvancedFog(void);
  void DisableAdvancedFog(void);

  const psyqo::Color &GetAmbientColour(void) const;
  void SetAmbientColour(psyqo::Color colour);

  const psyqo::Color &GetFogColour(void) const;
  void SetFogColour(psyqo::Color colour);
};
```

`Renderer` also has `SetFogColour` and `IsSimpleFogEnabled`. Prefer `Renderer::Instance().SetFogColour` over `Lighting::instance().SetFogColour`; see [Changing the fog colour](#changing-the-fog-colour) for why.

:::note[Renamed]
`SetAmbient` was renamed to `SetAmbientColour`, and `m_ambient`/`m_fogColour`/`m_isSimpleFogEnabled` are now private. Use the getters above instead of reaching for the fields directly.
:::

## Fog

Fog blends geometry toward the fog colour the further it is from the camera. Because the screen is also cleared to the fog colour, distant geometry fades into the background rather than ending at a hard edge.

There are two modes, and both are off by default:

|                           | Simple fog                                       | Advanced fog                                                       |
| ------------------------- | ------------------------------------------------ | ------------------------------------------------------------------ |
| Textured faces            | Tinted toward the fog colour, never fully faded  | Fade all the way into the fog colour                               |
| Good for                  | Dark fog colours, keeping the draw cost down     | Light fog colours, heavily textured scenes                         |
| Extra GPU commands        | None                                             | One more per partly fogged textured face, more for textures with holes |
| Where it starts           | Further from the camera                          | Closer to the camera                                               |
| Needs from your textures  | Nothing                                          | The STP bit, which `LoadTIM` sets by default                       |

If both are turned on, advanced fog wins and simple fog does nothing.

### Turning fog on

`StartScene()` looks at which mode is on and writes that mode's fog distances to the GTE. It does that once, when it's called, so turn the mode on before `StartScene()` runs: at the top of your scene's `start()`, or before you switch to the scene.

```cpp
void GameplayScene::start(StartReason reason) {
  Lighting::instance().EnableAdvancedFog();
  Renderer::Instance().SetFogColour({20, 20, 30});
  Renderer::Instance().StartScene(); // picks up the fog mode set above
  // ...
}
```

Or from game code, before the scene starts:

```cpp
Lighting::instance().EnableSimpleFog();
Renderer::Instance().SetFogColour({20, 20, 30});
g_madnightEngine.SwitchScene(&gameplayScene); // gameplayScene's start() calls StartScene()
```

Things to know:

- The on/off state is checked every frame, so `DisableSimpleFog()` and `DisableAdvancedFog()` take effect straight away. Turning a mode on, or switching from one mode to the other, after `StartScene()` draws with whatever fog distances the GTE was last given, if any, rather than that mode's own. Call `StartScene()` again or set the mode before the scene starts.
- The fog distances are fixed per mode. `Renderer::SetFogNearFar` is private, so a game can't move them. Advanced fog starts closer to the camera and reaches full fog sooner than simple fog.

### Changing the fog colour

Use `Renderer::Instance().SetFogColour`. It stores the colour on `Lighting` and also writes it into the GTE, which is where simple fog reads it from. You can call it at any time, including mid-scene.

`Lighting::instance().SetFogColour` only stores the colour. The background and advanced fog pick it up on the next frame, but simple fog on meshes, billboards and 3D particles keeps using the old colour until the next `StartScene()`.

```cpp
// a storm rolling in: switch to a darker fog mid-scene
Renderer::Instance().SetFogColour({60, 60, 70});
```

### Simple fog

Simple fog blends each vertex colour toward the fog colour based on its depth, using the GTE. It runs after faces have been culled and clipped, so only faces that actually get drawn pay for it, and it adds no extra GPU commands.

On a textured face the vertex colour tints the texture rather than replacing it, so simple fog tints textured faces toward the fog colour without ever fully fading them out. Dark parts of a texture stay dark however far away they are. That suits a dark fog colour such as night-time black or deep blue, where the result looks like the scene fading into darkness. With a pale fog colour, textured faces won't fade into the background; use [advanced fog](#advanced-fog) for that.

```cpp
void GameplayScene::start(StartReason reason) {
  Lighting::instance().EnableSimpleFog();
  Renderer::Instance().SetFogColour({8, 8, 16}); // dark fog, where simple fog looks right
  Renderer::Instance().StartScene();
}
```

The cost is one GTE depth-cue operation per vertex of each drawn face.

### Advanced fog

Advanced fog (Silent Hill style) gives textured faces a real blend, `texel * (1 - t) + fog * t` where `t` is the fog amount at that vertex, so textured geometry fades fully into any fog colour, light or dark.

```cpp
void GameplayScene::start(StartReason reason) {
  Lighting::instance().EnableAdvancedFog();
  Renderer::Instance().SetFogColour({160, 160, 170}); // pale grey fog
  Renderer::Instance().StartScene();
}
```

How it draws, and what that means for you:

- A textured face with any fogged vertex is drawn twice: first an opaque untextured face in the fog colour scaled by `t`, then the textured face semi-transparent on top, with its vertex colours scaled by `(1 - t)`. Only texels with the STP bit set take part in the blend, which is why [`LoadTIM`](./textures) sets that bit unless the texture is loaded with flag 1.
- A face with no fogged vertices is drawn once, as normal.
- A face whose vertices are all fully fogged is skipped. The background shows through instead, and since `Render()` clears to the fog colour, it looks the same.
- An untextured face is always a single primitive, with its colour already blended as `vertex * (1 - t) + fog * t`.
- A texture with holes (transparent texels) has to be loaded with flag 1, so the fog is drawn on its texels and not over the holes. Each partly fogged face using it costs eight GPU commands, three of them polygons, instead of two. See [Textures](./textures) for how to load with flag 1, and the [SCENEBIN guide](../guides/scenebin#texture-extra-placement) for doing it from a manifest.

The extra commands come out of the same 125,000-byte per-frame arena as everything else, so a scene that fits without fog may not fit with it (see the bump allocator note under [Renderer](#renderer)). For detailed models, [quick advanced fog](#quick-advanced-fog) trades some accuracy for a much lower cost.

### Quick advanced fog

Setting the `RF_QUICK_ADVANCED_FOG` render flag on a `GameObject` swaps the second primitive per face for one primitive for the whole object. Each face is darkened by `(1 - t)`, then a single additive quad covering the object's screen rectangle adds the fog colour scaled by the object's average `t` across all its vertices. The flag does nothing unless advanced fog is on.

Use it for detailed models with lots of faces, such as characters, where drawing every face twice gets expensive. Because the whole object gets one fog amount, it suits compact objects; a long object stretching away from the camera gets the same fog along its whole length.

```cpp
// hero.tim must be loaded with flag 1 (see below)
GameObject *hero = g_madnightEngine.m_gameObjectManager.GetGameObjectByName("hero");
if (hero) {
  hero->SetRenderFlag(RF_QUICK_ADVANCED_FOG);
}
```

The quad only draws on pixels with the mask bit clear, so:

- the object's texture must be loaded with flag 1, or its texels come out with the mask bit set and get no fog;
- anything else inside that rectangle with the mask bit clear (the cleared background, other untextured or flag-1 geometry) gets the same add, which shows as a faint box around the object.

The cost is four extra GPU commands per object (the quad plus three state changes), however many faces it has.

### Fog on billboards and particles

Billboards and particles use one colour for all their corners. It's scaled by the ambient colour, then blended toward the fog colour using the depth of a single corner. That's a plain colour blend in both modes, without advanced fog's second primitive, so a textured billboard or particle is tinted toward the fog colour the way [simple fog](#simple-fog) tints textured faces.

Particles from a 2D emitter (`AreParticles2D()`) are drawn as sprites and take their fog amount from the CPU instead of the GTE: none up to a screen depth of `NEAR_FOG_DISTANCE` (2,000), full at `FULL_FOG_DISTANCE` (3,500), and a straight line between the two. Those two constants only affect 2D particles, so they won't line up exactly with the fog on meshes and billboards.

## Colour constants

`src/render/colour.hh`

```cpp
static constexpr psyqo::Color COLOUR_WHITE  = {.r = 255, .g = 255, .b = 255};
static constexpr psyqo::Color COLOUR_YELLOW = {.r = 255, .g = 255, .b = 0};
```

Used as UI defaults (e.g. `TextHUDElement`'s default colour, `MenuItem`'s default/selected text colour).

## Clipping

`src/render/clip.hh`

Free functions used by `RenderGameObjects`/`RenderBillboards`/`RenderParticles` to trivially reject screen-space primitives against the clip rectangle before they're pushed to the ordering table.

```cpp
// Returns non-zero if triangle (v0, v1, v2) is outside `clip`.
int tri_clip(const psyqo::Rect *clip, psyqo::Vertex *v0, psyqo::Vertex *v1, psyqo::Vertex *v2);

// Returns non-zero if quad (v0, v1, v2, v3) is outside `clip`.
int quad_clip(const psyqo::Rect *clip, psyqo::Vertex *v0, psyqo::Vertex *v1, psyqo::Vertex *v2, psyqo::Vertex *v3);
```

There is no near-plane or frustum clipping anywhere in this section: these are a trivial reject only, run after GTE projection, checking whether the already-projected 2D vertices fall entirely outside the screen rectangle on one side, and correctly rejecting the primitive when they do. Nothing here clips a polygon that straddles the screen edge or the near plane: it's a whole-primitive accept/reject test.
