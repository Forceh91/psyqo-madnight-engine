/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "hall_scene.hh"

#include "common/hardware/pcsxhw.h"
#include "helpers/world_space.hh"
#include "math/vector.hh"
#include "madnight.hh"
#include "render/renderer.hh"

#include <EASTL/algorithm.h>
#include <EASTL/fixed_string.h>
#include <EASTL/fixed_vector.h>
#include <psyqo/xprintf.h>

using namespace psyqo::fixed_point_literals;
using namespace psyqo::trig_literals;

namespace {

constexpr const char* HALL_MESH = "MODELS/HALL.MB";
constexpr const char* HALL_TEXTURE = "TEXTURES/HALL.TIM";
constexpr const char* HERO_MESH = "MODELS/HERO.MB";
constexpr const char* HERO_TEXTURE = "TEXTURES/HERO.TIM";
constexpr const char* SAVE_NAME = "BESLUS-00000TECHDEMO";
constexpr uint32_t SAVE_MAGIC = 0x4f4d4454; // "TDMO"

// one lap of the camera path, in vsyncs
constexpr uint32_t LAP_LENGTH = 2400;

// SPU channels, and how often the creak plays (vsyncs)
constexpr uint8_t MUSIC_CHANNEL = 0;
constexpr uint8_t CREAK_CHANNEL = 1;
constexpr uint32_t CREAK_INTERVAL = 600;

// the hall runs along +Z from 0 to 24 units (48 m), see tools/make_assets.py
constexpr psyqo::FixedPoint<> HERO_START_Z = 2.0_ws;
constexpr int32_t HERO_SPEED = 24; // 1/16ths of a raw unit per vsync, about 1.4 m/s

// the bench walks the hero into the right wall from here, see CheckWall(). the brick face
// is at x = 1.5, and the wall's box reaches 32 raw units (the exporter's minimum half
// thickness) in front of it
constexpr psyqo::Vec3 WALL_TEST_START = {0, 0, 5.0_ws};
constexpr int32_t WALL_TEST_FACE = (1.25_ws).value;

constexpr psyqo::Color FOG_COLOUR = {.r = 92, .g = 96, .b = 100};
constexpr psyqo::Color AMBIENT = {.r = 100, .g = 96, .b = 92};

} // namespace

void HallScene::start(StartReason reason) {
	Lighting::instance().EnableAdvancedFog();
	Lighting::instance().SetAmbientColour(AMBIENT);
	Renderer::Instance().SetFogColour(FOG_COLOUR);
	Renderer::Instance().StartScene();
	Renderer::Instance().SetActiveCamera(&m_camera);

	g_madnightEngine.m_input.setOnEvent([this](auto event) {
		if (event.type == psyqo::AdvancedPad::Event::ButtonReleased &&
			event.button == psyqo::AdvancedPad::Button::Start) {
			m_menu.Enable();
		}
	});

	if (reason == StartReason::Create) {
		// a fixed seed, so every run spawns the same particles
		g_madnightEngine.m_rand.seed(0x7ec4de30);
		CreateWorld();
		CreateMenu();
		StartSound();
		LoadStats();
	}
}

void HallScene::teardown(TearDownReason reason) {
	g_madnightEngine.m_input.setOnEvent(nullptr);
	g_madnightEngine.m_soundManager.SilenceChannels((1 << MUSIC_CHANNEL) | (1 << CREAK_CHANNEL));
}

void HallScene::StartSound(void) {
	auto& sound = g_madnightEngine.m_soundManager;
	VagEntry* music = sound.IsVAGLoaded("SFX/FCNTNA.VAG");
	VagEntry* creak = sound.IsVAGLoaded("SFX/AMBIENT_CREAK.VAG");
	printf("TECHDEMO vag music=%s creak=%s\n", music ? "ok" : "missing", creak ? "ok" : "missing");
	// FCNTNA.VAG carries its own loop flags, so the SPU repeats it without help
	if (music) {
		sound.PlayVAGFile(music, MUSIC_CHANNEL, sound.CreatePlaybackConfig(music, 0x1800));
	}
}

void HallScene::UpdateSound(uint32_t deltaTime) {
	m_creakTimer += deltaTime;
	if (m_creakTimer < CREAK_INTERVAL) {
		return;
	}
	m_creakTimer -= CREAK_INTERVAL;
	auto& sound = g_madnightEngine.m_soundManager;
	VagEntry* creak = sound.IsVAGLoaded("SFX/AMBIENT_CREAK.VAG");
	if (creak) {
		sound.PlayVAGFile(creak, CREAK_CHANNEL, sound.CreatePlaybackConfig(creak, 0x2800), true);
	}
}

