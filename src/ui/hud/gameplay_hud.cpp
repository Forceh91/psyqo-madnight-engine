/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "gameplay_hud.hh"

void GameplayHUD::Render(void) {
	// make sure its enabled
	if (!m_isEnabled) {
		return;
	}

	// for each text hud element, draw to the screen...
	for (auto& element : m_textHUDElements) {
		element.Render(m_rect);
	}

	// for each sprite hud element, draw to the screen
	for (auto& element : m_spriteHUDElements) {
		element.Render(m_rect);
	}
}

void GameplayHUD::Destroy(void) {
	m_isEnabled = false;

	m_textHUDElements.clear(true);
	m_spriteHUDElements.clear(true);
}
