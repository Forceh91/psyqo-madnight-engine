#include "madnight.hh"

#include <EASTL/vector.h>
#include <psyqo/alloc.h>
#include <third_party/snitch/snitch_all.hpp>

// `HardLoadingScreen` hands its queue to `FileLoader::LoadFiles` with `clearPools` on, which dumps the engine's
// pools before anything new is loaded. `SoftLoadingScreen` passes `clearPools` off. an empty queue runs the dump
// and returns without touching the CD, so the coroutine finishes on its first resume.
static void RunLoadFiles(bool clearPools) {
	eastl::vector<LoadQueue> files;
	auto routine = g_madnightEngine.m_fileLoader.LoadFiles(eastl::move(files), clearPools);
	routine.resume();
	REQUIRE(routine.done());
}

static psyqo::Vec3 ZeroPos() {
	psyqo::Vec3 pos;
	pos.x = 0;
	pos.y = 0;
	pos.z = 0;
	return pos;
}

static psyqo::Vec2 UnitSize() {
	psyqo::Vec2 size;
	size.x = 1.0;
	size.y = 1.0;
	return size;
}

static GameObject* MakeGameObject(const eastl::string_view& name) {
	return g_madnightEngine.m_gameObjectManager.CreateGameObject(name, ZeroPos(), GameObjectRotation{0, 0, 0});
}

TEST_CASE("Hard loading screen teardown destroys every game object and frees the whole pool") {
	auto& mgr = g_madnightEngine.m_gameObjectManager;

	GameObject* renderList[3];
	for (auto i = 0; i < 3; i++) {
		renderList[i] = MakeGameObject("scene_object");
		REQUIRE(renderList[i] != nullptr);
	}
	mgr.SetRenderableGameObjects(eastl::span<GameObject*>(renderList, 3));
	REQUIRE(mgr.GetActiveGameObjects().size() == 3);

	RunLoadFiles(true);

	REQUIRE(mgr.GetActiveGameObjects().empty());
	for (auto i = 0; i < MAX_GAME_OBJECTS; i++) {
		REQUIRE(mgr.GetGameObjects()[i].id() == INVALID_POOL_ID);
	}

	// every slot is free again, handed out from 0
	for (auto i = 0; i < MAX_GAME_OBJECTS; i++) {
		auto* obj = MakeGameObject("refill");
		REQUIRE(obj != nullptr);
		REQUIRE(obj->id() == i);
	}
	REQUIRE(MakeGameObject("overflow") == nullptr);

	mgr.Dump();
}

TEST_CASE("Hard loading screen teardown frees the collision data") {
	auto* colbin = g_madnightEngine.m_colbinManager.Colbin();

	// stand in for a loaded colbin, allocated the same way `LoadColbin` does
	colbin->header.floorTriCount = 2;
	colbin->header.wallOBBCount = 1;
	colbin->gridHeader.gridWidth = 2;
	colbin->gridHeader.gridHeight = 1;
	colbin->floors = (FloorTri*)psyqo_malloc(sizeof(FloorTri) * 2);
	colbin->walls = (OBB*)psyqo_malloc(sizeof(OBB));
	colbin->gridCells = (GridCell*)psyqo_malloc(sizeof(GridCell) * 2);
	for (auto i = 0; i < 2; i++) {
		colbin->gridCells[i].count = 1;
		colbin->gridCells[i].indices = (uint16_t*)psyqo_malloc(sizeof(uint16_t));
	}
	REQUIRE(g_madnightEngine.m_colbinManager.walls().size() == 1);

	RunLoadFiles(true);

	REQUIRE(colbin->floors == nullptr);
	REQUIRE(colbin->walls == nullptr);
	REQUIRE(colbin->gridCells == nullptr);
	REQUIRE(colbin->header.floorTriCount == 0);
	REQUIRE(colbin->header.wallOBBCount == 0);
	REQUIRE(colbin->gridHeader.gridWidth == 0);
	REQUIRE(colbin->gridHeader.gridHeight == 0);
	REQUIRE(g_madnightEngine.m_colbinManager.walls().empty());
}

TEST_CASE("Soft loading screen keeps the current scene's game objects") {
	auto& mgr = g_madnightEngine.m_gameObjectManager;

	auto* obj = MakeGameObject("kept");
	REQUIRE(obj != nullptr);
	auto id = obj->id();

	RunLoadFiles(false);

	const auto& active = mgr.GetActiveGameObjects();
	REQUIRE(active.size() == 1);
	REQUIRE(active[0] == obj);
	REQUIRE(obj->id() == id);
	REQUIRE(mgr.GetGameObjectByName("kept") == obj);

	mgr.Dump();
}

TEST_CASE("Hard loading screen teardown destroys every billboard and emitter and frees both pools") {
	auto& billboards = g_madnightEngine.m_billboardManager;
	auto& emitters = g_madnightEngine.m_particleEmitterManager;

	auto* billboard = billboards.CreateBillboard("scene_billboard", ZeroPos(), UnitSize());
	REQUIRE(billboard != nullptr);
	psyqo::FixedPoint<> radius = 5.0;
	auto* emitter = emitters.CreateParticleEmitter("scene_emitter", ZeroPos(), radius, 10, 2);
	REQUIRE(emitter != nullptr);

	RunLoadFiles(true);

	REQUIRE(billboards.GetActiveBillboards().empty());
	REQUIRE(emitters.GetActiveEmitters().empty());
	for (auto i = 0; i < MAX_BILLBOARDS; i++) {
		REQUIRE(billboards.GetBillboards()[i].id() == INVALID_POOL_ID);
	}
	for (auto i = 0; i < MAX_PARTICLE_EMITTERS; i++) {
		REQUIRE(emitters.GetEmitters()[i].id() == INVALID_POOL_ID);
	}

	// every slot is free again
	for (auto i = 0; i < MAX_BILLBOARDS; i++) {
		REQUIRE(billboards.CreateBillboard("refill", ZeroPos(), UnitSize()) != nullptr);
	}
	REQUIRE(billboards.CreateBillboard("one too many", ZeroPos(), UnitSize()) == nullptr);
	billboards.Dump();
}
