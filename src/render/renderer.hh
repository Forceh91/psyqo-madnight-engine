#ifndef _RENDERER_H
#define _RENDERER_H

#include "../textures/texture_manager.hh"
#include "../core/collision_types.hh"
#include "lighting.hh"

#include "camera.hh"
#include "psyqo/bump-allocator.hh"
#include "psyqo/fixed-point.hh"
#include "psyqo/font.hh"
#include "psyqo/fragments.hh"
#include "psyqo/gpu.hh"
#include "psyqo/matrix.hh"
#include "psyqo/primitives/common.hh"

static constexpr uint16_t ORDERING_TABLE_SIZE = 10'000;
static constexpr uint16_t FULL_FOG_DISTANCE = 3'500; // screen z
static constexpr uint16_t NEAR_FOG_DISTANCE = 2'000; // screen z
static constexpr uint32_t BUMP_ALLOCATOR_BYTES = 125'000; // this is for each frame, so double what this number is is used up in RAM
static constexpr uint16_t SUBDIVISION_DISTANCE = 750; // after view space transformation
static constexpr psyqo::Color c_loadingBackgroundColour = {.r = 0, .g = 0, .b = 0};

class Renderer final {
  static Renderer *m_instance;
  static psyqo::Font<> m_systemFont;

  psyqo::GPU &m_gpu;
  uint32_t m_lastFrameCounter = 0;
  Camera *m_activeCamera;
  psyqo::Vec3 m_gteCameraPos = {0, 0, 0};

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

  // lighting, cached at start of scene
  Lighting* m_lighting = nullptr;

  Renderer(psyqo::GPU &gpuInstance) : m_gpu(gpuInstance){};
  ~Renderer(){};

  psyqo::Vec3 SetupCamera(const psyqo::Matrix33 &camRotationMatrix, const psyqo::Vec3 &negativeCamPos);
  psyqo::Vec3 TransformObjectToViewSpace(const psyqo::Vec3 &pos, const psyqo::Matrix33 &cameraRotationMatrix, const psyqo::Matrix33 &finalCameraMatrix);

  void RenderGameObjects(uint32_t deltaTime, const psyqo::Matrix33 &cameraRotationMatrix);
  void SubdivideTexturedQuad(psyqo::Fragments::SimpleFragment<psyqo::Prim::GouraudTexturedQuad>* texturedQuad, uint32_t zIndex, psyqo::OrderingTable<ORDERING_TABLE_SIZE>* ot, uint8_t maxDepth = 1);
  void SubdivideTexturedTri(psyqo::Fragments::SimpleFragment<psyqo::Prim::GouraudTexturedTriangle>* tri, uint32_t zIndex, psyqo::OrderingTable<ORDERING_TABLE_SIZE>* ot, uint8_t maxDepth = 1);

  void RenderBillboards(uint32_t deltaTime, const psyqo::Matrix33 &cameraRotationMatrix);
  void RenderParticles(uint32_t deltaTime, const psyqo::Matrix33 &cameraRotationMatrix);
  
  bool IsGameObjectVisible(const psyqo::Vec3& objectPos, const AABBCollision& collisionBox, const int32_t& boundingSphereRadius);

  psyqo::FixedPoint<> GetFogFactor(uint32_t z);

  void ApplyAmbientToColour(psyqo::Color* colA);
  void ApplyAmbientToColours(psyqo::Color* colA, psyqo::Color* colB, psyqo::Color* colC);
  void ApplyAmbientToColours(psyqo::Color* colA, psyqo::Color* colB, psyqo::Color* colC, psyqo::Color* colD);
  void ApplyFogToColour(psyqo::Color* col, psyqo::FixedPoint<> fogFactor);
  psyqo::Color ApplyFogToColourGTE(psyqo::Color input);
  
  void SetFarColour(void);
  void SetFogNearFar(psyqo::FixedPoint<> near, psyqo::FixedPoint<> far);
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
  void Clear(psyqo::Color clearColour = Lighting::instance().m_fogColour);
  void RenderLoadingScreen(uint16_t loadPercentage);
  void RenderSprite(const TimFile *tim, const psyqo::Rect rect, const psyqo::PrimPieces::UVCoords uv);
  void SetActiveCamera(Camera *camera);
  const Camera* ActiveCamera(void) const { return m_activeCamera; }
  const bool& IsSimpleFogEnabled(void) const { return Lighting::instance().m_isSimpleFogEnabled; }


  static Renderer &Instance() { return *m_instance; }
  psyqo::GPU &GPU() { return m_gpu; }
  psyqo::Font<> *SystemFont() { return &m_systemFont; }
};

#endif
