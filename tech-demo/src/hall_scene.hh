/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "core/object/gameobject.hh"
#include "core/particles/particle_emitter.hh"
#include "render/camera.hh"
#include "ui/hud/gameplay_hud.hh"
#include "ui/menu/menu.hh"

#include <psyqo/memory-card-filesystem.hh>
#include <psyqo/scene.hh>

/*
 * one scene that uses as much of the engine as it can at once: a dim hall under
 * advanced fog, a skinned character walking through it with quick advanced fog,
 * two particle emitters, trigger volumes, a pause menu and a memory card save.
 *
 * the camera runs on a fixed path keyed to elapsed vsyncs, so every lap draws the
 * same frames, and the HUD shows how many vsyncs each frame took. built with
 * TECHDEMO_BENCH defined it runs one lap, prints the numbers, saves them to the
 * memory card, reads them back and exits the emulator.
 */
class HallScene final : public psyqo::Scene {
  public:
	// what goes on the memory card, and what the bench prints
	struct LapStats {
		uint32_t magic;
		uint32_t laps;
		uint32_t frames;
		uint32_t vsyncs;
		uint32_t worstFrame;
	};

  private:
	void start(StartReason reason) override;
	void teardown(TearDownReason reason) override;
	void frame() override;

	void CreateWorld(void);
	void CreateMenu(void);
	void UpdateHero(uint32_t deltaTime);
	void UpdateCamera(void);
	void EndLap(void);
	void SaveStats(void);

	Camera m_camera;
	GameObject* m_hall = nullptr;
	GameObject* m_hero = nullptr;
	GameObject* m_nearTrigger = nullptr;
	GameObject* m_farTrigger = nullptr;
	ParticleEmitter* m_embers = nullptr;
	ParticleEmitter* m_dust = nullptr;

	// hero position along the hall in 1/16ths of a raw unit, so slow speeds don't round to 0
	int32_t m_heroZ = 0;
	int8_t m_heroDirection = 1;

	uint32_t m_lapTime = 0; // vsyncs into the current lap
	LapStats m_lap = {};	// the lap being run
	LapStats m_lastLap = {};
	bool m_quickFog = true;

	GameplayHUD m_hud = GameplayHUD("Tech demo HUD", {.pos = {0, 0}, .size = {320, 240}});
	TextHUDElement* m_costText = nullptr;
	TextHUDElement* m_lapText = nullptr;
	TextHUDElement* m_cardText = nullptr;

	Menu m_menu = Menu("Pause", {.pos = {100, 80}, .size = {140, 80}});
	MenuItem* m_fogItem = nullptr;
	MenuItem* m_saveItem = nullptr;

	// the save runs from frame(), never from a button callback
	bool m_saveRequested = false;
	bool m_saveInFlight = false;
	LapStats m_saveBuffer = {};
	LapStats m_readBack = {};
	uint32_t m_readBackLen = 0;
	psyqo::MemoryCardFileSystem::Icon m_icon = {};
};
