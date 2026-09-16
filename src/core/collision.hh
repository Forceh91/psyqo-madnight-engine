#pragma once

#include "collision_types.hh"
#include "object/gameobject.hh"
#include <psyqo/fixed-point.hh>

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
};
