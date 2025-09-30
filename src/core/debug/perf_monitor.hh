#ifndef _PERF_MONITOR_H
#define _PERF_MONITOR_H

#include "../../ui/hud/gameplay_hud.hh"

class PerfMonitor final {
public:
  // this should be called last in your render loop
  static void Render(uint32_t deltaTime);

private:
  static bool m_hasInitialized;
  static GameplayHUD m_perfMontiorHUD;
  static TextHUDElement *m_heapSizeText;
  static TextHUDElement *m_fpsText;

  static void Init(void);
};

#endif
