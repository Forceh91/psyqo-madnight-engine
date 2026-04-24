#ifndef _PERF_MONITOR_H
#define _PERF_MONITOR_H

#include "../../ui/hud/gameplay_hud.hh"

class PerfMonitor final {
public:
  // this should be called last in your render loop
  static void Render(uint32_t deltaTime);
  static void SetRenderedGameObjects(uint8_t renderedObjects, uint8_t totalObjects) { m_renderedGameObjects = renderedObjects; m_totalGameObjects = totalObjects; }

private:
  static bool m_hasInitialized;
  static GameplayHUD m_perfMontiorHUD;
  static TextHUDElement *m_heapSizeText;
  static TextHUDElement *m_fpsText;

  static void Init(void);

  static uint32_t m_deltaTimeAccum;
  static uint32_t m_frameCount;
  static uint8_t m_renderedGameObjects;
  static uint8_t m_totalGameObjects;
};

#endif
