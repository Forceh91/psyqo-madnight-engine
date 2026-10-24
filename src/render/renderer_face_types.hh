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

struct FaceNode {
	psyqo::GTE::PackedVec3 verts[4];		  // world space vert positions
	eastl::array<psyqo::Vertex, 4> projected; // actually projected vert positions
	uint32_t projectedDepths[4];			  // the z value of each projected corner
	psyqo::Color colours[4];				  // colours
	psyqo::Color fogColours[4];				  // for advanced fog colours
	uint32_t irs[4];						  // irs
	psyqo::PrimPieces::UVCoords uvA = {}, uvB = {};
	psyqo::PrimPieces::UVCoordsPadded uvC = {}, uvD = {};
};

struct CutoutFogQuad {
	psyqo::Prim::TPage tpage; // ABR = FullBackAndFullFront
	psyqo::Prim::MaskControl forceSet{psyqo::Prim::MaskControl::ForceSet};
	psyqo::Prim::GouraudQuad marker; // black, setSemiTrans()
	psyqo::Prim::MaskControl fromSource{psyqo::Prim::MaskControl::FromSource};
	psyqo::Prim::GouraudTexturedQuad textured; // VC, setOpaque()
	psyqo::Prim::MaskControl test{psyqo::Prim::MaskControl::FromSource, psyqo::Prim::MaskControl::Test::Yes};
	psyqo::Prim::GouraudQuad fog; // FC*f, setSemiTrans()
	psyqo::Prim::MaskControl reset{psyqo::Prim::MaskControl::FromSource};
};
