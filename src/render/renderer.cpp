/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "renderer.hh"
#include "EASTL/algorithm.h"
#include "clip.hh"

#include "../core/billboard/billboard_manager.hh"
#include "../core/debug/debug_menu.hh"
#include "../core/debug/perf_monitor.hh"
#include "../core/object/gameobject_manager.hh"
#include "../core/particles/particle_manager.hh"
#include "../defs.hh"
#include "../madnight.hh"

#include <psyqo/fixed-point.hh>
#include <psyqo/fragments.hh>
#include <psyqo/gte-kernels.hh>
#include <psyqo/gte-math.hh>
#include <psyqo/gte-registers.hh>
#include <psyqo/matrix.hh>
#include <psyqo/primitives/common.hh>
#include <psyqo/primitives/control.hh>
#include <psyqo/primitives/quads.hh>
#include <psyqo/primitives/sprites.hh>
#include <psyqo/primitives/triangles.hh>
#include <psyqo/vector.hh>

Renderer* Renderer::m_instance = nullptr;
psyqo::Font<100> Renderer::m_systemFont;
static constexpr psyqo::Rect SCREEN_SPACE = {.pos = {0, 0}, .size = {320, 240}};
static constexpr psyqo::Matrix33 identityMatrix = {
	{{1.0_fp, 0.0_fp, 0.0_fp}, {0.0_fp, 1.0_fp, 0.0_fp}, {0.0_fp, 0.0_fp, 1.0_fp}}};
static constexpr uint8_t PROJECTION_DISTANCE = 120;

#if ENABLE_BONE_DEBUG
static constexpr psyqo::Color boneColours[MAX_BONES] = {
	// Spine + neck
	{255, 255, 100}, // 0: Hips
	{255, 255, 120}, // 1: Spine
	{255, 255, 140}, // 2: Spine1
	{255, 255, 160}, // 3: Spine2
	{255, 255, 180}, // 4: Neck

	// Head
	{64, 224, 208}, // 5: Head (turquoise)
	{72, 239, 223}, // 6: HeadTop_End

	// Left Arm (blue-heavy)
	{0, 100, 255},	 // 7: LeftShoulder
	{255, 120, 255}, // 8: LeftArm
	{125, 140, 255}, // 9: LeftForeArm
	{80, 160, 255},	 // 10: LeftHand
	{0, 180, 255},	 // 11: LeftHandThumb1
	{0, 200, 255},	 // 12: LeftHandThumb2
	{0, 220, 255},	 // 13: LeftHandThumb3
	{0, 240, 255},	 // 14: LeftHandThumb4
	{0, 255, 220},	 // 15: LeftHandIndex1
	{0, 255, 200},	 // 16: LeftHandIndex2
	{0, 255, 180},	 // 17: LeftHandIndex3
	{0, 255, 160},	 // 18: LeftHandIndex4

	// Right Arm (green-heavy)
	{0, 255, 120},	 // 19: RightShoulder
	{255, 255, 100}, // 20: RightArm
	{125, 220, 100}, // 21: RightForeArm
	{80, 200, 100},	 // 22: RightHand
	{0, 180, 120},	 // 23: RightHandThumb1
	{0, 160, 140},	 // 24: RightHandThumb2
	{0, 140, 160},	 // 25: RightHandThumb3
	{0, 120, 180},	 // 26: RightHandThumb4
	{0, 100, 200},	 // 27: RightHandIndex1
	{0, 100, 220},	 // 28: RightHandIndex2
	{0, 100, 240},	 // 29: RightHandIndex3
	{0, 120, 255},	 // 30: RightHandIndex4

	// Left Leg (red/orange)
	{255, 100, 0}, // 31: LeftUpLeg
	{255, 120, 0}, // 32: LeftLeg
	{255, 140, 0}, // 33: LeftFoot
	{255, 160, 0}, // 34: LeftToeBase
	{255, 180, 0}, // 35: LeftToe_End

	// Right Leg (red/orange variant)
	{255, 100, 50}, // 36: RightUpLeg
	{255, 120, 50}, // 37: RightLeg
	{255, 140, 50}, // 38: RightFoot
	{255, 160, 50}, // 39: RightToeBase
	{255, 180, 50}	// 40: RightToe_End
};
#endif

void Renderer::Init(psyqo::GPU& gpuInstance) {
	if (m_instance != nullptr) {
		return;
	}

	m_instance = new Renderer(gpuInstance);
	m_systemFont.uploadSystemFont(m_instance->GPU(), {960, 256});
}

void Renderer::VRamUpload(const uint16_t* data, int16_t x, int16_t y, int16_t width, int16_t height) {
	psyqo::Rect vramRegion = {.pos = {x, y}, .size = {width, height}};
	m_gpu.uploadToVRAM(data, vramRegion);
}

void Renderer::StartScene(void) {
	// update lighting instance
	m_lighting = &Lighting::instance();

	// clear translation registers
	psyqo::GTE::clear<psyqo::GTE::Register::TRX, psyqo::GTE::Unsafe>();
	psyqo::GTE::clear<psyqo::GTE::Register::TRY, psyqo::GTE::Unsafe>();
	psyqo::GTE::clear<psyqo::GTE::Register::TRZ, psyqo::GTE::Unsafe>();

	// set the screen offset in the GTE (half of the screen x/y resolution is the standard)
	psyqo::GTE::write<psyqo::GTE::Register::OFX, psyqo::GTE::Unsafe>(psyqo::FixedPoint<16>(160.0).raw());
	psyqo::GTE::write<psyqo::GTE::Register::OFY, psyqo::GTE::Unsafe>(psyqo::FixedPoint<16>(120.0).raw());

	// write the projection plane distance
	psyqo::GTE::write<psyqo::GTE::Register::H, psyqo::GTE::Unsafe>(PROJECTION_DISTANCE);

	// set the scaling for z averaging
	psyqo::GTE::write<psyqo::GTE::Register::ZSF3, psyqo::GTE::Unsafe>(ORDERING_TABLE_SIZE / 3);
	psyqo::GTE::write<psyqo::GTE::Register::ZSF4, psyqo::GTE::Unsafe>(ORDERING_TABLE_SIZE / 4);

	// set fog colour (FC)
	SetFarColour();
}

void Renderer::SetFogColour(const psyqo::Color& colour) {
	m_lighting->SetFogColour(colour);
	SetFarColour();
}

void Renderer::SetFarColour(void) {
	psyqo::GTE::write<psyqo::GTE::Register::RFC, psyqo::GTE::Unsafe>(m_lighting->GetFogColour().r << 4);
	psyqo::GTE::write<psyqo::GTE::Register::GFC, psyqo::GTE::Unsafe>(m_lighting->GetFogColour().g << 4);
	psyqo::GTE::write<psyqo::GTE::Register::BFC, psyqo::GTE::Unsafe>(m_lighting->GetFogColour().b << 4);

	SetFogNearFar(0.5_fp, 1.55_fp);
}

void Renderer::SetFogNearFar(psyqo::FixedPoint<> near, psyqo::FixedPoint<> far) {
	if (near == 0.0_fp || far == 0.0_fp) {
		return;
	}

	const auto a = near.value;
	const auto b = far.value;
	const auto h = SCREEN_SPACE.size.x / 2;

	// TODO: rewrite this to use fixed point numbers directly
	const auto dqa = ((-a * b / (b - a)) << 8) / h;
	const auto dqaF = eastl::clamp<int16_t>(dqa, -32767, 32767);
	const auto dqbF = ((b << 12) / (b - a) << 12);

	psyqo::GTE::write<psyqo::GTE::Register::DQA, psyqo::GTE::Unsafe>(dqaF);
	psyqo::GTE::write<psyqo::GTE::Register::DQB, psyqo::GTE::Safe>(dqbF);
}

