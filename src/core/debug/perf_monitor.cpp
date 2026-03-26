#include "perf_monitor.hh"
#include "../../render/renderer.hh"
#include "psyqo/alloc.h"

GameplayHUD PerfMonitor::m_perfMontiorHUD = GameplayHUD("Perf Monitor", {.pos = {5, 10}, .size = {100, 100}});
TextHUDElement *PerfMonitor::m_heapSizeText = nullptr;
TextHUDElement *PerfMonitor::m_fpsText = nullptr;
bool PerfMonitor::m_hasInitialized = false;
uint32_t PerfMonitor::m_deltaTimeAccum;
uint32_t PerfMonitor::m_frameCount;

void PerfMonitor::Init(void) {
  m_heapSizeText = m_perfMontiorHUD.AddTextHUDElement(TextHUDElement("HEAP", {.pos = {5, 0}, .size = {100, 100}}));
  m_heapSizeText->SetFont(Renderer::Instance().SystemFont());

  m_fpsText = m_perfMontiorHUD.AddTextHUDElement(TextHUDElement("FPS", {.pos = {5, 15}, .size = {100, 100}}));
  m_fpsText->SetFont(Renderer::Instance().SystemFont());
  m_hasInitialized = true;
}

void PerfMonitor::Render(uint32_t deltaTime) {
  if (!m_hasInitialized)
    Init();

  char heapSize[GAMEPLAY_HUD_ELEMENT_MAX_STR_LEN];
  snprintf(heapSize, GAMEPLAY_HUD_ELEMENT_MAX_STR_LEN, "Heap Used: %d",
           (int)((uint8_t *)psyqo_heap_end() - (uint8_t *)psyqo_heap_start()));
  m_heapSizeText->SetDisplayText(heapSize);

  m_deltaTimeAccum += deltaTime;
  m_frameCount++;
  if (m_frameCount >= 30) {
      auto avgDelta = 1.0_fp * m_deltaTimeAccum / m_frameCount;
      auto fps = 1.0_fp * Renderer::Instance().GPU().getRefreshRate() / avgDelta;
      char fpsStr[GAMEPLAY_HUD_ELEMENT_MAX_STR_LEN];
      snprintf(fpsStr, GAMEPLAY_HUD_ELEMENT_MAX_STR_LEN, "FPS: %d", fps.integer());
      m_fpsText->SetDisplayText(fpsStr);
      m_deltaTimeAccum = 0;
      m_frameCount = 0;
  }

  m_perfMontiorHUD.Render();
}