void HallScene::CreateWorld(void) {
	auto& objects = g_madnightEngine.m_gameObjectManager;

	m_hall = objects.CreateGameObject("hall", {0, 0, 0}, {0, 0, 0}, GameObjectTag::ENVIRONMENT);
	if (m_hall) {
		m_hall->SetMesh(HALL_MESH);
		m_hall->SetTexture(HALL_TEXTURE);
	}

	m_heroZ = HERO_START_Z.value * 16;
	m_hero = objects.CreateGameObject("hero", {0, 0, HERO_START_Z}, {0, 0, 0}, GameObjectTag::INTERACTABLE);
	if (m_hero) {
		m_hero->SetMesh(HERO_MESH);
		m_hero->SetTexture(HERO_TEXTURE);
		m_hero->SetRenderFlag(RF_QUICK_ADVANCED_FOG);

		MeshBin* mesh = nullptr;
		g_madnightEngine.m_meshManager.GetMeshFromName(HERO_MESH, &mesh);
		auto* walk = g_madnightEngine.m_animationManager.GetAnimationFromName("walk");
		if (mesh && mesh->skeleton && walk) {
			g_madnightEngine.m_skeletonController.SetAnimation(mesh->skeleton, walk);
		}
	}

	// the hero turns round when it walks into one of these
	m_nearTrigger = objects.CreateGameObject("near_end", {0, -0.5_ws, 1.0_ws}, {0, 0, 0}, GameObjectTag::INTERACTABLE);
	if (m_nearTrigger) {
		m_nearTrigger->SetAsTrigger({3.0_ws, 1.0_ws, 0.5_ws});
	}
	m_farTrigger = objects.CreateGameObject("far_end", {0, -0.5_ws, 21.0_ws}, {0, 0, 0}, GameObjectTag::INTERACTABLE);
	if (m_farTrigger) {
		m_farTrigger->SetAsTrigger({3.0_ws, 1.0_ws, 0.5_ws});
	}

	auto& emitters = g_madnightEngine.m_particleEmitterManager;

	// embers rising off the floor by the left wall
	m_embers = emitters.CreateParticleEmitter("embers", {-1.0_ws, -0.1_ws, 6.0_ws}, 0.12_ws, 12, 2);
	if (m_embers) {
		m_embers->SetParticles2D(true);
		m_embers->SetParticleSize({0.16_ws, 0.16_ws}, {0.05_ws, 0.05_ws});
		m_embers->SetParticleColour({255, 170, 60}, {120, 30, 16});
		m_embers->SetParticleVelocity({0, -0.35_ws, 0}, {0.05_ws, -0.1_ws, 0});
		m_embers->Start();
	}

	// warm dust drifting down through the middle of the hall, where the hero walks
	m_dust = emitters.CreateParticleEmitter("dust", {0, -1.4_ws, 9.0_ws}, 1.0_ws, 20, 3);
	if (m_dust) {
		m_dust->SetParticles2D(true);
		m_dust->SetParticleSize({0.1_ws, 0.1_ws});
		m_dust->SetParticleColour({230, 190, 120}, {140, 100, 60});
		m_dust->SetParticleVelocity({0, 0.55_ws, 0});
		m_dust->SetSpawnInsideRadius(true);
		m_dust->SetParticleVelocitySpread({0.15_ws, 0.1_ws, 0.15_ws});
		m_dust->Start();
	}

	m_camera.SetFixed();

	m_costText = m_hud.AddTextHUDElement(TextHUDElement("cost", {.pos = {8, 200}, .size = {300, 12}}));
	m_lapText = m_hud.AddTextHUDElement(TextHUDElement("lap", {.pos = {8, 212}, .size = {300, 12}}));
	m_cardText = m_hud.AddTextHUDElement(TextHUDElement("card", {.pos = {8, 224}, .size = {300, 12}}));
	m_lapText->SetDisplayText("first lap");
}