/* this must be called at the start of each frame */
uint32_t Renderer::Process(void) {
	uint32_t currentFrameCount = m_gpu.getFrameCount();

	// figure out delta time (last frame count minus current frame count)
	uint32_t deltaTime = currentFrameCount - m_lastFrameCounter;
	if (deltaTime == 0) {
		return 0;
	}

	// update last frame count
	m_lastFrameCounter = currentFrameCount;

	// reset what sprite/tpage we're drawing
	m_currentSpriteFragment = 0;

	// give back the delta time
	return deltaTime;
}

psyqo::Vec3 Renderer::SetupCamera(const psyqo::Matrix33& camRotationMatrix, const psyqo::Vec3& negativeCamPos) {
	// clear TRX/Y/Z safely
	psyqo::GTE::clear<psyqo::GTE::Register::TRX, psyqo::GTE::Safe>();
	psyqo::GTE::clear<psyqo::GTE::Register::TRY, psyqo::GTE::Safe>();
	psyqo::GTE::clear<psyqo::GTE::Register::TRZ, psyqo::GTE::Safe>();

	// rotate camera translation vector by the camera rotation (matrix)
	psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(camRotationMatrix);
	psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(negativeCamPos);

	// multiply vector by matrix and add vector
	psyqo::GTE::Kernels::mvmva<psyqo::GTE::Kernels::MX::RT, psyqo::GTE::Kernels::MV::V0, psyqo::GTE::Kernels::TV::TR>();
	return psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();
}

psyqo::Vec3 Renderer::TransformObjectToViewSpace(const psyqo::Vec3& pos, const psyqo::Matrix33& cameraRotationMatrix,
												 const psyqo::Matrix33& finalCameraMatrix) {
	// restore camera rotation for delta transform
	psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(cameraRotationMatrix);
	psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(m_gteCameraPos);

	// calculate delta pos of the object compared to the camera
	// hacky fix to get follow camera working: add on a delta offset before doing the final object delta pos
	// deltaOffset will return {0, 0, 0} when not in follow mode
	auto objDeltaPos = m_activeCamera->deltaOffset() + (pos - m_activeCamera->pos());
	psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(objDeltaPos);
	psyqo::GTE::Kernels::rt();

	// get the view space translation and write it back for the meshes
	auto finalObjPos = psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();

	// write the final rotation + translation
	psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(finalCameraMatrix);
	psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(finalObjPos);

	return finalObjPos;
}

void Renderer::Render(void) { Render(1); }

void Renderer::Render(uint32_t deltaTime) {
	// get the frame buffer we're currently rendering
	int frameBuffer = m_gpu.getParity();

	// get the current allocator so we can reset it
	auto& allocator = m_allocators[frameBuffer];
	allocator.reset();

	// chain the fill command to clear the buffer
	auto& clear = m_clear[frameBuffer];
	m_gpu.getNextClear(clear.primitive, m_lighting->GetFogColour());
	m_gpu.chain(clear);

	// make use of `gpu.pumpCallbacks` at some point in here
	// so that the gpu timers, and subsequently the modplayer
	// are able to get their timer ticks fired off on time
	// rendering the objects will be the most expensive part of the rendering
	m_gpu.pumpCallbacks();

	// fallback if we don't have an active camera set
	psyqo::Matrix33 cameraRotationMatrix = {{0, 0, 0}};
	if (m_activeCamera != nullptr) {
		cameraRotationMatrix = m_activeCamera->rotationMatrix();
		m_gteCameraPos = SetupCamera(cameraRotationMatrix, -m_activeCamera->pos());
	}

	RenderGameObjects(deltaTime, cameraRotationMatrix);

	RenderBillboards(deltaTime, cameraRotationMatrix);

	RenderParticles(deltaTime, cameraRotationMatrix);

	// send the entire ordering table as a DMA chain to the gpu
	m_gpu.chain(m_orderingTables[frameBuffer]);

	// do this last incase it gets more complex and needs to go on top
	g_madnightEngine.m_debugMenu.Draw(m_gpu);
}

