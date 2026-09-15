#pragma once

#include "psyqo/vector.hh"

enum CollisionType { SOLID, TRIGGER };

struct OBB {
	psyqo::Vec3 center = {0, 0, 0};
	psyqo::Vec3 axes[3] = {};
	psyqo::Vec3 halfExtents = {0, 0, 0};
	uint32_t flags = 0; // reserved, defaults to 0
};

struct AABBCollision {
	psyqo::Vec3 min = {0, 0, 0};
	psyqo::Vec3 max = {0, 0, 0};
};
