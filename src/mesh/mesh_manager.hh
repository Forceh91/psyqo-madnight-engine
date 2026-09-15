#pragma once
#include <EASTL/fixed_string.h>
#include <stdint.h>

#include "psyqo/coroutine.hh"
#include "psyqo/primitives/common.hh"
#include "psyqo/vector.hh"

#include "../core/collision_types.hh"
#include "skeleton/skeleton.hh"

static constexpr uint8_t MAX_LOADED_MESHES = 250;
static constexpr uint16_t MAX_FACES_PER_MESH = 1000;

struct MeshBinVertexColours {
	uint8_t r = -1, g = -1, b = -1; // -1 if not present. otherwise 0-255
} alignas(4);

struct MeshBinIndex {
	int16_t i1 = 0, i2 = 0, i3 = 0, i4 = 0;
} alignas(4);

struct BoundingSphere {
	psyqo::Vec3 centre = {0, 0, 0};
	int32_t radius = 0;
};

struct MeshBin {
	uint8_t type = 0; // 1 = quads, 2 = tris (unused)

	// sub header
	uint32_t vertexCount = 0;
	uint32_t indicesCount = 0;
	uint32_t facesCount = 0;
	uint32_t normalsCount = 0;
	uint32_t uvCount = 0;
	uint8_t hasSkeleton = 0;
	uint8_t numBones = 0;

	// variable-length data
	// verts
	psyqo::Vec3* vertices = nullptr;
	MeshBinVertexColours* vertexColours = nullptr;
	MeshBinIndex* vertexIndices = nullptr;

	// noramls
	psyqo::Vec3* normals = nullptr;
	MeshBinIndex* normalIndices = nullptr;

	// UVs
	psyqo::PrimPieces::UVCoords* uvs = nullptr;
	MeshBinIndex* uvIndices = nullptr;

	// skeleton info
	Skeleton* skeleton = nullptr;
	uint8_t* boneForVertex = nullptr; // vertex index -> bone index
	psyqo::Vec3* verticesOnBonePos = nullptr;

	// basic min/max collision box
	AABBCollision collisionBox = {};
	BoundingSphere bsphere = {};
};

struct LoadedMeshBin {
	uint64_t meshNameHash = 0;
	bool isLoaded = false;
	MeshBin mesh = {};
};

class MeshManager {
	static LoadedMeshBin mLoadedMeshes[MAX_LOADED_MESHES];

	static MeshBin* IsMeshLoaded(const eastl::string_view& mesh_name);
	static MeshBin* IsMeshLoaded(uint64_t meshNameHash);
	static int16_t FindSpaceForMesh(void);

  public:
	static psyqo::Coroutine<> LoadMesh(const eastl::string_view& meshName, MeshBin** meshOut);
	static void GetMeshFromName(const eastl::string_view& meshName, MeshBin** meshOut);
	static void UnloadMesh(const eastl::string_view& mesh_name);

	// dump all meshes in memory and start fresh
	// this is used when switching to a loading screen for instance.
	// this is a dangerous function as it wont check if anything is used
	static void Dump(void);
	static void FreeLoadedMesh(LoadedMeshBin* mesh);
};