void Renderer::RenderGameObjects(uint32_t deltaTime, const psyqo::Matrix33& cameraRotationMatrix) {
	eastl::array<psyqo::Vertex, 4> projected;

	uint32_t zIndex = 0;
	psyqo::PrimPieces::TPageAttr tpage;
	psyqo::Rect offset = {0, 0};
	psyqo::Matrix33 finalCameraMatrix = {0};

	auto frameBuffer = m_gpu.getParity();
	auto& allocator = m_allocators[frameBuffer];
	auto& ot = m_orderingTables[frameBuffer];

	// get game objects. if there's nothing to render then just early return
	const auto& gameObjects = g_madnightEngine.m_gameObjectManager.GetActiveGameObjects();
	if (gameObjects.empty()) {
		return;
	}

	// now for each object...
	int renderedObjects = 0;
	for (const auto& gameObject : gameObjects) {
		// we dont need to get mesh data for every single vert since it wont change, so lets only do that once
		const auto mesh = gameObject->mesh();
		if (!mesh) {
			continue;
		}

		// get the rotation matrix for the game object and then combine with the camera rotations
		psyqo::GteMath::multiplyMatrix33(cameraRotationMatrix, gameObject->rotationMatrix(), &finalCameraMatrix);

		// see if the entire game object will be visible based off its aabb centre
		psyqo::Vec3 centre = gameObject->mesh()->bsphere.centre + gameObject->pos();
		auto deltaCentre = TransformObjectToViewSpace(centre, cameraRotationMatrix, finalCameraMatrix);
		if (!IsGameObjectVisible(deltaCentre, gameObject->mesh()->collisionBox, gameObject->mesh()->bsphere.radius)) {
			continue;
		}

		// transform the game object into view space
		renderedObjects++;
		auto viewSpacePos = TransformObjectToViewSpace(gameObject->pos(), cameraRotationMatrix, finalCameraMatrix);

		// if we've got a skeleton on this mesh
		if (mesh->hasSkeleton) {
			g_madnightEngine.m_skeletonController.PlayAnimation(mesh->skeleton, deltaTime);

			// update the bone/rotation matrices
			g_madnightEngine.m_skeletonController.UpdateSkeletonBoneMatrices(mesh->skeleton);

			// adjust pos of verts that are attached to bones
			for (int32_t i = 0; i < mesh->facesCount; i++) {
				auto& face = mesh->facesData[i];
				const int vertCount = face.isQuad ? 4 : 3;
				for (int j = 0; j < vertCount; j++) {
					auto vertIx = face.isQuad ? mesh->quads[face.rendererFaceIx].vertIxs[j]
											  : mesh->tris[face.rendererFaceIx].vertIxs[j];
					auto vert = face.isQuad ? mesh->quads[face.rendererFaceIx].verts[j]
											: mesh->tris[face.rendererFaceIx].verts[j];

					auto& bone = mesh->skeleton->bones[mesh->boneForVertex[vertIx]];
					if (!bone.isDirty) {
						continue;
					}

					psyqo::Matrix33 rotationOffset;
					psyqo::GteMath::multiplyMatrix33(bone.worldMatrix.rotationMatrix,
													 bone.bindPoseInverse.rotationMatrix, &rotationOffset);

					// translation offset = world_T + (R_world * bindPoseInverse.translation)
					psyqo::Vec3 translationOffset;
					psyqo::GteMath::matrixVecMul3(bone.worldMatrix.rotationMatrix, bone.bindPoseInverse.translation,
												  &translationOffset);
					translationOffset += bone.worldMatrix.translation;

					// final position
					psyqo::Vec3 rotatedVert;
					psyqo::GteMath::matrixVecMul3(rotationOffset, vert, &rotatedVert);
					auto packedBonePos = psyqo::GTE::PackedVec3(rotatedVert + translationOffset);
					face.isQuad ? mesh->quads[face.rendererFaceIx].boneAdjustedVerts[j] = packedBonePos
								: mesh->tris[face.rendererFaceIx].boneAdjustedVerts[j];
				}
			}

			// mark all bones as clean
			g_madnightEngine.m_skeletonController.MarkBonesClean(mesh->skeleton);
		}

		// now we've done all this we can render the mesh and apply texture (if needed)
		// we dont need to get texture data for every single vert since it wont change, so lets only do that once
		// if its not a nullptr fill out some data so we don't have to do it every face
		const auto texture = gameObject->texture();
		if (texture) {
			// get the tpage and uv offset info
			tpage = g_madnightEngine.m_textureManager.GetTPageAttr(texture);
			offset = g_madnightEngine.m_textureManager.GetTPageUVForTim(texture);
			offset.pos.y += (texture->height - 1);
		}

		// auto applyUV = [&](auto& uvDest, int index) {
		// 	auto uv = mesh->uvs[index];
		// 	uvDest.u = offset.pos.x + uv.u;
		// 	uvDest.v = offset.pos.y - uv.v;
		// };

		for (int32_t i = 0; i < mesh->facesCount; i++) {
			auto faceData = mesh->facesData[i];

			if (faceData.isQuad) {
				ProcessMeshQuad(mesh->quads[faceData.rendererFaceIx], 0, texture, tpage, offset, allocator, ot);
			} else {
				ProcessMeshTri(mesh->tris[faceData.rendererFaceIx], 0, texture, tpage, offset, allocator, ot);
			}

			// uint32_t pA, pB, pC, pD;

			// // vert 1
			// psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(renderVerts[mesh->vertexIndices[i].i1]);
			// psyqo::GTE::Kernels::rtps();
			// pA = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();

			// // vert 2
			// psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(isQuad ? renderVerts[mesh->vertexIndices[i].i2]
			// 															 : renderVerts[mesh->vertexIndices[i].i3]);
			// psyqo::GTE::Kernels::rtps();
			// pB = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();

			// // vert 3
			// psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(isQuad ? renderVerts[mesh->vertexIndices[i].i3]
			// 															 : renderVerts[mesh->vertexIndices[i].i4]);
			// psyqo::GTE::Kernels::rtps();
			// pC = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();

			// // nclip determines the winding order of the vertices. if they are clockwise then it is facing towards us
			// psyqo::GTE::Kernels::nclip();

			// // read the result of this and skip rendering if its backfaced
			// if (psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0>() == 0) {
			// 	continue;
			// }

			// // read projected verts from SXY0/1/2
			// psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[0].packed);
			// psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&projected[1].packed);
			// psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&projected[2].packed);

			// if (isQuad) {
			// 	// vert 4
			// 	psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(renderVerts[mesh->vertexIndices[i].i4]);
			// 	psyqo::GTE::Kernels::rtps();
			// 	pD = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
			// 	psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&projected[3].packed);

			// 	// average z index for ordering
			// 	psyqo::GTE::Kernels::avsz4();
			// } else {
			// 	// average z index for ordering
			// 	psyqo::GTE::Kernels::avsz3();
			// }

			// // make sure we dont go out of bounds
			// zIndex = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ>();
			// if (zIndex == 0 || zIndex >= ORDERING_TABLE_SIZE) {
			// 	continue;
			// }

			// // if its out of the screen space we can clip too
			// if ((isQuad && quad_clip(&SCREEN_SPACE, &projected[0], &projected[1], &projected[2], &projected[3])) ||
			// 	(!isQuad && tri_clip(&SCREEN_SPACE, &projected[0], &projected[1], &projected[2]))) {
			// 	continue;
			// }

			// // now apply colours using stored IR0 values
			// psyqo::Color colA = {mesh->vertexColours[mesh->vertexIndices[i].i1].r,
			// 					 mesh->vertexColours[mesh->vertexIndices[i].i1].g,
			// 					 mesh->vertexColours[mesh->vertexIndices[i].i1].b};
			// psyqo::Color colB, colC;
			// if (isQuad) {
			// 	colB = {mesh->vertexColours[mesh->vertexIndices[i].i2].r,
			// 			mesh->vertexColours[mesh->vertexIndices[i].i2].g,
			// 			mesh->vertexColours[mesh->vertexIndices[i].i2].b};
			// 	colC = {mesh->vertexColours[mesh->vertexIndices[i].i3].r,
			// 			mesh->vertexColours[mesh->vertexIndices[i].i3].g,
			// 			mesh->vertexColours[mesh->vertexIndices[i].i3].b};
			// } else {
			// 	colB = {mesh->vertexColours[mesh->vertexIndices[i].i3].r,
			// 			mesh->vertexColours[mesh->vertexIndices[i].i3].g,
			// 			mesh->vertexColours[mesh->vertexIndices[i].i3].b};
			// 	colC = {mesh->vertexColours[mesh->vertexIndices[i].i4].r,
			// 			mesh->vertexColours[mesh->vertexIndices[i].i4].g,
			// 			mesh->vertexColours[mesh->vertexIndices[i].i4].b};
			// }

			// ApplyAmbientToColour(&colA);
			// colA = ApplyFogToColourGTE(colA, pA);
			// ApplyAmbientToColour(&colB);
			// colB = ApplyFogToColourGTE(colB, pB);
			// ApplyAmbientToColour(&colC);
			// colC = ApplyFogToColourGTE(colC, pC);

			// if (isQuad) {
			// 	psyqo::Color colD = {mesh->vertexColours[mesh->vertexIndices[i].i4].r,
			// 						 mesh->vertexColours[mesh->vertexIndices[i].i4].g,
			// 						 mesh->vertexColours[mesh->vertexIndices[i].i4].b};
			// 	ApplyAmbientToColour(&colD);
			// 	colD = ApplyFogToColourGTE(colD, pD);

			// 	// now take a quad fragment from our array and:
			// 	// set its vertices
			// 	auto& quad = allocator.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
			// 	quad.primitive.pointA = projected[0];
			// 	quad.primitive.pointB = projected[1];
			// 	quad.primitive.pointC = projected[2];
			// 	quad.primitive.pointD = projected[3];

			// 	// set its colour, and make it opaque
			// 	quad.primitive.setColorA(colA);
			// 	quad.primitive.setColorB(colB);
			// 	quad.primitive.setColorC(colC);
			// 	quad.primitive.setColorD(colD);
			// 	quad.primitive.setOpaque();

			// 	// do we have a texture for this?
			// 	if (texture) {
			// 		// set its tpage
			// 		quad.primitive.tpage = tpage;

			// 		// set its clut if it has one
			// 		if (texture->hasClut) {
			// 			quad.primitive.clutIndex = {texture->clutX, texture->clutY};
			// 		}

			// 		// set its uv coords
			// 		applyUV(quad.primitive.uvA, mesh->uvIndices[i].i1);
			// 		applyUV(quad.primitive.uvB, mesh->uvIndices[i].i2);
			// 		applyUV(quad.primitive.uvC, mesh->uvIndices[i].i3);
			// 		applyUV(quad.primitive.uvD, mesh->uvIndices[i].i4);
			// 	}

			// 	// finally we can insert the quad fragment into the ordering table at the calculated z-index
			// 	if (zIndex <= SUBDIVISION_MAX_DISTANCE) {
			// 		SubdivideTexturedQuad(&quad, zIndex, &ot, 0);
			// 	} else {
			// 		ot.insert(quad, zIndex);
			// 	}
			// } else {
			// 	// now take a tri fragment from our array and:
			// 	// set its vertices
			// 	auto& tri = allocator.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
			// 	tri.primitive.pointA = projected[0];
			// 	tri.primitive.pointB = projected[1];
			// 	tri.primitive.pointC = projected[2];

			// 	// set its colour, and make it opaque
			// 	tri.primitive.setColorA(colA);
			// 	tri.primitive.setColorB(colB);
			// 	tri.primitive.setColorC(colC);
			// 	tri.primitive.setOpaque();

			// 	// do we have a texture for this?
			// 	if (texture) {
			// 		// set its tpage
			// 		tri.primitive.tpage = tpage;

			// 		// set its clut if it has one
			// 		if (texture->hasClut) {
			// 			tri.primitive.clutIndex = {texture->clutX, texture->clutY};
			// 		}

			// 		// set its uv coords
			// 		applyUV(tri.primitive.uvA, mesh->uvIndices[i].i1);
			// 		applyUV(tri.primitive.uvB, mesh->uvIndices[i].i3);
			// 		applyUV(tri.primitive.uvC, mesh->uvIndices[i].i4);
			// 	}

			// 	// finally we can insert the tri fragment into the ordering table at the calculated z-index
			// 	if (zIndex <= SUBDIVISION_MAX_DISTANCE) {
			// 		SubdivideTexturedTri(&tri, zIndex, &ot, 2);
			// 	} else {
			// 		ot.insert(tri, zIndex);
			// 	}
			// }
		}

#if ENABLE_BONE_DEBUG
		if (mesh->hasSkeleton && mesh->skeleton.numBones > 0) {
			for (int j = 0; j < mesh->skeleton->numBones; j++) {
				auto& bone = mesh->skeleton->bones[j];
				psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(bone.startPos);
				psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V1>(bone.endPos);
				psyqo::GTE::Kernels::rtpt();

				psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[0].packed);
				psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&projected[1].packed);

				auto& line = allocator.allocateFragment<psyqo::Prim::Line>();
				line.primitive.pointA = projected[0];
				line.primitive.pointB = projected[1];

				line.primitive.setColorA(boneColours[j]);
				line.primitive.setColorB(boneColours[j]);
				line.primitive.setOpaque();

				ot.insert(line, 1);
			}
		}
