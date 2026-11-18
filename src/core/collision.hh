/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "collision_types.hh"
#include "object/gameobject.hh"

#include <psyqo/fixed-point.hh>

static constexpr uint8_t COLLISION_BROAD_PHASE_LIMIT = 20;

struct CollisionTest {
	psyqo::Vec3 mtv = {0, 0, 0};
	psyqo::Vec3 normal = {0, 0, 0};
	psyqo::FixedPoint<> penetration = 0;
};

class Collision {
  public:
	void GenerateAABBForMesh(const GameObject* object, AABBCollision* collisionBoxOut);
	bool IsAABBCollision(const AABBCollision& collisionA, const AABBCollision& collisionB);
	bool IsSATCollision(const OBB& collisionA, const OBB& collisionB, CollisionTest* resultOut);

	// does a quick broad phase pass over potential OBB colliders.
	// for the OBB to compare against, whatever OBB your game object has is sufficient here.
	// this function will automatically gather all walls from your colbin and do a grid check against them
	// if no colbin is loaded then it early exits and your `potentialCollisions` vector is untouched
	void
	PotentialColbinWallCollisions(const OBB& obb,
								  eastl::fixed_vector<OBB*, COLLISION_BROAD_PHASE_LIMIT, false>& potentialCollisions);
};
