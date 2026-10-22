#pragma once

#include <EASTL/array.h>
#include <psyqo/gte-registers.hh>
#include <psyqo/primitives/common.hh>

struct QuadNode {
	psyqo::GTE::PackedVec3 verts[4];		  // world space vert positions
	eastl::array<psyqo::Vertex, 4> projected; // actually projected vert positions
	uint32_t projectedDepths[4];			  // the z value of each projected corner
	psyqo::Color colours[4];				  // colours
	uint32_t irs[4];						  // irs
	psyqo::PrimPieces::UVCoords uvA = {}, uvB = {};
	psyqo::PrimPieces::UVCoordsPadded uvC = {}, uvD = {};
	bool isSkeleton = false;
};
