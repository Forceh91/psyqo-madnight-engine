/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "../../helpers/archive.hh"
#include "gameobject.hh"
#include "gameobject_defs.hh"

#include <EASTL/array.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/span.h>
#include <psyqo/vector.hh>

static constexpr uint8_t MAX_GAME_OBJECTS = 250;

class GameObjectManager final {
	eastl::array<GameObject, MAX_GAME_OBJECTS> m_gameObjects;
	eastl::fixed_vector<GameObject*, MAX_GAME_OBJECTS> m_activeGameObjects;
	eastl::fixed_vector<GameObject*, MAX_GAME_OBJECTS> m_renderableGameObjects;

	int16_t GetFreeIndex(void);

  public:
	GameObject* CreateGameObject(const eastl::string_view& name, const uint64_t& nameHash, const psyqo::Vec3& pos,
								 const GameObjectRotation& rotation, const GameObjectTag& tag = GameObjectTag::NONE);
	GameObject* CreateGameObject(const eastl::string_view& name, const psyqo::Vec3& pos,
								 const GameObjectRotation& rotation, const GameObjectTag& tag = GameObjectTag::NONE) {
		return CreateGameObject(name, HashName(name), pos, rotation, tag);
	}
	void DestroyGameObject(GameObject* gameObject);
	const eastl::fixed_vector<GameObject*, MAX_GAME_OBJECTS>& GetActiveGameObjects(void);
	void ClearRenderableGameObjects(void);
	void SetRenderableGameObjects(const eastl::span<GameObject*> renderList);
	const eastl::fixed_vector<GameObject*, MAX_GAME_OBJECTS>& GetGameObjectsWithTag(GameObjectTag tag);
	const eastl::array<GameObject, MAX_GAME_OBJECTS>& GetGameObjects(void) { return m_gameObjects; }
	GameObject* GetGameObjectByName(const eastl::string_view& name);
	GameObject* GetGameObjectByName(uint64_t nameHash);
	void Dump(void);
};
