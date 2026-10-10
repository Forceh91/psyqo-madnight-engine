#pragma once

#include <psyqo/gte-registers.hh>
#include <psyqo/primitives/common.hh>

struct RendererQuad {
	psyqo::GTE::PackedVec3 verts[4];
	psyqo::GTE::PackedVec3 normal;
	psyqo::Color colours[4];
	psyqo::PrimPieces::UVCoords uvA, uvB;
	psyqo::PrimPieces::UVCoordsPadded uvC, uvD;
};
