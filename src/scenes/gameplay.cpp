/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "gameplay.hh"
#include "../core/collision.hh"
#include "../core/debug//perf_monitor.hh"
#include "../core/debug/debug_menu.hh"
#include "../core/object/gameobject_manager.hh"
#include "../core/raycast.hh"
#include "../madnight.hh"
#include "../render/colour.hh"
#include "../render/renderer.hh"
#include "../sound/sound_manager.hh"
#include "psyqo/alloc.h"
#include "psyqo/xprintf.h"

void GameplayScene::start(StartReason reason) {
	Renderer::Instance().StartScene();

	g_madnightEngine.m_input.setOnEvent([&](auto event) {
		if (event.type != psyqo::AdvancedPad::Event::ButtonReleased) {
			return;
		}
		if (event.button == psyqo::AdvancedPad::Button::Start) {
			m_menu.Enable();
		}
	});

	// the below only needs to happen if this was a freshly created scene
	if (reason != StartReason::Create) {
		return;
	}

	m_heapSizeText = m_debugHUD.AddTextHUDElement(TextHUDElement("HEAP", {.pos = {5, 0}, .size = {100, 100}}));
	m_fpsText = m_debugHUD.AddTextHUDElement(TextHUDElement("FPS", {.pos = {5, 15}, .size = {100, 100}}));

	if (m_camera == nullptr) {
		m_camera = new Camera();
	}
}

void GameplayScene::teardown(TearDownReason reason) { g_madnightEngine.m_input.setOnEvent(nullptr); }

void GameplayScene::frame() {
	auto& renderInstance = Renderer::Instance();
	auto& gpu = Renderer::Instance().GPU();
	uint32_t deltaTime = renderInstance.Process();
	if (deltaTime == 0) {
		return;
	}

	// process camera inputs
	m_camera->Process(deltaTime);

	// process debug menu
	g_madnightEngine.m_debugMenu.Process();

	// raycast
	const auto& raycastDistance = g_madnightEngine.m_debugMenu.RaycastDistance();
	Ray ray = {
		.origin = m_camera->pos(), .direction = m_camera->forwardVector(), .maxDistance = raycastDistance * ONE_METRE};
	RayHit hit = {0};

	// bool didHit = Raycast::RaycastScene(ray, GameObjectTag::ENVIRONMENT, &hit);
	// printf("did hit=%d\n", didHit);

	// collision detection test...
	// auto objects = g_madnightEngine.m_gameObjectManager.GetGameObjectsWithTag(GameObjectTag::ENVIRONMENT);
	// bool collision = Collision::IsSATCollision(objects[0]->obb(), objects[1]->obb());
	// printf("collision=%d\n", collision);

	// the central point for rendering gameobjects etc
	renderInstance.Render();

	if (g_madnightEngine.m_debugMenu.IsEnabled()) {
		return;
	}

	if (g_madnightEngine.m_debugMenu.DisplayDebugHUD()) {
		g_madnightEngine.m_perfMonitor.Render(deltaTime);
	}
}
