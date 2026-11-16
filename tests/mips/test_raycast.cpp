#include "core/object/gameobject_manager.hh"
#include "core/raycast.hh"
#include "helpers/world_space.hh"
#include "madnight.hh"
#include "render/camera.hh"

#include <psyqo/gte-math.hh>
#include <psyqo/soft-math.hh>
#include <third_party/snitch/snitch_all.hpp>

using namespace psyqo::fixed_point_literals;
using namespace psyqo::trig_literals;

static GameObject* Trigger(const char* name, psyqo::Vec3 pos, psyqo::FixedPoint<> size) {
	auto* object = g_madnightEngine.m_gameObjectManager.CreateGameObject(name, pos, GameObjectRotation{0, 0, 0},
																		 GameObjectTag::INTERACTABLE);
	REQUIRE(object != nullptr);
	object->SetAsTrigger({size, size, size});
	return object;
}

static psyqo::FixedPoint<> Abs(psyqo::FixedPoint<> v) { return v < 0 ? -v : v; }

TEST_CASE("RaycastScene returns the nearest hit, not the first in the pool") {
	// created far first so it sits ahead of the near one in the pool
	auto* farBox = Trigger("far", {0, 0, -4.0_ws}, 1.0_ws);
	auto* nearBox = Trigger("near", {0, 0, -2.0_ws}, 1.0_ws);

	Ray ray = {{0, 0, 0}, {0, 0, -1.0_fp}, 5.0_ws};
	RayHit hit;
	REQUIRE(g_madnightEngine.m_raycast.RaycastScene(ray, GameObjectTag::INTERACTABLE, &hit));
	CHECK(hit.object == nearBox);
	CHECK(hit.distance == 1.5_ws);
	CHECK(hit.hitPos.z == -1.5_ws);

	g_madnightEngine.m_gameObjectManager.DestroyGameObject(farBox);
	g_madnightEngine.m_gameObjectManager.DestroyGameObject(nearBox);
}

TEST_CASE("Camera forwardVector is the view direction when pitched and yawed") {
	Camera camera;
	camera.SetAngle(0.2_pi, 0.25_pi, 0);

	// in view space the forward vector must be the +z axis, the centre of the screen
	psyqo::Vec3 view;
	psyqo::GteMath::matrixVecMul3(camera.rotationMatrix(), camera.forwardVector(), &view);
	CHECK(Abs(view.x) < 0.01_fp);
	CHECK(Abs(view.y) < 0.01_fp);
	CHECK(view.z > 0.99_fp);

	psyqo::GteMath::matrixVecMul3(camera.rotationMatrix(), camera.rightVector(), &view);
	CHECK(view.x > 0.99_fp);
	CHECK(Abs(view.y) < 0.01_fp);
	CHECK(Abs(view.z) < 0.01_fp);

	psyqo::GteMath::matrixVecMul3(camera.rotationMatrix(), camera.upVector(), &view);
	CHECK(Abs(view.x) < 0.01_fp);
	CHECK(view.y > 0.99_fp);
	CHECK(Abs(view.z) < 0.01_fp);
}
