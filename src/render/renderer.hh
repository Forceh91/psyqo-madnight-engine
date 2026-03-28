#ifndef _RENDERER_H
#define _RENDERER_H

#include "../textures/texture_manager.hh"
#include "../core/collision_types.hh"

#include "camera.hh"
#include "psyqo/bump-allocator.hh"
#include "psyqo/fixed-point.hh"
#include "psyqo/font.hh"
#include "psyqo/fragments.hh"
#include "psyqo/gpu.hh"
#include "psyqo/matrix.hh"
#include "psyqo/primitives/common.hh"

static constexpr uint16_t ORDERING_TABLE_SIZE = 4'000;
static constexpr uint16_t FULL_FOG_DISTANCE = 3'500; // screen z
static constexpr uint16_t NEAR_FOG_DISTANCE = 2'000; // screen z
static constexpr uint32_t BUMP_ALLOCATOR_BYTES = 125'000; // this is for each frame, so double what this number is is used up in RAM
static constexpr psyqo::Color m_clearColour = {.r = 0, .g = 0, .b = 0};
static constexpr psyqo::Color c_loadingBackgroundColour = {.r = 0, .g = 0, .b = 0};

class Renderer final {
  static Renderer *m_instance;
  static psyqo::Font<> m_systemFont;

  psyqo::GPU &m_gpu;
  uint32_t m_lastFrameCounter = 0;
  Camera *m_activeCamera;
  psyqo::Vec3 m_gteCameraPos = {0, 0, 0};
  bool m_isSimpleFogEnabled = false;

  // create 2 ordering tables, one for each frame buffer
  psyqo::OrderingTable<ORDERING_TABLE_SIZE> m_orderingTables[2];

  // when using ordering tables we also need to sort fill commands as well
  psyqo::Fragments::SimpleFragment<psyqo::Prim::FastFill> m_clear[2];

  // bump allocator so we're not guessing at runtime how many quads/lines/etc/etc/etc we're gonna have
  psyqo::BumpAllocator<BUMP_ALLOCATOR_BYTES> m_allocators[2];

  // texture page + sprite info
  // TODO: move to bump allocator?
  eastl::array<psyqo::Fragments::SimpleFragment<psyqo::Prim::TPage>, 40> m_tpages[2];
  eastl::array<psyqo::Fragments::SimpleFragment<psyqo::Prim::Sprite>, 40> m_sprites[2];
  uint8_t m_currentSpriteFragment = 0;

  Renderer(psyqo::GPU &gpuInstance) : m_gpu(gpuInstance){};
  ~Renderer(){};

  psyqo::Vec3 SetupCamera(const psyqo::Matrix33 &camRotationMatrix, const psyqo::Vec3 &negativeCamPos);
  psyqo::Vec3 TransformObjectToViewSpace(const psyqo::Vec3 &pos, const psyqo::Matrix33 &cameraRotationMatrix, const psyqo::Matrix33 &finalCameraMatrix);

  void RenderGameObjects(uint32_t deltaTime, const psyqo::Matrix33 &cameraRotationMatrix);
  void RenderBillboards(uint32_t deltaTime, const psyqo::Matrix33 &cameraRotationMatrix);
  void RenderParticles(uint32_t deltaTime, const psyqo::Matrix33 &cameraRotationMatrix);
  
  bool IsGameObjectVisible(const AABBCollision& collisionBox, const psyqo::FixedPoint<>& boundingSphereRadius);

  psyqo::FixedPoint<> GetFogFactor(uint32_t z);
  void ApplyFogToColour(psyqo::Color* col, psyqo::FixedPoint<> fogFactor);
public:
  static void Init(psyqo::GPU &gpuInstance);

  void StartScene(void);
  void VRamUpload(const uint16_t *data, int16_t x, int16_t y, int16_t width, int16_t height);
  // returns the delta time
  // must be called on each scene frame
  // if `deltaTime` is 0 then its recommended to early return
  // and not go any further with your rendering
  uint32_t Process(void);
  void Render(void);
  void Render(uint32_t deltaTime);
  void Clear(void);
  void RenderLoadingScreen(uint16_t loadPercentage);
  void RenderSprite(const TimFile *tim, const psyqo::Rect rect, const psyqo::PrimPieces::UVCoords uv);
  void SetActiveCamera(Camera *camera);
  const Camera* ActiveCamera(void) const { return m_activeCamera; }
  const bool& IsSimpleFogEnabled(void) const { return m_isSimpleFogEnabled; }
  void EnableSimpleFog(void) { m_isSimpleFogEnabled = true; }
  void DisableSimpleFog(void) { m_isSimpleFogEnabled = false; }

  static Renderer &Instance() { return *m_instance; }
  psyqo::GPU &GPU() { return m_gpu; }
  psyqo::Font<> *SystemFont() { return &m_systemFont; }
};

#endif
