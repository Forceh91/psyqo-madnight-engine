/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <EASTL/array.h>
#include <psyqo/gte-registers.hh>
#include <psyqo/primitives/common.hh>
#include <psyqo/primitives/control.hh>
#include <psyqo/primitives/quads.hh>
#include <psyqo/primitives/triangles.hh>

struct RendererQuad {
	psyqo::GTE::PackedVec3 verts[4] = {};
	psyqo::GTE::PackedVec3* boneAdjustedVerts = nullptr;
	int16_t vertIxs[4] = {};
	psyqo::GTE::PackedVec3 normal = {};
	psyqo::Color colours[4] = {};
	psyqo::PrimPieces::UVCoords uvA = {}, uvB = {};
	psyqo::PrimPieces::UVCoordsPadded uvC = {}, uvD = {};
};

struct RendererTri {
	psyqo::GTE::PackedVec3 verts[3] = {};
	psyqo::GTE::PackedVec3* boneAdjustedVerts = nullptr;
	int16_t vertIxs[3] = {};
	psyqo::GTE::PackedVec3 normal = {};
	psyqo::Color colours[3] = {};
	psyqo::PrimPieces::UVCoords uvA = {}, uvB = {};
	psyqo::PrimPieces::UVCoordsPadded uvC = {};
};

// used for sub division on quads
struct FaceCorner {
	psyqo::GTE::PackedVec3 vert; // world space vert pos
	psyqo::Vertex projected;	 // projected screen pos
	uint32_t depth;				 // depth of the projected corner
	uint32_t ir;				 // ir0 of the projected vert
	psyqo::Color colour;
	psyqo::Color fogColour; // if advanced fog is turned on
	psyqo::PrimPieces::UVCoords uv;
};