#endif
	}

	g_madnightEngine.m_perfMonitor.SetRenderedGameObjects(renderedObjects, gameObjects.size());
}

void Renderer::RenderBillboards(uint32_t deltaTime, const psyqo::Matrix33& cameraRotationMatrix) {
	eastl::array<psyqo::Vertex, 4> projected;
	uint32_t zIndex = 0;
	psyqo::PrimPieces::TPageAttr tpage;
	psyqo::Rect offset = {0, 0};

	auto frameBuffer = m_gpu.getParity();
	auto& allocator = m_allocators[frameBuffer];
	auto& ot = m_orderingTables[frameBuffer];

	auto const& billboards = g_madnightEngine.m_billboardManager.GetActiveBillboards();
	if (billboards.empty()) {
		return;
	}

	// billboards just use the inverse of the camera rotation matrix as rotation
	psyqo::Matrix33 finalCameraMatrix = {0};
	// billboards do not inherit camera rotation, so this is the identity by construction
	finalCameraMatrix = identityMatrix;

	for (auto const& billboard : billboards) {
		TransformObjectToViewSpace(billboard->pos(), cameraRotationMatrix, finalCameraMatrix);

		const auto texture = billboard->pTexture();
		if (texture) {
			tpage = g_madnightEngine.m_textureManager.GetTPageAttr(texture);
			offset = g_madnightEngine.m_textureManager.GetTPageUVForTim(texture);
			offset.pos.y += (texture->height - 1);
		}

		// load first 3 verts into GTE
		psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(billboard->corners()[0]);
		psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V1>(billboard->corners()[1]);
		psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(billboard->corners()[2]);
		psyqo::GTE::Kernels::rtpt();
		psyqo::GTE::Kernels::nclip();

		if (!psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0>()) {
			continue;
		}

		// store the first vert so we can read the last one in
		psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[0].packed);

		psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(billboard->corners()[3]);
		psyqo::GTE::Kernels::rtps();
		uint32_t p = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();

		psyqo::GTE::Kernels::avsz4();
		zIndex = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ>();
		if (zIndex == 0 || zIndex >= ORDERING_TABLE_SIZE) {
			continue;
		}

		// read the last three verts from GTE
		psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[1].packed);
		psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&projected[2].packed);
		psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&projected[3].packed);

		if (quad_clip(&SCREEN_SPACE, &projected[0], &projected[1], &projected[2], &projected[3])) {
			continue;
		}

		// handle colour + fog — all verts share same colour on a billboard
		auto colour = billboard->colour();
		ApplyAmbientToColour(&colour);
		colour = ApplyFogToColourGTE(colour, p);

		if (!texture) {
			auto& quad = allocator.allocateFragment<psyqo::Prim::GouraudQuad>();
			quad.primitive.pointA = projected[0];
			quad.primitive.pointB = projected[1];
			quad.primitive.pointC = projected[2];
			quad.primitive.pointD = projected[3];

			quad.primitive.setColorA(colour);
			quad.primitive.setColorB(colour);
			quad.primitive.setColorC(colour);
			quad.primitive.setColorD(colour);
			quad.primitive.setOpaque();

			ot.insert(quad, zIndex);
		} else {
			auto& quad = allocator.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
			quad.primitive.pointA = projected[0];
			quad.primitive.pointB = projected[1];
			quad.primitive.pointC = projected[2];
			quad.primitive.pointD = projected[3];

			quad.primitive.setColorA(colour);
			quad.primitive.setColorB(colour);
			quad.primitive.setColorC(colour);
			quad.primitive.setColorD(colour);
			quad.primitive.setOpaque();

			quad.primitive.tpage = tpage;
			if (texture->hasClut) {
				quad.primitive.clutIndex = {texture->clutX, texture->clutY};
			}

			auto uvA = billboard->uv()[0];
			quad.primitive.uvA.u = offset.pos.x + uvA.u;
			quad.primitive.uvA.v = offset.pos.y - uvA.v;

			auto uvB = billboard->uv()[1];
			quad.primitive.uvB.u = offset.pos.x + uvB.u;
			quad.primitive.uvB.v = offset.pos.y - uvB.v;

			auto uvC = billboard->uv()[2];
			quad.primitive.uvC.u = offset.pos.x + uvC.u;
			quad.primitive.uvC.v = offset.pos.y - uvC.v;

			auto uvD = billboard->uv()[3];
			quad.primitive.uvD.u = offset.pos.x + uvD.u;
			quad.primitive.uvD.v = offset.pos.y - uvD.v;

			ot.insert(quad, zIndex);
		}
	}
}

