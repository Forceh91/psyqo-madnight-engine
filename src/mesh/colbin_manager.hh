#pragma once
#include "../core/collision_types.hh"
#include "../helpers/archive.hh"
#include "EASTL/fixed_string.h"
#include "EASTL/span.h"
#include "psyqo/coroutine.hh"
#include "psyqo/vector.hh"

struct Header {
	eastl::fixed_string<char, 6> magic = ""; // COLBIN
	uint8_t version = 0;					 // 2
	uint32_t floorTriCount = 0;
	uint32_t wallOBBCount = 0;
};

struct GridHeader {
	int32_t originX = 0;
	int32_t originZ = 0;
	uint32_t cellSize = 0;
	uint16_t gridWidth = 0;
	uint16_t gridHeight = 0;
};

struct GridCell {
	uint16_t count = 0;
	uint16_t* indices = nullptr;
};

struct FloorTri {
	psyqo::Vec3 v0 = {0, 0, 0}; // vertex 0 (x, y, z) — scaled by 128
	psyqo::Vec3 v1 = {0, 0, 0}; // vertex 1 (x, y, z) — scaled by 128
	psyqo::Vec3 v2 = {0, 0, 0}; // vertex 2 (x, y, z) — scaled by 128
	int16_t n[3] = {0, 0, 0};	// face normal (x, y, z) — FP12 (scaled by 4096)
};

struct ColBin {
	Header header = {};
	GridHeader gridHeader = {};
	GridCell* gridCells = nullptr;
	FloorTri* floors = nullptr;
	OBB* walls = nullptr;
};

class ColbinManager {
  public:
	static psyqo::Coroutine<> LoadColbin(const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN>& name,
										 ColBin** colbinOut);
	static ColBin* Colbin(void) { return &m_colbin; }
	static void Dump(void);
	static eastl::span<OBB> walls(void) { return {m_colbin.walls, m_colbin.header.wallOBBCount}; };

  private:
	static ColBin m_colbin;
};