void HallScene::CreateMenu(void) {
	m_menu.AddTextHUDElement(TextHUDElement("title", {.pos = {0, 0}, .size = {140, 12}}))->SetDisplayText("PAUSED");

	m_fogItem = m_menu.AddMenuItem("fog", "Hero fog: quick", {.pos = {0, 20}, .size = {140, 12}});
	m_fogItem->SetOnConfirm([this]() {
		m_quickFog = !m_quickFog;
		if (m_quickFog) {
			m_hero->SetRenderFlag(RF_QUICK_ADVANCED_FOG);
		} else {
			m_hero->ClearRenderFlag(RF_QUICK_ADVANCED_FOG);
		}
		m_fogItem->SetText(m_quickFog ? "Hero fog: quick" : "Hero fog: per face");
	});

	m_saveItem = m_menu.AddMenuItem("save", "Save last lap", {.pos = {0, 36}, .size = {140, 12}});
	m_saveItem->SetOnConfirm([this]() { m_saveRequested = true; });

	m_menu.AddMenuItem("resume", "Resume", {.pos = {0, 52}, .size = {140, 12}})->SetOnConfirm([this]() {
		m_menu.Disable();
	});

	// the card is written from the menu's frame, not from inside the button callback
	m_menu.SetOnFrame([this](uint32_t) { SaveStats(); });

	// a plain two-colour icon for the BIOS memory card screen
	m_icon.frameCount = 1;
	m_icon.clut[1] = 0x3d08; // dim blue
	m_icon.clut[2] = 0x7fff;
	for (int i = 0; i < 128; i++) {
		int y = i / 8, x = (i % 8) * 2;
		uint8_t lo = (y > 3 && y < 12 && x > 3 && x < 12) ? 2 : 1;
		uint8_t hi = (y > 3 && y < 12 && x + 1 > 3 && x + 1 < 12) ? 2 : 1;
		m_icon.pixels[0][i] = lo | (hi << 4);
	}
}

void HallScene::UpdateHero(uint32_t deltaTime) {
	if (!m_hero) {
		return;
	}

	m_heroZ += HERO_SPEED * m_heroDirection * int32_t(deltaTime);
	auto pos = m_hero->pos();
	pos.z.value = m_heroZ / 16;
	m_hero->SetPosition(pos);
	if (auto hits = CollideHero()) {
		m_lapHits += hits;
		m_heroZ = m_hero->pos().z.value * 16;
	}

	// turn round at whichever end it is walking towards
	auto* end = m_heroDirection > 0 ? m_farTrigger : m_nearTrigger;
	CollisionTest result;
	if (end && g_madnightEngine.m_collisionHelper.IsSATCollision(m_hero->obb(), end->obb(), &result)) {
		m_heroDirection = -m_heroDirection;
		// the mesh faces +Z
		m_hero->SetRotation(0, m_heroDirection > 0 ? 0.0_pi : 1.0_pi, 0);
	}
}

// pushes the hero out of every hall wall it overlaps, which leaves it sliding along them.
// returns how many it hit
uint32_t HallScene::CollideHero(void) {
	auto& colbins = g_madnightEngine.m_colbinManager;
	auto* colbin = colbins.Colbin();
	if (!m_hero || !colbin->gridCells) {
		return 0;
	}

	// the cells under the hero's footprint on X and Z, whichever way it faces
	const auto& grid = colbin->gridHeader;
	const OBB& box = m_hero->obb();
	int32_t reach[2] = {};
	for (int i = 0; i < 3; i++) {
		auto half = i == 0 ? box.halfExtents.x : i == 1 ? box.halfExtents.y : box.halfExtents.z;
		reach[0] += (box.axes[i].x.abs() * half).value;
		reach[1] += (box.axes[i].z.abs() * half).value;
	}
	// signed, so a hero outside the grid clamps to its edge
	auto cell = [&grid](int32_t offset, int32_t count) {
		int32_t index = offset < 0 ? 0 : offset / int32_t(grid.cellSize);
		return index < count ? index : count - 1;
	};
	int32_t x0 = cell(box.center.x.value - reach[0] - grid.originX, grid.gridWidth);
	int32_t x1 = cell(box.center.x.value + reach[0] - grid.originX, grid.gridWidth);
	int32_t z0 = cell(box.center.z.value - reach[1] - grid.originZ, grid.gridHeight);
	int32_t z1 = cell(box.center.z.value + reach[1] - grid.originZ, grid.gridHeight);

	// a wall that crosses cells is listed in each of them
	eastl::fixed_vector<uint16_t, 16> nearby;
	for (int32_t x = x0; x <= x1; x++) {
		for (int32_t z = z0; z <= z1; z++) {
			const auto& gridCell = colbin->gridCells[x * grid.gridHeight + z];
			for (int i = 0; i < gridCell.count; i++) {
				if (eastl::find(nearby.begin(), nearby.end(), gridCell.indices[i]) == nearby.end()) {
					nearby.push_back(gridCell.indices[i]);
				}
			}
		}
	}

	uint32_t hits = 0;
	auto walls = colbins.walls();
	for (auto index : nearby) {
		CollisionTest result;
		if (!g_madnightEngine.m_collisionHelper.IsSATCollision(m_hero->obb(), walls[index], &result)) {
			continue;
		}
		// out along the contact normal only, so the rest of the move carries on along the wall
		m_hero->SetPosition(m_hero->pos() + result.mtv);
		hits++;
	}
	return hits;
}