// TODO: somethings not quite right with rotation?
void Renderer::RenderParticles(uint32_t deltaTime, const psyqo::Matrix33& cameraRotationMatrix) {
	eastl::array<psyqo::Vertex, 4> projected;
	uint32_t zIndex = 0;
	psyqo::PrimPieces::TPageAttr tpage;
	psyqo::Rect offset = {0, 0};

	auto frameBuffer = m_gpu.getParity();
	auto& allocator = m_allocators[frameBuffer];
	auto& ot = m_orderingTables[frameBuffer];

	auto const& emitters = g_madnightEngine.m_particleEmitterManager.GetActiveEmitters();
	if (emitters.empty()) {
		return;
	}

	// particles just use the inverse of the camera rotation matrix as rotation (when in 3d mode)
	psyqo::Matrix33 finalCameraMatrix = {0};
	// billboards do not inherit camera rotation, so this is the identity by construction
	finalCameraMatrix = identityMatrix;

	for (auto const& emitter : emitters) {
		auto const particles = emitter->particles();

		// send tpage info to gpu
		auto texture = emitter->pParticleTexture();
		if (texture) {
			auto tpageAttr = g_madnightEngine.m_textureManager.GetTPageAttr(texture);
			auto& tpage = allocator.allocateFragment<psyqo::Prim::TPage>();
			tpage.primitive.attr = tpageAttr;
			m_gpu.chain(tpage);
		}

		for (auto const& particle : particles) {
			auto finalParticlePos = TransformObjectToViewSpace(particle.pos(), cameraRotationMatrix, finalCameraMatrix);

			if (emitter->AreParticles2D()) {
				if (finalParticlePos.z <= 0) {
					continue;
				}

				// write the object position and camera rotation matrix
				psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(identityMatrix);
				psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(finalParticlePos);

				// we dont need to offset it
				psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(psyqo::Vec3{0, 0, 0});

				// single point rotation/translation
				psyqo::GTE::Kernels::rtps();

				// keep it within the OT
				zIndex = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ>();
				if (zIndex >= ORDERING_TABLE_SIZE) {
					continue;
				}

				// read in sxy2
				psyqo::Vertex vertex;
				psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&vertex.packed);

				// calculate scaled size and make sure it doesnt go below 1
				auto projectedSize = particle.size() * (1.0_fp * PROJECTION_DISTANCE) / finalParticlePos.z;
				auto scaledSize = psyqo::Vertex{static_cast<int16_t>(projectedSize.x.integer()),
												static_cast<int16_t>(projectedSize.y.integer())};
				scaledSize = {eastl::clamp<int16_t>(scaledSize.x, 1, scaledSize.x),
							  eastl::clamp<int16_t>(scaledSize.y, 1, scaledSize.y)};

				// make sure pos is sane
				auto pos = psyqo::Vertex{static_cast<int16_t>(vertex.x - scaledSize.x / 2),
										 static_cast<int16_t>(vertex.y - scaledSize.y / 2)};
				if (pos.x < 0 || pos.x >= SCREEN_SPACE.size.x || pos.y < 0 || pos.y >= SCREEN_SPACE.size.y) {
					continue;
				}

				auto& sprite = allocator.allocateFragment<psyqo::Prim::Sprite>();
				if (texture) {
					// update sprite with clut/uv data
					sprite.primitive.texInfo.clut = psyqo::PrimPieces::ClutIndex(texture->clutX, texture->clutY);

					// as particles can be quads they have 4 lots of uv data, so just take the first one
					sprite.primitive.texInfo.u = particle.uv()[0].u;
					sprite.primitive.texInfo.v = particle.uv()[0].v;
				}

				sprite.primitive.position = pos;
				sprite.primitive.size = scaledSize;

				// handle fog
				auto colour = particle.colour();
				ApplyAmbientToColour(&colour);

				if (m_lighting->IsSimpleFogEnabled()) {
					auto sz = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ1>();
					ApplyFogToColour(&colour, GetFogFactor(sz));
				}

				// set colour
				sprite.primitive.setColor(colour);

				ot.insert(sprite, zIndex);
			} else {
				if (texture) {
					tpage = g_madnightEngine.m_textureManager.GetTPageAttr(texture);
					offset = g_madnightEngine.m_textureManager.GetTPageUVForTim(texture);
					offset.pos.y += (texture->height - 1);
				}

				// load first 3 verts into GTE
				psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(particle.corners()[0]);
				psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V1>(particle.corners()[1]);
				psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(particle.corners()[2]);
				psyqo::GTE::Kernels::rtpt();
				psyqo::GTE::Kernels::nclip();
				if (!psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0>()) {
					continue;
				}

				// store the first vert so we can read the last one in
				psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[0].packed);

				psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(particle.corners()[3]);
				psyqo::GTE::Kernels::rtps();

				psyqo::GTE::Kernels::avsz4();
				zIndex = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ>();
				if (zIndex == 0 || zIndex >= ORDERING_TABLE_SIZE) {
					continue;
				}

				// read the last three verts from GTE
				psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[1].packed);
				psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&projected[2].packed);
				psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&projected[3].packed);

				// out of screen space, it can be clipped
				if (quad_clip(&SCREEN_SPACE, &projected[0], &projected[1], &projected[2], &projected[3])) {
					continue;
				}

				// handle colour + fog — particles use same colour for all verts so one rtps for IR0 is enough
				// re-transform corner 0 to get a representative IR0 for fog
				psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(particle.corners()[0]);
				psyqo::GTE::Kernels::rtps();
				uint32_t p = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();

				auto colour = particle.colour();
				ApplyAmbientToColour(&colour);
				colour = ApplyFogToColourGTE(colour, p);

				if (!texture) {
					auto& quad = allocator.allocateFragment<psyqo::Prim::GouraudQuad>();
					quad.primitive.pointA = projected[0];
					quad.primitive.pointB = projected[1];
					quad.primitive.pointC = projected[2];
					quad.primitive.pointD = projected[3];

					// set colour
					quad.primitive.setColorA(colour);
					quad.primitive.setColorB(colour);
					quad.primitive.setColorC(colour);
					quad.primitive.setColorD(colour);

					// make opaque
					quad.primitive.setOpaque();

					// insert into OT
					ot.insert(quad, zIndex);
				} else {
					auto& quad = allocator.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
					quad.primitive.pointA = projected[0];
					quad.primitive.pointB = projected[1];
					quad.primitive.pointC = projected[2];
					quad.primitive.pointD = projected[3];

					// set colour
					quad.primitive.setColorA(colour);
					quad.primitive.setColorB(colour);
					quad.primitive.setColorC(colour);
					quad.primitive.setColorD(colour);

					// make opaque
					quad.primitive.setOpaque();

					// set its tpage
					quad.primitive.tpage = tpage;

					// set its clut if it has one
					if (texture->hasClut) {
						quad.primitive.clutIndex = {texture->clutX, texture->clutY};
					}

					// set its uv coords
					auto uvA = particle.uv()[0];
					quad.primitive.uvA.u = offset.pos.x + uvA.u;
					quad.primitive.uvA.v = offset.pos.y - uvA.v;

					auto uvB = particle.uv()[1];
					quad.primitive.uvB.u = offset.pos.x + uvB.u;
					quad.primitive.uvB.v = offset.pos.y - uvB.v;

					auto uvC = particle.uv()[2];
					quad.primitive.uvC.u = offset.pos.x + uvC.u;
					quad.primitive.uvC.v = offset.pos.y - uvC.v;

					auto uvD = particle.uv()[3];
					quad.primitive.uvD.u = offset.pos.x + uvD.u;
					quad.primitive.uvD.v = offset.pos.y - uvD.v;

					// insert into OT
					ot.insert(quad, zIndex);
				}
			}
		}
	}
}

void Renderer::Clear(psyqo::Color color) {
	uint32_t frameBuffer = m_gpu.getParity();
	auto& clear = m_clear[frameBuffer];

	// clear the buffer
	m_gpu.getNextClear(clear.primitive, color);
	m_gpu.chain(clear);
}

void Renderer::RenderSprite(const TimFile* texture, const psyqo::Rect rect, const psyqo::PrimPieces::UVCoords uv) {
	// get the frame buffer we're currently rendering
	int frameBuffer = m_gpu.getParity();

	// current ordering tables and fill command
	auto& clear = m_clear[frameBuffer];
	auto& tpages = m_tpages[frameBuffer];
	auto& sprites = m_sprites[frameBuffer];

	// every caller shares one per-frame fragment budget
	if (m_currentSpriteFragment >= MAX_SPRITE_FRAGMENTS) {
		return;
	}

	// chain tpage info over
	auto tpageAttr = g_madnightEngine.m_textureManager.GetTPageAttr(texture);
	auto& tpage = tpages[m_currentSpriteFragment];
	tpage.primitive.attr = tpageAttr;
	m_gpu.chain(tpage);

	auto& sprite = sprites[m_currentSpriteFragment++];
	sprite.primitive.position = rect.pos;
	sprite.primitive.size = rect.size;

	// set its clut if it has one
	if (texture->hasClut) {
		sprite.primitive.texInfo.clut = psyqo::PrimPieces::ClutIndex(texture->clutX, texture->clutY);
	}

	// set the uv data
	psyqo::Rect uvOffset = g_madnightEngine.m_textureManager.GetTPageUVForTim(texture);
	sprite.primitive.texInfo.u = uv.u; // uvOffset.pos.x + uv.u;
	sprite.primitive.texInfo.v = uv.v; // uvOffset.pos.y + (rect.size.y - 1 - uv.v);

	m_gpu.chain(sprite);
}

