/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "../../ui/hud/gameplay_hud.hh"

class PerfMonitor final {
  public:
	// this should be called last in your render loop
	void Render(uint32_t deltaTime);
	void SetRenderedGameObjects(uint8_t renderedObjects, uint8_t totalObjects) {
		m_renderedGameObjects = renderedObjects;
		m_totalGameObjects = totalObjects;
	}

  private:
	void Init(void);

	bool m_hasInitialized = false;
	GameplayHUD m_perfMontiorHUD = GameplayHUD("Perf Monitor", {.pos = {5, 10}, .size = {100, 100}});
	TextHUDElement* m_heapSizeText = nullptr;
	TextHUDElement* m_fpsText = nullptr;

	uint32_t m_deltaTimeAccum = 0;
	uint32_t m_frameCount = 0;
	uint8_t m_renderedGameObjects = 0;
	uint8_t m_totalGameObjects = 0;
};
