/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <psyqo/fragments.hh>
#include <psyqo/primitives/quads.hh>
#include <psyqo/primitives/rectangles.hh>
#include <psyqo/scene.hh>

// a custom loading screen: pass it to HardLoadingScreen/SoftLoadingScreen in place of the engine's
// LoadingScene. with PCDRV the load is over in a few frames, from a disc it stays up longer
class TechDemoLoadingScene final : public psyqo::Scene {
	void start(StartReason reason) override;
	void frame(void) override;

	// one set per frame buffer: the GPU may still be reading last frame's while this one is built
	psyqo::Fragments::SimpleFragment<psyqo::Prim::Rectangle> m_track[2];
	psyqo::Fragments::SimpleFragment<psyqo::Prim::GouraudQuad> m_bar[2];
	static constexpr unsigned EMBER_COUNT = 10;
	psyqo::Fragments::SimpleFragment<psyqo::Prim::Rectangle> m_embers[2][EMBER_COUNT];

	uint32_t m_startTime = 0;
};