// walks the hero diagonally into the right wall, then along it, and checks that the wall
// stopped it at its face while it kept sliding down the hall
void HallScene::CheckWall(void) {
	if (!m_hero) {
		return;
	}
	auto saved = m_hero->pos();
	m_hero->SetPosition(WALL_TEST_START);

	constexpr int32_t intoWall = 100, alongWall = 40;
	uint32_t hits = 0;
	for (int32_t i = 0; i < intoWall + alongWall; i++) {
		auto pos = m_hero->pos();
		pos.x.value += i < intoWall ? 2 : 0;
		pos.z.value += 1;
		m_hero->SetPosition(pos);
		hits += CollideHero();
	}

	const OBB& box = m_hero->obb();
	int32_t face = box.center.x.value + box.halfExtents.x.value;
	int32_t z = m_hero->pos().z.value;
	int32_t slid = z - WALL_TEST_START.z.value;
	bool stopped = face <= WALL_TEST_FACE && face >= WALL_TEST_FACE - 1;
	bool ok = stopped && slid == intoWall + alongWall && hits > 0 && m_lapHits == 0;
	printf("TECHDEMO wall=%s face=%d expected=%d slid=%d hits=%u lap_hits=%u\n", ok ? "ok" : "FAIL", face,
		   WALL_TEST_FACE, slid, hits, m_lapHits);

	m_hero->SetPosition(saved);
}

void HallScene::UpdateCamera(void) {
	// first half: stand at the near end and watch the hero walk off into the fog and turn.
	// second half: dolly down the hall towards it, crossing from one side to the other
	static constexpr psyqo::Vec3 standPos = {0.9_ws, -0.8_ws, 0.6_ws};
	static constexpr psyqo::Vec3 dollyEnd = {-0.9_ws, -0.9_ws, 12.0_ws};
	constexpr uint32_t half = LAP_LENGTH / 2;

	psyqo::Vec3 camPos = standPos;
	if (m_lapTime >= half) {
		camPos = Lerp(standPos, dollyEnd, psyqo::FixedPoint<>(int32_t(((m_lapTime - half) << 12) / half), psyqo::FixedPoint<>::RAW));
	}
	m_camera.SetPosition(camPos);

	psyqo::Vec3 target = m_hero ? m_hero->pos() : psyqo::Vec3{0, 0, 4.0_ws};
	target.y = -0.6_ws;

	m_camera.LookAt(&target);
}

void HallScene::EndLap(void) {
	m_lap.magic = SAVE_MAGIC;
	m_lap.laps = m_lastLap.laps + 1;
	m_lastLap = m_lap;

	uint32_t avg = m_lap.frames ? m_lap.vsyncs * 100 / m_lap.frames : 0;
	printf("TECHDEMO lap=%u frames=%u vsyncs=%u avg_x100=%u worst=%u\n", m_lap.laps, m_lap.frames, m_lap.vsyncs, avg,
		   m_lap.worstFrame);

	eastl::fixed_string<char, 48> text;
	text.sprintf("lap %u: %u.%02u vsyncs/frame, worst %u", m_lap.laps, avg / 100, avg % 100, m_lap.worstFrame);
	m_lapText->SetDisplayText(text);

	m_lap = {};
	m_lapTime = 0;
	m_heroZ = HERO_START_Z.value * 16;
	m_heroDirection = 1;
	if (m_hero) {
		m_hero->SetRotation(0, 0, 0);
	}

#ifdef TECHDEMO_BENCH
	CheckWall();
	m_saveRequested = true;
#endif
	m_lapHits = 0;
}