void Renderer::SetActiveCamera(Camera* camera) { m_activeCamera = camera; }

bool Renderer::IsGameObjectVisible(const psyqo::Vec3& cameraPos, const AABBCollision& collisionBox,
								   const int32_t& boundingSphereRadius) {
	int32_t cx = cameraPos.x.value;
	int32_t cy = cameraPos.y.value;
	int32_t cz = cameraPos.z.value;
	int32_t r = boundingSphereRadius;

	// reject if sphere is entirely behind near plane
	if (cz + r <= 0) {
		return false;
	}

	// clamp to near plane to avoid divide by zero
	if (cz < 1) {
		cz = 1;
	}

	// project centre onto screen
	int32_t sx = (cx * PROJECTION_DISTANCE) / cz + SCREEN_SPACE.size.x / 2;
	int32_t sy = (cy * PROJECTION_DISTANCE) / cz + SCREEN_SPACE.size.y / 2;

	// screen space radius
	int32_t sr = (r * PROJECTION_DISTANCE) / cz;

	// reject if sphere projection does not overlap viewport
	if (sx + sr < 0 || sx - sr > SCREEN_SPACE.size.x) {
		return false;
	}

	if (sy + sr < 0 || sy - sr > SCREEN_SPACE.size.y) {
		return false;
	}

	return true;
}

void Renderer::ProcessMeshQuad(const RendererQuad& quad, int8_t depth, const TimFile* texture,
							   const psyqo::PrimPieces::TPageAttr& tpage, const psyqo::Rect& uvOffset,
							   BA& bumpAllocator, OT& orderingTable) {
	// projections for each vert
	eastl::array<psyqo::Vertex, 4> projected;

	// IR0 for each vert
	uint32_t ir[4];
	uint32_t zIndex;

	auto verts = quad.boneAdjustedVerts ? quad.boneAdjustedVerts : quad.verts;

	// vertex 1->3
	for (int i = 0; i < 3; i++) {
		psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(verts[i]);
		psyqo::GTE::Kernels::rtps();
		ir[i] = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
	}

	// figure out nclip and skip rendering if backfaced
	psyqo::GTE::Kernels::nclip();
	if (psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0>() == 0) {
		return;
	}

	// read projected verts
	psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[0].packed);
	psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&projected[1].packed);
	psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&projected[2].packed);

	// read all the SZ data
	uint32_t usz0, usz1, usz2, usz3;
	psyqo::GTE::read<psyqo::GTE::Register::SZ0>(&usz0);
	psyqo::GTE::read<psyqo::GTE::Register::SZ1>(&usz1);
	psyqo::GTE::read<psyqo::GTE::Register::SZ2>(&usz2);

	// final vert
	psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(verts[3]);
	psyqo::GTE::Kernels::rtps();
	ir[3] = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
	psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&projected[3].packed);
	psyqo::GTE::read<psyqo::GTE::Register::SZ3>(&usz3);

	int32_t sz0 = (int32_t)usz0, sz1 = (int32_t)usz1, sz2 = (int32_t)usz2, sz3 = (int32_t)usz3;

	// figure out avg z index
	psyqo::GTE::Kernels::avsz4();

	// keep inside OT bounds
	zIndex = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ>();
	if (zIndex == 0 || zIndex >= ORDERING_TABLE_SIZE) {
		return;
	}

	// clip screen space
	if (quad_clip(&SCREEN_SPACE, &projected[0], &projected[1], &projected[2], &projected[3])) {
		return;
	}

	// determine if it should be sub-divided if its not got bones, and at least one edge is close enough
	auto isCloseEnoughForSubDivision =
		!quad.boneAdjustedVerts && (sz0 <= SUBDIVISION_MAX_DISTANCE || sz1 <= SUBDIVISION_MAX_DISTANCE ||
									sz2 <= SUBDIVISION_MAX_DISTANCE || sz3 <= SUBDIVISION_MAX_DISTANCE);

	// sub divide if we have depth and we're close enough
	if (depth < SUBDIVISION_MAX_DEPTH && isCloseEnoughForSubDivision) {
		auto abs = [](int32_t v) -> int32_t { return v < 0 ? -v : v; };
		auto edgeLength = [&abs](const psyqo::GTE::PackedVec3 a, const psyqo::GTE::PackedVec3 b) -> auto {
			return abs(a.x.value - b.x.value) + abs(a.y.value - b.y.value) + abs(a.z.value - b.z.value);
		};

		// make sure to measure the width and height in 3d, not just 2d.
		auto width = edgeLength(quad.verts[0], quad.verts[1]) + edgeLength(quad.verts[2], quad.verts[3]);
		auto height = edgeLength(quad.verts[0], quad.verts[2]) + edgeLength(quad.verts[1], quad.verts[3]);

		auto midVert = [](const psyqo::GTE::PackedVec3& a, const psyqo::GTE::PackedVec3& b) -> auto {
			psyqo::GTE::PackedVec3 mid;
			mid.x.value = ((a.x.value + b.x.value) >> 1);
			mid.y.value = ((a.y.value + b.y.value) >> 1);
			mid.z.value = ((a.z.value + b.z.value) >> 1);
			return mid;
		};

		auto midColour = [](const psyqo::Color& a, const psyqo::Color& b) -> auto {
			return psyqo::Color{static_cast<uint8_t>((a.r + b.r) >> 1), static_cast<uint8_t>((a.g + b.g) >> 1),
								static_cast<uint8_t>((a.b + b.b) >> 1)};
		};

		auto quadA = quad, quadB = quad;
		if (width >= height) {
			// split left right, mid points of TL (v0), TR (v1)
			psyqo::GTE::PackedVec3 midVert01 = midVert(quad.verts[0], quad.verts[1]),
								   midVert23 = midVert(quad.verts[2], quad.verts[3]);

			psyqo::PrimPieces::UVCoords midUV01, midUV23;
			midUV01.u = (quad.uvA.u + quad.uvB.u) >> 1;
			midUV01.v = (quad.uvA.v + quad.uvB.v) >> 1;
			midUV23.u = (quad.uvC.u + quad.uvD.u) >> 1;
			midUV23.v = (quad.uvC.v + quad.uvD.v) >> 1;

			psyqo::Color midColour01 = midColour(quad.colours[0], quad.colours[1]),
						 midColour23 = midColour(quad.colours[2], quad.colours[3]);

			quadA.verts[1] = midVert01;
			quadA.verts[3] = midVert23;
			quadA.uvB = midUV01;
			quadA.uvD = {midUV23.u, midUV23.v, 0};
			quadA.colours[1] = midColour01;
			quadA.colours[3] = midColour23;

			quadB.verts[0] = midVert01;
			quadB.verts[2] = midVert23;
			quadB.uvA = midUV01;
			quadB.uvC = {midUV23.u, midUV23.v, 0};
			quadB.colours[0] = midColour01;
			quadB.colours[2] = midColour23;
		} else {
			// split top/bottom, mid points of TL (v0), BL (v2)
			psyqo::GTE::PackedVec3 midVert02 = midVert(quad.verts[0], quad.verts[2]),
								   midVert13 = midVert(quad.verts[1], quad.verts[3]);

			psyqo::PrimPieces::UVCoords midUVAC, midUVBD;
			midUVAC.u = (quad.uvA.u + quad.uvC.u) >> 1;
			midUVAC.v = (quad.uvA.v + quad.uvC.v) >> 1;
			midUVBD.u = (quad.uvB.u + quad.uvD.u) >> 1;
			midUVBD.v = (quad.uvB.v + quad.uvD.v) >> 1;

			psyqo::Color midColour02 = midColour(quad.colours[0], quad.colours[2]),
						 midColour13 = midColour(quad.colours[1], quad.colours[3]);

			quadA.verts[2] = midVert02;
			quadA.verts[3] = midVert13;
			quadA.uvC = {midUVAC.u, midUVAC.v, 0};
			quadA.uvD = {midUVBD.u, midUVBD.v, 0};
			quadA.colours[2] = midColour02;
			quadA.colours[3] = midColour13;

			quadB.verts[0] = midVert02;
			quadB.verts[1] = midVert13;
			quadB.uvA = midUVAC;
			quadB.uvB = midUVBD;
			quadB.colours[0] = midColour02;
			quadB.colours[1] = midColour13;
		}

		ProcessMeshQuad(quadA, depth + 1, texture, tpage, uvOffset, bumpAllocator, orderingTable);
		ProcessMeshQuad(quadB, depth + 1, texture, tpage, uvOffset, bumpAllocator, orderingTable);
		return;
	}

	// OTHERWISE continue to process and add it to OT
	eastl::array<psyqo::Color, 4> colours;
	for (int i = 0; i < 4; i++) {
		colours[i] = quad.colours[i];
		ApplyAmbientToColour(&colours[i]);
		colours[i] = ApplyFogToColourGTE(colours[i], ir[i]);
	}

	// textured
	if (texture != nullptr) {
		auto& quadPrim = bumpAllocator.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
		quadPrim.primitive.pointA = projected[0];
		quadPrim.primitive.pointB = projected[1];
		quadPrim.primitive.pointC = projected[2];
		quadPrim.primitive.pointD = projected[3];

		quadPrim.primitive.setColorA(colours[0]);
		quadPrim.primitive.setColorB(colours[1]);
		quadPrim.primitive.setColorC(colours[2]);
		quadPrim.primitive.setColorD(colours[3]);
		quadPrim.primitive.setOpaque();

		quadPrim.primitive.tpage = tpage;

		if (texture->hasClut) {
			quadPrim.primitive.clutIndex = {texture->clutX, texture->clutY};
		}

		quadPrim.primitive.uvA = {static_cast<uint8_t>(uvOffset.pos.x + quad.uvA.u),
								  static_cast<uint8_t>(uvOffset.pos.y - quad.uvA.v)};
		quadPrim.primitive.uvB = {static_cast<uint8_t>(uvOffset.pos.x + quad.uvB.u),
								  static_cast<uint8_t>(uvOffset.pos.y - quad.uvB.v)};
		quadPrim.primitive.uvC = {static_cast<uint8_t>(uvOffset.pos.x + quad.uvC.u),
								  static_cast<uint8_t>(uvOffset.pos.y - quad.uvC.v)};
		quadPrim.primitive.uvD = {static_cast<uint8_t>(uvOffset.pos.x + quad.uvD.u),
								  static_cast<uint8_t>(uvOffset.pos.y - quad.uvD.v)};

		orderingTable.insert(quadPrim, zIndex);
	} else { // untextured
		auto& quadPrim = bumpAllocator.allocateFragment<psyqo::Prim::GouraudQuad>();
		quadPrim.primitive.pointA = projected[0];
		quadPrim.primitive.pointB = projected[1];
		quadPrim.primitive.pointC = projected[2];
		quadPrim.primitive.pointD = projected[3];

		quadPrim.primitive.setColorA(colours[0]);
		quadPrim.primitive.setColorB(colours[1]);
		quadPrim.primitive.setColorC(colours[2]);
		quadPrim.primitive.setColorD(colours[3]);
		quadPrim.primitive.setOpaque();

		orderingTable.insert(quadPrim, zIndex);
	}
}

