#pragma once

#include "object/gameobject.hh"
#include "world_defs.hh"

#include "psyqo/vector.hh"

static constexpr psyqo::FixedPoint<> maxRayDistance = ONE_METRE * 10; // 10m

typedef struct _RAY {
	psyqo::Vec3 origin = {0, 0, 0};
	psyqo::Vec3 direction = {0, 0, 0};	 // normalized
	psyqo::FixedPoint<> maxDistance = 0; // in metres. try to keep this value small. 128px = 1m
} Ray;

typedef struct _RAY_HIT {
	bool hit = false;
	psyqo::FixedPoint<> distance = 0;
	psyqo::Vec3 hitPos = {0, 0, 0}; // world-space hit
	GameObject* object = nullptr;
} RayHit;

class Raycast {
	static bool DoesRaycastInterceptAABB(const Ray& ray, const GameObject* object, psyqo::FixedPoint<>* outDistance);

  public:
	static bool RaycastScene(const Ray& ray, GameObjectTag targetTag, RayHit* hitOut);
};