void HallScene::LoadStats(void) {
	auto& card = g_madnightEngine.m_memoryCardHelper;
	if (!(card.GetCardsInPorts() & CardPort::Port0)) {
		printf("TECHDEMO load=none laps=0\n");
		return;
	}

	m_loaded = {};
	card.ReadFile(psyqo::MemoryCard::Port::Port0, SAVE_NAME, &m_loaded, sizeof(m_loaded), &m_loadedLen,
				  [this](auto error) {
					  bool found = error == psyqo::MemoryCard::Error::OK && m_loadedLen >= sizeof(m_loaded) &&
								   m_loaded.magic == SAVE_MAGIC;
					  if (!found) {
						  printf("TECHDEMO load=none laps=0\n");
						  return;
					  }
					  printf("TECHDEMO load=found laps=%u\n", m_loaded.laps);
					  uint32_t avg = m_loaded.frames ? m_loaded.vsyncs * 100 / m_loaded.frames : 0;
					  eastl::fixed_string<char, 48> text;
					  text.sprintf("card: lap %u, %u.%02u vsyncs/frame, worst %u", m_loaded.laps, avg / 100, avg % 100,
								   m_loaded.worstFrame);
					  m_cardText->SetDisplayText(text);
				  });
}

void HallScene::SaveStats(void) {
	if (!m_saveRequested || m_saveInFlight) {
		return;
	}
	m_saveRequested = false;

	auto& card = g_madnightEngine.m_memoryCardHelper;
	if (!(card.GetCardsInPorts() & CardPort::Port0)) {
		m_cardText->SetDisplayText("no memory card in slot 1");
		printf("TECHDEMO card=none\n");
#ifdef TECHDEMO_BENCH
		pcsx_exit(1);
#endif
		return;
	}

	m_saveInFlight = true;
	m_saveBuffer = m_lastLap;
	m_cardText->SetDisplayText("saving...");
	card.WriteFile(psyqo::MemoryCard::Port::Port0, SAVE_NAME, "MADNIGHT TECH DEMO", m_icon, &m_saveBuffer,
				   sizeof(m_saveBuffer), [this, &card](auto error) {
					  if (error != psyqo::MemoryCard::Error::OK) {
						  m_saveInFlight = false;
						  m_cardText->SetDisplayText("save failed");
						  printf("TECHDEMO card=write-failed error=%d\n", int(error));
#ifdef TECHDEMO_BENCH
						  pcsx_exit(1);
#endif
						  return;
					  }
					  // read it back, so the save is checked and not just sent
					  m_readBack = {};
					  card.ReadFile(psyqo::MemoryCard::Port::Port0, SAVE_NAME, &m_readBack, sizeof(m_readBack),
									&m_readBackLen, [this](auto error) {
									   m_saveInFlight = false;
									   bool match = error == psyqo::MemoryCard::Error::OK &&
													m_readBackLen >= sizeof(m_readBack) &&
													__builtin_memcmp(&m_readBack, &m_saveBuffer, sizeof(m_readBack)) == 0;
									   m_cardText->SetDisplayText(match ? "saved to slot 1" : "save did not read back");
									   printf("TECHDEMO card=%s laps=%u\n", match ? "ok" : "mismatch", m_readBack.laps);
#ifdef TECHDEMO_BENCH
									   pcsx_exit(match ? 0 : 1);
#endif
								   });
				  });
}

void HallScene::frame() {
	auto& renderer = Renderer::Instance();
	uint32_t deltaTime = renderer.Process();
	if (deltaTime == 0) {
		return;
	}

	m_lapTime += deltaTime;
	m_lap.frames++;
	m_lap.vsyncs += deltaTime;
	if (deltaTime > m_lap.worstFrame) {
		m_lap.worstFrame = deltaTime;
	}

	UpdateHero(deltaTime);
	UpdateCamera();
	UpdateSound(deltaTime);
	m_camera.Process(deltaTime);
	if (m_embers) {
		m_embers->Process(deltaTime);
	}
	if (m_dust) {
		m_dust->Process(deltaTime);
	}

	renderer.Render(deltaTime);

	eastl::fixed_string<char, 48> text;
	text.sprintf("this frame: %u vsync%s", deltaTime, deltaTime == 1 ? "" : "s");
	m_costText->SetDisplayText(text);
	m_hud.Render();
	g_madnightEngine.m_perfMonitor.Render(deltaTime);

	SaveStats();

	if (m_lapTime >= LAP_LENGTH) {
		EndLap();
	}
}
