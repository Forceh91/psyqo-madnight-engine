#include "perf_monitor.hh"
#include "../../render/renderer.hh"
#include "psyqo/alloc.h"

GameplayHUD PerfMonitor::m_perfMontiorHUD = GameplayHUD("Perf Monitor", {.pos = {5, 10}, .size = {100, 100}});
TextHUDElement *PerfMonitor::m_heapSizeText = nullptr;
TextHUDElement *PerfMonitor::m_fpsText = nullptr;
bool PerfMonitor::m_hasInitialized = false;

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

  char fps[GAMEPLAY_HUD_ELEMENT_MAX_STR_LEN];
  snprintf(fps, GAMEPLAY_HUD_ELEMENT_MAX_STR_LEN, "FPS: %d", Renderer::Instance().GPU().getRefreshRate() / deltaTime);
  m_fpsText->SetDisplayText(fps);

  m_perfMontiorHUD.Render();
}