void Renderer::ProcessMeshTri(const RendererTri& tri, int8_t depth, const TimFile* texture,
							  const psyqo::PrimPieces::TPageAttr& tpage, const psyqo::Rect& uvOffset, BA& bumpAllocator,
							  OT& orderingTable) {
	// projections for each vert
	eastl::array<psyqo::Vertex, 3> projected;

	// IR0 for each vert
	uint32_t ir[3];
	uint32_t zIndex;

	auto verts = tri.boneAdjustedVerts ? tri.boneAdjustedVerts : tri.verts;

	// vertex 1->3
	for (int i = 0; i < 3; i++) {
		psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(verts[i]);
		psyqo::GTE::Kernels::rtps();
		ir[i] = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
	}

	// figure out nclip and skip rendering if backfaced
	psyqo::GTE::Kernels::nclip();
	if (psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0>() == 0) {
		return;
	}

	// read projected verts
	psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[0].packed);
	psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&projected[1].packed);
	psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&projected[2].packed);

	// read all the SZ data
	uint32_t usz0, usz1, usz2, usz3;
	psyqo::GTE::read<psyqo::GTE::Register::SZ0>(&usz0);
	psyqo::GTE::read<psyqo::GTE::Register::SZ1>(&usz1);
	psyqo::GTE::read<psyqo::GTE::Register::SZ2>(&usz2);

	int32_t sz0 = (int32_t)usz0, sz1 = (int32_t)usz1, sz2 = (int32_t)usz2;

	// figure out avg z index
	psyqo::GTE::Kernels::avsz3();

	// keep inside OT bounds
	zIndex = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ>();
	if (zIndex == 0 || zIndex >= ORDERING_TABLE_SIZE) {
		return;
	}

	// clip screen space
	if (tri_clip(&SCREEN_SPACE, &projected[0], &projected[1], &projected[2])) {
		return;
	}

	auto isCloseEnoughForSubDivision =
		sz0 <= SUBDIVISION_MAX_DISTANCE || sz1 <= SUBDIVISION_MAX_DISTANCE || sz2 <= SUBDIVISION_MAX_DISTANCE;

	// sub divide if we have depth and we're close enough
	if (depth < SUBDIVISION_MAX_DEPTH && isCloseEnoughForSubDivision) {
		auto abs = [](int32_t v) -> int32_t { return v < 0 ? -v : v; };
		auto edgeLength = [&abs](const psyqo::GTE::PackedVec3 a, const psyqo::GTE::PackedVec3 b) -> auto {
			return abs(a.x.value - b.x.value) + abs(a.y.value - b.y.value) + abs(a.z.value - b.z.value);
		};

		// make sure to measure the width and height in 3d, not just 2d.
		auto sz01 = abs(sz0 - sz1);
		auto sz12 = abs(sz1 - sz2);
		auto sz20 = abs(sz2 - sz0);

		auto midVert = [](const psyqo::GTE::PackedVec3& a, const psyqo::GTE::PackedVec3& b) -> auto {
			psyqo::GTE::PackedVec3 mid;
			mid.x.value = ((a.x.value + b.x.value) >> 1);
			mid.y.value = ((a.y.value + b.y.value) >> 1);
			mid.z.value = ((a.z.value + b.z.value) >> 1);
			return mid;
		};

		auto midColour = [](const psyqo::Color& a, const psyqo::Color& b) -> auto {
			return psyqo::Color{static_cast<uint8_t>((a.r + b.r) >> 1), static_cast<uint8_t>((a.g + b.g) >> 1),
								static_cast<uint8_t>((a.b + b.b) >> 1)};
		};

		auto triA = tri, triB = tri;
		if (sz01 >= sz12 && sz01 >= sz20) {
			// split mid points of v0 and v1
			psyqo::GTE::PackedVec3 midVert01 = midVert(tri.verts[0], tri.verts[1]);

			psyqo::PrimPieces::UVCoords midUV01;
			midUV01.u = (tri.uvA.u + tri.uvB.u) >> 1;
			midUV01.v = (tri.uvA.v + tri.uvB.v) >> 1;

			psyqo::Color midColour01 = midColour(tri.colours[0], tri.colours[1]);

			triA.verts[1] = midVert01;
			triA.uvB = midUV01;
			triA.colours[1] = midColour01;

			triB.verts[0] = midVert01;
			triB.uvA = midUV01;
			triB.colours[0] = midColour01;
		} else if (sz12 >= sz01 && sz12 >= sz20) {
			// split mid points of v1 and v2
			psyqo::GTE::PackedVec3 midVert12 = midVert(tri.verts[1], tri.verts[2]);

			psyqo::PrimPieces::UVCoords midUV12;
			midUV12.u = (tri.uvB.u + tri.uvC.u) >> 1;
			midUV12.v = (tri.uvB.v + tri.uvC.v) >> 1;

			psyqo::Color midColour12 = midColour(tri.colours[1], tri.colours[2]);

			triA.verts[2] = midVert12;
			triA.uvC = {midUV12.u, midUV12.v, 0};
			triA.colours[2] = midColour12;

			triB.verts[1] = midVert12;
			triB.uvB = midUV12;
			triB.colours[1] = midColour12;
		} else {
			// split mid points of v0 and v2
			psyqo::GTE::PackedVec3 midVert02 = midVert(tri.verts[0], tri.verts[2]);

			psyqo::PrimPieces::UVCoords midUV02;
			midUV02.u = (tri.uvA.u + tri.uvC.u) >> 1;
			midUV02.v = (tri.uvA.v + tri.uvC.v) >> 1;

			psyqo::Color midColour02 = midColour(tri.colours[0], tri.colours[2]);

			triA.verts[2] = midVert02;
			triA.uvC = {midUV02.u, midUV02.v, 0};
			triA.colours[2] = midColour02;

			triB.verts[0] = midVert02;
			triB.uvA = midUV02;
			triB.colours[0] = midColour02;
		}

		ProcessMeshTri(triA, depth + 1, texture, tpage, uvOffset, bumpAllocator, orderingTable);
		ProcessMeshTri(triB, depth + 1, texture, tpage, uvOffset, bumpAllocator, orderingTable);
		return;
	}

	// OTHERWISE continue to process and add it to OT
	eastl::array<psyqo::Color, 3> colours;
	for (int i = 0; i < 3; i++) {
		colours[i] = tri.colours[i];
		ApplyAmbientToColour(&colours[i]);
		colours[i] = ApplyFogToColourGTE(colours[i], ir[i]);
	}

	// textured
	if (texture != nullptr) {
		auto& triPrim = bumpAllocator.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
		triPrim.primitive.pointA = projected[0];
		triPrim.primitive.pointB = projected[1];
		triPrim.primitive.pointC = projected[2];

		triPrim.primitive.setColorA(colours[0]);
		triPrim.primitive.setColorB(colours[1]);
		triPrim.primitive.setColorC(colours[2]);
		triPrim.primitive.setOpaque();

		triPrim.primitive.tpage = tpage;

		if (texture->hasClut) {
			triPrim.primitive.clutIndex = {texture->clutX, texture->clutY};
		}

		triPrim.primitive.uvA = {static_cast<uint8_t>(uvOffset.pos.x + tri.uvA.u),
								 static_cast<uint8_t>(uvOffset.pos.y - tri.uvA.v)};
		triPrim.primitive.uvB = {static_cast<uint8_t>(uvOffset.pos.x + tri.uvB.u),
								 static_cast<uint8_t>(uvOffset.pos.y - tri.uvB.v)};
		triPrim.primitive.uvC = {static_cast<uint8_t>(uvOffset.pos.x + tri.uvC.u),
								 static_cast<uint8_t>(uvOffset.pos.y - tri.uvC.v)};

		orderingTable.insert(triPrim, zIndex);
	} else { // untextured
		auto& triPrim = bumpAllocator.allocateFragment<psyqo::Prim::GouraudTriangle>();
		triPrim.primitive.pointA = projected[0];
		triPrim.primitive.pointB = projected[1];
		triPrim.primitive.pointC = projected[2];

		triPrim.primitive.setColorA(colours[0]);
		triPrim.primitive.setColorB(colours[1]);
		triPrim.primitive.setColorC(colours[2]);
		triPrim.primitive.setOpaque();

		orderingTable.insert(triPrim, zIndex);
	}
}

