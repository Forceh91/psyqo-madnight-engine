/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "hall_scene.hh"

#include "common/hardware/pcsxhw.h"
#include "helpers/world_space.hh"
#include "madnight.hh"
#include "render/renderer.hh"

#include <EASTL/fixed_string.h>
#include <psyqo/atan2.hh>
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

// the hall runs along +Z from 0 to 24 units (48 m), see tools/make_assets.py
constexpr int32_t HERO_START_Z = 2;
constexpr int32_t HERO_SPEED = 24; // 1/16ths of a raw unit per vsync, about 1.4 m/s

constexpr psyqo::Color FOG_COLOUR = {.r = 92, .g = 96, .b = 100};
constexpr psyqo::Color AMBIENT = {.r = 100, .g = 96, .b = 92};

psyqo::Vec3 Lerp(const psyqo::Vec3& a, const psyqo::Vec3& b, uint32_t t, uint32_t length) {
	psyqo::Vec3 out;
	out.x.value = a.x.value + (b.x.value - a.x.value) * int32_t(t) / int32_t(length);
	out.y.value = a.y.value + (b.y.value - a.y.value) * int32_t(t) / int32_t(length);
	out.z.value = a.z.value + (b.z.value - a.z.value) * int32_t(t) / int32_t(length);
	return out;
}

uint32_t ISqrt(uint32_t v) {
	uint32_t r = 0;
	for (uint32_t bit = 1u << 30; bit; bit >>= 2) {
		if (v >= r + bit) {
			v -= r + bit;
			r = (r >> 1) + bit;
		} else {
			r >>= 1;
		}
	}
	return r;
}

} // namespace

void HallScene::start(StartReason reason) {
	// the fog mode is read once by StartScene, so set it first
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
	}
}

void HallScene::teardown(TearDownReason reason) { g_madnightEngine.m_input.setOnEvent(nullptr); }

void HallScene::CreateWorld(void) {
	auto& objects = g_madnightEngine.m_gameObjectManager;

	m_hall = objects.CreateGameObject("hall", {0, 0, 0}, {0, 0, 0}, GameObjectTag::ENVIRONMENT);
	if (m_hall) {
		m_hall->SetQuadType(GameObjectQuadType::GouraudTextureQuad);
		m_hall->SetMesh(HALL_MESH);
		m_hall->SetTexture(HALL_TEXTURE);
	}

	m_heroZ = HERO_START_Z * 128 * 16;
	m_hero = objects.CreateGameObject("hero", {0, 0, psyqo::FixedPoint<>(HERO_START_Z * 128, psyqo::FixedPoint<>::RAW)},
									  {0, 0, 0}, GameObjectTag::INTERACTABLE);
	if (m_hero) {
		// a reused pool slot keeps the old object's flags (#93)
		m_hero->ClearFlags();
		m_hero->ClearRenderFlags();
		m_hero->SetQuadType(GameObjectQuadType::GouraudTextureQuad);
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

	// embers off a brazier by the left wall
	m_embers = emitters.CreateParticleEmitter("embers", {-1.0_ws, -0.1_ws, 6.0_ws}, 0.12_ws, 12, 2);
	if (m_embers) {
		m_embers->SetRotation({0, 0, 0}); // builds the rotation matrix a pooled emitter is missing (#94)
		m_embers->SetParticleSize({0.06_ws, 0.06_ws}, {0.02_ws, 0.02_ws});
		m_embers->SetParticleColour({255, 170, 60}, {90, 24, 16});
		m_embers->SetParticleVelocity({0, -0.35_ws, 0}, {0.05_ws, -0.1_ws, 0});
		m_embers->Start();
	}

	// dust sinking through the far end of the hall, out in the fog
	m_dust = emitters.CreateParticleEmitter("dust", {0, -1.4_ws, 12.0_ws}, 1.2_ws, 8, 3);
	if (m_dust) {
		m_dust->SetRotation({0, 0, 0});
		m_dust->SetParticleSize({0.03_ws, 0.03_ws});
		m_dust->SetParticleColour({150, 150, 140}, {80, 80, 76});
		m_dust->SetParticleVelocity({0, 0.12_ws, 0});
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

	// turn round at whichever end it is walking towards
	auto* end = m_heroDirection > 0 ? m_farTrigger : m_nearTrigger;
	CollisionTest result;
	if (end && g_madnightEngine.m_collisionHelper.IsSATCollision(m_hero->obb(), end->obb(), &result)) {
		m_heroDirection = -m_heroDirection;
		// the mesh faces +Z. a skinned mesh ignores its rotation until #110 is fixed
		m_hero->SetRotation(0, m_heroDirection > 0 ? 0.0_pi : 1.0_pi, 0);
	}
}

void HallScene::UpdateCamera(void) {
	// first half: stand at the near end and watch the hero walk off into the fog and turn.
	// second half: dolly down the hall towards it, crossing from one side to the other
	static constexpr psyqo::Vec3 standPos = {0.9_ws, -0.8_ws, 0.6_ws};
	static constexpr psyqo::Vec3 dollyEnd = {-0.9_ws, -0.9_ws, 12.0_ws};
	constexpr uint32_t half = LAP_LENGTH / 2;

	psyqo::Vec3 camPos = standPos;
	if (m_lapTime >= half) {
		camPos = Lerp(standPos, dollyEnd, m_lapTime - half, half);
	}
	// the renderer applies the camera position twice (#106), so hand it half of where it should be
	psyqo::Vec3 halfPos;
	halfPos.x.value = camPos.x.value / 2;
	halfPos.y.value = camPos.y.value / 2;
	halfPos.z.value = camPos.z.value / 2;
	m_camera.SetPosition(halfPos);

	psyqo::Vec3 target = m_hero ? m_hero->pos() : psyqo::Vec3{0, 0, 4.0_ws};
	target.y = -0.6_ws;

	// aim from the real position with angles; Camera::LookAt would aim from the halved one.
	// a negative pitch looks down, and +Y is down
	int32_t dx = (target.x - camPos.x).value;
	int32_t dy = (target.y - camPos.y).value;
	int32_t dz = (target.z - camPos.z).value;
	int32_t flat = ISqrt(dx * dx + dz * dz);
	m_camera.SetAngle(-psyqo::atan2(dy, flat), psyqo::atan2(dx, dz), 0);
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
	m_heroZ = HERO_START_Z * 128 * 16;
	m_heroDirection = 1;
	if (m_hero) {
		m_hero->SetRotation(0, 0, 0);
	}

#ifdef TECHDEMO_BENCH
	m_saveRequested = true;
#endif
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
	auto* fs = card.FileSystem();
	fs->writeFile(psyqo::MemoryCard::Port::Port0, SAVE_NAME, "MADNIGHT TECH DEMO", m_icon, &m_saveBuffer,
				  sizeof(m_saveBuffer), [this, fs](auto error) {
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
					  fs->readFile(psyqo::MemoryCard::Port::Port0, SAVE_NAME, &m_readBack, sizeof(m_readBack),
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
