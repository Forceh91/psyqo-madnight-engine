#include "madnight.hh"

#include <third_party/snitch/snitch_all.hpp>

using namespace psyqo::fixed_point_literals;

// an axis-aligned box, sizes in world space fixed point
static OBB Box(psyqo::FixedPoint<> x, psyqo::FixedPoint<> y, psyqo::FixedPoint<> z, psyqo::FixedPoint<> hx,
			   psyqo::FixedPoint<> hy, psyqo::FixedPoint<> hz) {
	OBB box;
	box.center = {x, y, z};
	box.axes[0] = {1.0_fp, 0.0_fp, 0.0_fp};
	box.axes[1] = {0.0_fp, 1.0_fp, 0.0_fp};
	box.axes[2] = {0.0_fp, 0.0_fp, 1.0_fp};
	box.halfExtents = {hx, hy, hz};
	return box;
}

TEST_CASE("IsSATCollision treats boxes that only touch as apart") {
	CollisionTest result;
	auto wall = Box(0.0_fp, 0.0_fp, 0.0_fp, 1.0_fp, 2.0_fp, 4.0_fp);

	// face to face on x, sharing the whole y/z overlap
	auto touching = Box(2.0_fp, 0.0_fp, 0.0_fp, 1.0_fp, 1.0_fp, 1.0_fp);
	CHECK_FALSE(g_madnightEngine.m_collisionHelper.IsSATCollision(touching, wall, &result));

	auto apart = Box(2.5_fp, 0.0_fp, 0.0_fp, 1.0_fp, 1.0_fp, 1.0_fp);
	CHECK_FALSE(g_madnightEngine.m_collisionHelper.IsSATCollision(apart, wall, &result));
}

TEST_CASE("IsSATCollision pushes an overlapping box out along the shallowest axis") {
	CollisionTest result;
	auto wall = Box(0.0_fp, 0.0_fp, 0.0_fp, 1.0_fp, 2.0_fp, 4.0_fp);

	// 0.25 into the wall on x, deep on y and z
	auto box = Box(1.75_fp, 0.0_fp, 0.0_fp, 1.0_fp, 1.0_fp, 1.0_fp);
	REQUIRE(g_madnightEngine.m_collisionHelper.IsSATCollision(box, wall, &result));
	CHECK(result.normal.x == 1.0_fp);
	CHECK(result.normal.y == 0.0_fp);
	CHECK(result.normal.z == 0.0_fp);
	CHECK(result.penetration == 0.25_fp);
}