psyqo::FixedPoint<> Renderer::GetFogFactor(uint32_t z) {
	if (z <= NEAR_FOG_DISTANCE) {
		return 0.0_fp;
	}
	if (z >= FULL_FOG_DISTANCE) {
		return 1.0_fp;
	}

	return ((z - NEAR_FOG_DISTANCE) * 1.0_fp) / (FULL_FOG_DISTANCE - NEAR_FOG_DISTANCE);
}

void Renderer::ApplyAmbientToColour(psyqo::Color* colA) {
	auto& ambient = m_lighting->GetAmbientColour();
	colA->r = (colA->r * ambient.r) >> 7;
	colA->g = (colA->g * ambient.g) >> 7;
	colA->b = (colA->b * ambient.b) >> 7;
}

void Renderer::ApplyAmbientToColours(psyqo::Color* colA, psyqo::Color* colB, psyqo::Color* colC) {
	auto& ambient = m_lighting->GetAmbientColour();
	colA->r = (colA->r * ambient.r) >> 7;
	colA->g = (colA->g * ambient.g) >> 7;
	colA->b = (colA->b * ambient.b) >> 7;
	colB->r = (colB->r * ambient.r) >> 7;
	colB->g = (colB->g * ambient.g) >> 7;
	colB->b = (colB->b * ambient.b) >> 7;
	colC->r = (colC->r * ambient.r) >> 7;
	colC->g = (colC->g * ambient.g) >> 7;
	colC->b = (colC->b * ambient.b) >> 7;
}

void Renderer::ApplyAmbientToColours(psyqo::Color* colA, psyqo::Color* colB, psyqo::Color* colC, psyqo::Color* colD) {
	auto& ambient = m_lighting->GetAmbientColour();
	colA->r = (colA->r * ambient.r) >> 7;
	colA->g = (colA->g * ambient.g) >> 7;
	colA->b = (colA->b * ambient.b) >> 7;
	colB->r = (colB->r * ambient.r) >> 7;
	colB->g = (colB->g * ambient.g) >> 7;
	colB->b = (colB->b * ambient.b) >> 7;
	colC->r = (colC->r * ambient.r) >> 7;
	colC->g = (colC->g * ambient.g) >> 7;
	colC->b = (colC->b * ambient.b) >> 7;
	colD->r = (colD->r * ambient.r) >> 7;
	colD->g = (colD->g * ambient.g) >> 7;
	colD->b = (colD->b * ambient.b) >> 7;
}

void Renderer::ApplyFogToColour(psyqo::Color* col, psyqo::FixedPoint<> fogFactor) {
	if (!m_lighting->IsSimpleFogEnabled()) {
		return;
	}

	auto inv = 1.0_fp - fogFactor;
	auto& fog = m_lighting->GetFogColour();
	col->r = (((col->r * inv) + (fog.r * fogFactor)).value) >> 12;
	col->g = (((col->g * inv) + (fog.g * fogFactor)).value) >> 12;
	col->b = (((col->b * inv) + (fog.b * fogFactor)).value) >> 12;
}

// Interpolate from input to FC
psyqo::Color Renderer::ApplyFogToColourGTE(psyqo::Color input, uint32_t p) {
	if (!m_lighting->IsSimpleFogEnabled()) {
		return input;
	}

	psyqo::GTE::write<psyqo::GTE::Register::IR0, psyqo::GTE::Unsafe>(p);
	psyqo::GTE::write<psyqo::GTE::Register::RGB, psyqo::GTE::Safe>(input.packed);
	psyqo::GTE::Kernels::dpcs();
	return {.packed = psyqo::GTE::readRaw<psyqo::GTE::Register::RGB2>()};
}
