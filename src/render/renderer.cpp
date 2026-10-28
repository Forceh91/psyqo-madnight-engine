/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "renderer.hh"
#include "EASTL/algorithm.h"
#include "EASTL/array.h"
#include "clip.hh"

#include "../core/billboard/billboard_manager.hh"
#include "../core/debug/debug_menu.hh"
#include "../core/debug/perf_monitor.hh"
#include "../core/object/gameobject_manager.hh"
#include "../core/particles/particle_manager.hh"
#include "../defs.hh"
#include "../madnight.hh"
#include "renderer_face_types.hh"

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
	if (m_lighting->IsAdvancedFogEnabled()) {
		SetFogNearFar(0.33_fp, 1.1_fp);
	} else if (m_lighting->IsSimpleFogEnabled()) {
		SetFogColour(m_lighting->GetFogColour());
		SetFogNearFar(0.5_fp, 1.55_fp);
	}
}

void Renderer::SetFogColour(const psyqo::Color& colour) {
	m_lighting->SetFogColour(colour);
	GTESetFarColour(colour);
}

void Renderer::GTESetFarColour(const psyqo::Color& far) {
	psyqo::GTE::write<psyqo::GTE::Register::RFC, psyqo::GTE::Unsafe>(far.r << 4);
	psyqo::GTE::write<psyqo::GTE::Register::GFC, psyqo::GTE::Unsafe>(far.g << 4);
	psyqo::GTE::write<psyqo::GTE::Register::BFC, psyqo::GTE::Unsafe>(far.b << 4);
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
	auto meshStart = 0;
	for (const auto& gameObject : gameObjects) {
		// we dont need to get mesh data for every single vert since it wont change, so lets only do that once
		const auto* mesh = gameObject->mesh();
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
		TransformObjectToViewSpace(gameObject->pos(), cameraRotationMatrix, finalCameraMatrix);

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

					if (face.isQuad) {
						mesh->quads[face.rendererFaceIx].boneAdjustedVerts[j] = packedBonePos;
					} else {
						mesh->tris[face.rendererFaceIx].boneAdjustedVerts[j] = packedBonePos;
					}
				}
			}

			// mark all bones as clean
			g_madnightEngine.m_skeletonController.MarkBonesClean(mesh->skeleton);
		}

		meshStart = m_gpu.now();
		// now we've done all this we can render the mesh and apply texture (if needed)
		// we dont need to get texture data for every single vert since it wont change, so lets only do that once
		// if its not a nullptr fill out some data so we don't have to do it every face
		const auto texture = gameObject->texture();
		if (texture) {
			// get the tpage and uv offset info
			tpage = g_madnightEngine.m_textureManager.GetTPageAttr(texture);
			if (m_lighting->IsAdvancedFogEnabled()) {
				tpage.set(psyqo::Prim::TPageAttr::FullBackAndFullFront);
			}
			offset = g_madnightEngine.m_textureManager.GetTPageUVForTim(texture);
			offset.pos.y += (texture->height - 1);
		}

		const auto& quickAdvancedFog =
			m_lighting->IsAdvancedFogEnabled() && gameObject->HasRenderFlag(RF_QUICK_ADVANCED_FOG);
		const FaceData faceData = {texture,			tpage, offset, allocator, ot, m_lighting->IsAdvancedFogEnabled(),
								   quickAdvancedFog};

		// if we're doing a character then quick advanced fog is used (stp untextured quad on top)
		if (quickAdvancedFog) {
			m_quickAdvancedFogOverlayQuad = nullptr;

			// overlay
			m_quickAdvancedFogOverlayQuad = &faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudQuad>();
			m_quickAdvancedFogOverlayQuad->primitive.setSemiTrans();
			m_quickAdvancedFogSmallestZ = ORDERING_TABLE_SIZE - 1;

			m_ovMinX = INT16_MAX, m_ovMinY = INT16_MAX, m_ovMaxX = -INT16_MAX, m_ovMaxY = -INT16_MAX;
		}

		for (int32_t i = 0; i < mesh->facesCount; i++) {
			const auto& rendererFace = mesh->facesData[i];

			if (rendererFace.isQuad) {
				ProcessMeshQuad(mesh->quads[rendererFace.rendererFaceIx], 0, faceData);
			} else {
				ProcessMeshTri(mesh->tris[rendererFace.rendererFaceIx], 0, faceData);
			}
		}

		if (quickAdvancedFog && m_quickAdvancedFogOverlayQuad && m_ovMinX != INT16_MAX && m_ovMinX < m_ovMaxX) {
			auto& reset = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
			reset.primitive = {psyqo::Prim::MaskControl::FromSource};
			faceData.orderingTable.insert(reset, m_quickAdvancedFogSmallestZ - 1);

			m_quickAdvancedFogOverlayQuad->primitive.pointA = psyqo::Vertex{m_ovMinX, m_ovMinY};
			m_quickAdvancedFogOverlayQuad->primitive.pointB = psyqo::Vertex{m_ovMaxX, m_ovMinY};
			m_quickAdvancedFogOverlayQuad->primitive.pointC = psyqo::Vertex{m_ovMinX, m_ovMaxY};
			m_quickAdvancedFogOverlayQuad->primitive.pointD = psyqo::Vertex{m_ovMaxX, m_ovMaxY};

			auto& fog = m_lighting->GetFogColour();
			psyqo::Color add = {uint8_t((fog.r * m_quickFogT) >> 12), uint8_t((fog.g * m_quickFogT) >> 12),
								uint8_t((fog.b * m_quickFogT) >> 12)};

			m_quickAdvancedFogOverlayQuad->primitive.setColorA(add);
			m_quickAdvancedFogOverlayQuad->primitive.setColorB(add);
			m_quickAdvancedFogOverlayQuad->primitive.setColorC(add);
			m_quickAdvancedFogOverlayQuad->primitive.setColorD(add);

			faceData.orderingTable.insert(*m_quickAdvancedFogOverlayQuad, m_quickAdvancedFogSmallestZ - 1);

			auto& overlayTpage = faceData.bumpAllocator.allocateFragment<psyqo::Prim::TPage>();
			overlayTpage.primitive.attr = tpage;
			overlayTpage.primitive.attr.set(psyqo::Prim::TPageAttr::FullBackAndFullFront);
			faceData.orderingTable.insert(overlayTpage, m_quickAdvancedFogSmallestZ - 1);

			auto& test = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
			test.primitive = {psyqo::Prim::MaskControl::FromSource, psyqo::Prim::MaskControl::Test::Yes};
			faceData.orderingTable.insert(test, m_quickAdvancedFogSmallestZ - 1);
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

	m_gpu.pumpCallbacks();
	auto delta = m_gpu.now() - meshStart;
	auto avg = delta / renderedObjects;
	// printf("processing %i objects took %ius (avg. %ius) to complete\n", renderedObjects, delta, avg);

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

				// keep it within the OT. we dont have all sz points written here so we need to derive it ourselves
				auto sz = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ3>();
				zIndex = ((ORDERING_TABLE_SIZE / 3) * 3 * sz) >> 12;
				if (zIndex == 0 || zIndex >= ORDERING_TABLE_SIZE) {
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

void Renderer::ProcessMeshQuad(const RendererQuad& quad, int8_t depth, const FaceData& faceData) {
	// projections for each vert
	eastl::array<psyqo::Vertex, 4> projected;
	FaceNode node;

	auto* verts = quad.boneAdjustedVerts ? quad.boneAdjustedVerts : quad.verts;

	// vertex 1->3
	for (int i = 0; i < 3; i++) {
		psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(verts[i]);
		psyqo::GTE::Kernels::rtps();
		node.irs[i] = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
	}

	// figure out nclip and skip rendering if backfaced
	psyqo::GTE::Kernels::nclip();
	if ((int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0>() == 0) {
		return;
	}

	// read projected verts
	psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&node.projected[0].packed);
	psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&node.projected[1].packed);
	psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&node.projected[2].packed);

	// final vert
	psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(verts[3]);
	psyqo::GTE::Kernels::rtps();
	node.irs[3] = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
	psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&node.projected[3].packed);

	// read all the SZ data
	psyqo::GTE::read<psyqo::GTE::Register::SZ0>(&node.projectedDepths[0]);
	psyqo::GTE::read<psyqo::GTE::Register::SZ1>(&node.projectedDepths[1]);
	psyqo::GTE::read<psyqo::GTE::Register::SZ2>(&node.projectedDepths[2]);
	psyqo::GTE::read<psyqo::GTE::Register::SZ3>(&node.projectedDepths[3]);

	// we can skip a node if its going to be fully obscured by fog anyway
	if (faceData.isAdvancedFog && node.irs[0] >= 4096 && node.irs[1] >= 4096 && node.irs[2] >= 4096 &&
		node.irs[3] >= 4096) {
		return;
	}

	auto shouldSubDivide =
		!quad.boneAdjustedVerts &&
		(node.projectedDepths[0] >= SUBDIVISION_MIN_DISTANCE || node.projectedDepths[1] >= SUBDIVISION_MIN_DISTANCE ||
		 node.projectedDepths[2] >= SUBDIVISION_MIN_DISTANCE || node.projectedDepths[3] >= SUBDIVISION_MIN_DISTANCE) &&
		(node.projectedDepths[0] <= SUBDIVISION_MAX_DISTANCE || node.projectedDepths[1] <= SUBDIVISION_MAX_DISTANCE ||
		 node.projectedDepths[2] <= SUBDIVISION_MAX_DISTANCE || node.projectedDepths[3] <= SUBDIVISION_MAX_DISTANCE);

	if (shouldSubDivide) {
		// copy any remaining data we couldn't read straight from the GTE
		for (int i = 0; i < 4; i++) {
			node.verts[i] = quad.verts[i];
			node.colours[i] = quad.colours[i];
		}

		node.uvA = quad.uvA;
		node.uvB = quad.uvB;
		node.uvC = quad.uvC;
		node.uvD = quad.uvD;

		ProcessQuadNode(node, depth, faceData);
	} else {
		// it won't be sub divided so just emit it out
		// figure out avg z index
		psyqo::GTE::Kernels::avsz4();

		// keep inside OT bounds
		uint32_t zIndex = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ>();
		if (zIndex == 0 || zIndex >= ORDERING_TABLE_SIZE) {
			return;
		}

		// clip screen space
		if (quad_clip(&SCREEN_SPACE, &node.projected[0], &node.projected[1], &node.projected[2], &node.projected[3])) {
			return;
		}

		// apply fog colours
		eastl::array<psyqo::Color, 4> vertexColours = {quad.colours[0], quad.colours[1], quad.colours[2],
													   quad.colours[3]};
		eastl::array<psyqo::Color, 4> fogColours;

		if (!faceData.isAdvancedFog) {
			for (int i = 0; i < 4; i++) {
				ApplyAmbientToColour(&vertexColours[i]);
				vertexColours[i] = ApplyFogToColourGTE(vertexColours[i], node.irs[i]);
			}
		} else {
			if (!faceData.useQuickAdvancedFogPass) {
				GTESetFarColour({0, 0, 0});
				for (int i = 0; i < 4; i++) {
					ApplyAmbientToColour(&vertexColours[i]);
					vertexColours[i] = GTEColourLerp(vertexColours[i], node.irs[i]);
				}

				GTESetFarColour(m_lighting->GetFogColour());
				for (int i = 0; i < 4; i++) {
					fogColours[i] = GTEColourLerp({0, 0, 0}, node.irs[i]);
				}
			} else {
				for (int i = 0; i < 4; i++) {
					auto density = 4096 - node.irs[i];
					ApplyAmbientToColour(&node.colours[i]);
					node.colours[i] = {uint8_t((node.colours[i].r * density) >> 12),
									   uint8_t((node.colours[i].g * density) >> 12),
									   uint8_t((node.colours[i].b * density) >> 12)};
					m_quickFogT += node.irs[i];
				}

				// average the quick fog t scale
				m_quickFogT /= 4;
			}
		}

		bool isAdvancedFog = faceData.isAdvancedFog &&
							 (node.irs[0] > 0 || node.irs[1] > 0 || node.irs[2] > 0 || node.irs[3] > 0) &&
							 !faceData.useQuickAdvancedFogPass;

		// update overlay min/max points
		if (faceData.isAdvancedFog && faceData.useQuickAdvancedFogPass) {
			for (int i = 0; i < 4; i++) {
				m_ovMinX = eastl::min(m_ovMinX, node.projected[i].x);
				m_ovMinY = eastl::min(m_ovMinY, node.projected[i].y);
				m_ovMaxX = eastl::max(m_ovMaxX, node.projected[i].x);
				m_ovMaxY = eastl::max(m_ovMaxY, node.projected[i].y);
			}

			m_quickAdvancedFogSmallestZ = eastl::min(m_quickAdvancedFogSmallestZ, zIndex);
		}

		if (faceData.texture != nullptr) {
			if (!isAdvancedFog || !(faceData.texture->flags & (1 << 0))) {
				auto& quadPrim = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
				quadPrim.primitive.pointA = node.projected[0];
				quadPrim.primitive.pointB = node.projected[1];
				quadPrim.primitive.pointC = node.projected[2];
				quadPrim.primitive.pointD = node.projected[3];

				quadPrim.primitive.setColorA(vertexColours[0]);
				quadPrim.primitive.setColorB(vertexColours[1]);
				quadPrim.primitive.setColorC(vertexColours[2]);
				quadPrim.primitive.setColorD(vertexColours[3]);
				if (!isAdvancedFog) {
					quadPrim.primitive.setOpaque();
				} else {
					quadPrim.primitive.setSemiTrans();
				}

				quadPrim.primitive.tpage = faceData.tpage;

				if (faceData.texture->hasClut) {
					quadPrim.primitive.clutIndex = {faceData.texture->clutX, faceData.texture->clutY};
				}

				quadPrim.primitive.uvA = {static_cast<uint8_t>(faceData.uvOffset.pos.x + quad.uvA.u),
										  static_cast<uint8_t>(faceData.uvOffset.pos.y - quad.uvA.v)};
				quadPrim.primitive.uvB = {static_cast<uint8_t>(faceData.uvOffset.pos.x + quad.uvB.u),
										  static_cast<uint8_t>(faceData.uvOffset.pos.y - quad.uvB.v)};
				quadPrim.primitive.uvC = {static_cast<uint8_t>(faceData.uvOffset.pos.x + quad.uvC.u),
										  static_cast<uint8_t>(faceData.uvOffset.pos.y - quad.uvC.v)};
				quadPrim.primitive.uvD = {static_cast<uint8_t>(faceData.uvOffset.pos.x + quad.uvD.u),
										  static_cast<uint8_t>(faceData.uvOffset.pos.y - quad.uvD.v)};

				faceData.orderingTable.insert(quadPrim, zIndex);

				// we insert the untextured fog prim last because `insert` puts each new prim at the front of the
				// bucket.
				if (isAdvancedFog) {
					auto& quadFogPrim = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudQuad>();
					quadFogPrim.primitive.pointA = node.projected[0];
					quadFogPrim.primitive.pointB = node.projected[1];
					quadFogPrim.primitive.pointC = node.projected[2];
					quadFogPrim.primitive.pointD = node.projected[3];

					quadFogPrim.primitive.setColorA(fogColours[0]);
					quadFogPrim.primitive.setColorB(fogColours[1]);
					quadFogPrim.primitive.setColorC(fogColours[2]);
					quadFogPrim.primitive.setColorD(fogColours[3]);
					quadFogPrim.primitive.setOpaque();

					faceData.orderingTable.insert(quadFogPrim, zIndex);
				}
			} else {
				// when we're handling advanced fog for a textured primitive that has holes in
				// we need to: setup the tpage, force set mask, set a marker (black verts),
				// set mask control to from source, set the textured prim,
				// do another from source (Test=1), do the fog, and finally a reset
				// we insert this into the OT in reverse order.
				auto& reset = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
				reset.primitive = {psyqo::Prim::MaskControl::FromSource};
				faceData.orderingTable.insert(reset, zIndex);

				auto& fog = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudQuad>();
				fog.primitive.pointA = node.projected[0];
				fog.primitive.pointB = node.projected[1];
				fog.primitive.pointC = node.projected[2];
				fog.primitive.pointD = node.projected[3];

				fog.primitive.setColorA(fogColours[0]);
				fog.primitive.setColorB(fogColours[1]);
				fog.primitive.setColorC(fogColours[2]);
				fog.primitive.setColorD(fogColours[3]);
				fog.primitive.setSemiTrans();
				faceData.orderingTable.insert(fog, zIndex);

				auto& test = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
				test.primitive = {psyqo::Prim::MaskControl::FromSource, psyqo::Prim::MaskControl::Test::Yes};
				faceData.orderingTable.insert(test, zIndex);

				auto& textured = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
				textured.primitive.pointA = node.projected[0];
				textured.primitive.pointB = node.projected[1];
				textured.primitive.pointC = node.projected[2];
				textured.primitive.pointD = node.projected[3];

				textured.primitive.setColorA(vertexColours[0]);
				textured.primitive.setColorB(vertexColours[1]);
				textured.primitive.setColorC(vertexColours[2]);
				textured.primitive.setColorD(vertexColours[3]);
				textured.primitive.setOpaque();

				textured.primitive.tpage = faceData.tpage;
				if (faceData.texture->hasClut) {
					textured.primitive.clutIndex = {faceData.texture->clutX, faceData.texture->clutY};
				}

				textured.primitive.uvA = {static_cast<uint8_t>(faceData.uvOffset.pos.x + quad.uvA.u),
										  static_cast<uint8_t>(faceData.uvOffset.pos.y - quad.uvA.v)};
				textured.primitive.uvB = {static_cast<uint8_t>(faceData.uvOffset.pos.x + quad.uvB.u),
										  static_cast<uint8_t>(faceData.uvOffset.pos.y - quad.uvB.v)};
				textured.primitive.uvC = {static_cast<uint8_t>(faceData.uvOffset.pos.x + quad.uvC.u),
										  static_cast<uint8_t>(faceData.uvOffset.pos.y - quad.uvC.v)};
				textured.primitive.uvD = {static_cast<uint8_t>(faceData.uvOffset.pos.x + quad.uvD.u),
										  static_cast<uint8_t>(faceData.uvOffset.pos.y - quad.uvD.v)};
				faceData.orderingTable.insert(textured, zIndex);

				auto& fromSource = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
				fromSource.primitive = {psyqo::Prim::MaskControl::FromSource};
				faceData.orderingTable.insert(fromSource, zIndex);

				auto& marker = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudQuad>();
				marker.primitive.pointA = node.projected[0];
				marker.primitive.pointB = node.projected[1];
				marker.primitive.pointC = node.projected[2];
				marker.primitive.pointD = node.projected[3];

				marker.primitive.setColorA({
					0,
					0,
					0,
				});
				marker.primitive.setColorB({
					0,
					0,
					0,
				});
				marker.primitive.setColorC({
					0,
					0,
					0,
				});
				marker.primitive.setColorD({
					0,
					0,
					0,
				});
				marker.primitive.setSemiTrans();
				faceData.orderingTable.insert(marker, zIndex);

				auto& forceSet = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
				forceSet.primitive = {psyqo::Prim::MaskControl::ForceSet};
				faceData.orderingTable.insert(forceSet, zIndex);

				auto& tpage = faceData.bumpAllocator.allocateFragment<psyqo::Prim::TPage>();
				tpage.primitive.attr = faceData.tpage;
				faceData.orderingTable.insert(tpage, zIndex);
			}
		} else { // untextured
			auto& quadPrim = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudQuad>();
			quadPrim.primitive.pointA = node.projected[0];
			quadPrim.primitive.pointB = node.projected[1];
			quadPrim.primitive.pointC = node.projected[2];
			quadPrim.primitive.pointD = node.projected[3];

			quadPrim.primitive.setColorA(vertexColours[0]);
			quadPrim.primitive.setColorB(vertexColours[1]);
			quadPrim.primitive.setColorC(vertexColours[2]);
			quadPrim.primitive.setColorD(vertexColours[3]);
			quadPrim.primitive.setOpaque();

			faceData.orderingTable.insert(quadPrim, zIndex);
		}
	}
}

void Renderer::ProcessQuadNode(FaceNode& node, uint8_t subDivsionDepth, const FaceData& faceData) {
	// depth == 0 is handled by `ProcessMeshQuad`
	if (subDivsionDepth > 0) {
		// write the projected values back into sxy*
		psyqo::GTE::write<psyqo::GTE::Register::SXY0, psyqo::GTE::Unsafe>(node.projected[0].packed);
		psyqo::GTE::write<psyqo::GTE::Register::SXY1, psyqo::GTE::Unsafe>(node.projected[1].packed);
		psyqo::GTE::write<psyqo::GTE::Register::SXY2, psyqo::GTE::Unsafe>(node.projected[2].packed);

		// write the projected values back into sz*
		psyqo::GTE::write<psyqo::GTE::Register::SZ0, psyqo::GTE::Unsafe>(node.projectedDepths[0]);
		psyqo::GTE::write<psyqo::GTE::Register::SZ1, psyqo::GTE::Unsafe>(node.projectedDepths[1]);
		psyqo::GTE::write<psyqo::GTE::Register::SZ2, psyqo::GTE::Safe>(node.projectedDepths[2]);

		// figure out nclip and skip rendering if backfaced
		psyqo::GTE::Kernels::nclip();
		if ((int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0>() == 0) {
			return;
		}

		psyqo::GTE::write<psyqo::GTE::Register::SXY2, psyqo::GTE::Unsafe>(node.projected[3].packed);
		psyqo::GTE::write<psyqo::GTE::Register::SZ3, psyqo::GTE::Safe>(node.projectedDepths[3]);
	}

	// figure out avg z index
	psyqo::GTE::Kernels::avsz4();

	// keep inside OT bounds
	uint32_t zIndex = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ>();
	if (zIndex == 0 || zIndex >= ORDERING_TABLE_SIZE) {
		return;
	}

	// clip screen space
	if (quad_clip(&SCREEN_SPACE, &node.projected[0], &node.projected[1], &node.projected[2], &node.projected[3])) {
		return;
	}

	// if its the parent then do colour work that can be propogated
	if (subDivsionDepth == 0) {
		// apply fog colours (skipped if advanced fog is enabled)
		if (!faceData.isAdvancedFog) {
			for (int i = 0; i < 4; i++) {
				ApplyAmbientToColour(&node.colours[i]);
				node.colours[i] = ApplyFogToColourGTE(node.colours[i], node.irs[i]);
			}
		} else {
			if (!faceData.useQuickAdvancedFogPass) {
				GTESetFarColour({0, 0, 0});
				for (int i = 0; i < 4; i++) {
					ApplyAmbientToColour(&node.colours[i]);
					node.colours[i] = GTEColourLerp(node.colours[i], node.irs[i]);
				}

				GTESetFarColour(m_lighting->GetFogColour());
				for (int i = 0; i < 4; i++) {
					node.fogColours[i] = GTEColourLerp({0, 0, 0}, node.irs[i]);
				}
			} else {
				for (int i = 0; i < 4; i++) {
					auto density = 4096 - node.irs[i];
					ApplyAmbientToColour(&node.colours[i]);
					node.colours[i] = {uint8_t((node.colours[i].r * density) >> 12),
									   uint8_t((node.colours[i].g * density) >> 12),
									   uint8_t((node.colours[i].b * density) >> 12)};
					m_quickFogT += node.irs[i];
				}

				// average the quick fog t scale
				m_quickFogT /= 4;
			}
		}
	}

	// sub divide if we have depth and we're close enough
	if (subDivsionDepth <= SUBDIVISION_MAX_DEPTH) {
		auto abs = [](int32_t v) -> int32_t { return v < 0 ? -v : v; };
		auto edgeLength = [&abs](const psyqo::GTE::PackedVec3 a, const psyqo::GTE::PackedVec3 b) -> auto {
			return abs(a.x.value - b.x.value) + abs(a.y.value - b.y.value) + abs(a.z.value - b.z.value);
		};

		// make sure to measure the width and height in 3d, not just 2d.
		auto width = edgeLength(node.verts[0], node.verts[1]) + edgeLength(node.verts[2], node.verts[3]);
		auto height = edgeLength(node.verts[0], node.verts[2]) + edgeLength(node.verts[1], node.verts[3]);

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

		auto midIR = [](const uint32_t& a, const uint32_t& b) -> auto { return (a + b) >> 1; };

		FaceNode quadA, quadB;
		if (width >= height) {
			// split left right, mid points of TL (v0), TR (v1)
			psyqo::GTE::PackedVec3 midVert01 = midVert(node.verts[0], node.verts[1]),
								   midVert23 = midVert(node.verts[2], node.verts[3]);

			psyqo::Vertex midProjectedVert01 = {}, midProjectedVert23 = {};
			uint32_t midIR01 = 0, midIR23 = 0;
			uint32_t midZ01 = 0, midZ23 = 0;

			psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(midVert01);
			psyqo::GTE::Kernels::rtps();
			midIR01 = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
			psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&midProjectedVert01.packed);
			psyqo::GTE::read<psyqo::GTE::Register::SZ3>(&midZ01);

			psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(midVert23);
			psyqo::GTE::Kernels::rtps();
			midIR23 = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
			psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&midProjectedVert23.packed);
			psyqo::GTE::read<psyqo::GTE::Register::SZ3>(&midZ23);

			psyqo::PrimPieces::UVCoords midUV01, midUV23;
			midUV01.u = (node.uvA.u + node.uvB.u) >> 1;
			midUV01.v = (node.uvA.v + node.uvB.v) >> 1;
			midUV23.u = (node.uvC.u + node.uvD.u) >> 1;
			midUV23.v = (node.uvC.v + node.uvD.v) >> 1;

			psyqo::Color midColour01 = midColour(node.colours[0], node.colours[1]),
						 midColour23 = midColour(node.colours[2], node.colours[3]);

			psyqo::Color midFogColour01 = midColour(node.fogColours[0], node.fogColours[1]),
						 midFogColour23 = midColour(node.fogColours[2], node.fogColours[3]);

			// quad A
			quadA.verts[0] = node.verts[0];
			quadA.verts[1] = midVert01;
			quadA.verts[2] = node.verts[2];
			quadA.verts[3] = midVert23;

			quadA.projected[0] = node.projected[0];
			quadA.projected[1] = midProjectedVert01;
			quadA.projected[2] = node.projected[2];
			quadA.projected[3] = midProjectedVert23;

			quadA.projectedDepths[0] = node.projectedDepths[0];
			quadA.projectedDepths[1] = midZ01;
			quadA.projectedDepths[2] = node.projectedDepths[2];
			quadA.projectedDepths[3] = midZ23;

			quadA.uvA = node.uvA;
			quadA.uvB = midUV01;
			quadA.uvC = node.uvC;
			quadA.uvD = {midUV23.u, midUV23.v, 0};

			quadA.colours[0] = node.colours[0];
			quadA.colours[1] = midColour01;
			quadA.colours[2] = node.colours[2];
			quadA.colours[3] = midColour23;

			quadA.fogColours[0] = node.fogColours[0];
			quadA.fogColours[1] = midFogColour01;
			quadA.fogColours[2] = node.fogColours[2];
			quadA.fogColours[3] = midFogColour23;

			quadA.irs[0] = node.irs[0];
			quadA.irs[1] = midIR01;
			quadA.irs[2] = node.irs[2];
			quadA.irs[3] = midIR23;

			// quadB
			quadB.verts[0] = midVert01;
			quadB.verts[1] = node.verts[1];
			quadB.verts[2] = midVert23;
			quadB.verts[3] = node.verts[3];

			quadB.projected[0] = midProjectedVert01;
			quadB.projected[1] = node.projected[1];
			quadB.projected[2] = midProjectedVert23;
			quadB.projected[3] = node.projected[3];

			quadB.projectedDepths[0] = midZ01;
			quadB.projectedDepths[1] = node.projectedDepths[1];
			quadB.projectedDepths[2] = midZ23;
			quadB.projectedDepths[3] = node.projectedDepths[3];

			quadB.uvA = midUV01;
			quadB.uvB = node.uvB;
			quadB.uvC = {midUV23.u, midUV23.v, 0};
			quadB.uvD = node.uvD;

			quadB.colours[0] = midColour01;
			quadB.colours[1] = node.colours[1];
			quadB.colours[2] = midColour23;
			quadB.colours[3] = node.colours[3];

			quadB.fogColours[0] = midFogColour01;
			quadB.fogColours[1] = node.fogColours[1];
			quadB.fogColours[2] = midFogColour23;
			quadB.fogColours[3] = node.fogColours[3];

			quadB.irs[0] = midIR01;
			quadB.irs[1] = node.irs[1];
			quadB.irs[2] = midIR23;
			quadB.irs[3] = node.irs[3];
			// end quadB
		} else {
			// split top/bottom, mid points of TL (v0), BL (v2)
			psyqo::GTE::PackedVec3 midVert02 = midVert(node.verts[0], node.verts[2]),
								   midVert13 = midVert(node.verts[1], node.verts[3]);

			psyqo::Vertex midProjectedVert02 = {}, midProjectedVert13 = {};
			uint32_t midIR02 = 0, midIR13 = 0;
			uint32_t midZ02 = 0, midZ13 = 0;

			psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(midVert02);
			psyqo::GTE::Kernels::rtps();
			midIR02 = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
			psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&midProjectedVert02.packed);
			psyqo::GTE::read<psyqo::GTE::Register::SZ3>(&midZ02);

			psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(midVert13);
			psyqo::GTE::Kernels::rtps();
			midIR13 = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
			psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&midProjectedVert13.packed);
			psyqo::GTE::read<psyqo::GTE::Register::SZ3>(&midZ13);

			psyqo::PrimPieces::UVCoords midUVAC, midUVBD;
			midUVAC.u = (node.uvA.u + node.uvC.u) >> 1;
			midUVAC.v = (node.uvA.v + node.uvC.v) >> 1;
			midUVBD.u = (node.uvB.u + node.uvD.u) >> 1;
			midUVBD.v = (node.uvB.v + node.uvD.v) >> 1;

			psyqo::Color midColour02 = midColour(node.colours[0], node.colours[2]),
						 midColour13 = midColour(node.colours[1], node.colours[3]);

			psyqo::Color midFogColour02 = midColour(node.fogColours[0], node.fogColours[2]),
						 midFogColour13 = midColour(node.fogColours[1], node.fogColours[3]);

			// quadA
			quadA.verts[0] = node.verts[0];
			quadA.verts[1] = node.verts[1];
			quadA.verts[2] = midVert02;
			quadA.verts[3] = midVert13;

			quadA.projected[0] = node.projected[0];
			quadA.projected[1] = node.projected[1];
			quadA.projected[2] = midProjectedVert02;
			quadA.projected[3] = midProjectedVert13;

			quadA.projectedDepths[0] = node.projectedDepths[0];
			quadA.projectedDepths[1] = node.projectedDepths[1];
			quadA.projectedDepths[2] = midZ02;
			quadA.projectedDepths[3] = midZ13;

			quadA.uvA = node.uvA;
			quadA.uvB = node.uvB;
			quadA.uvC = {midUVAC.u, midUVAC.v, 0};
			quadA.uvD = {midUVBD.u, midUVBD.v, 0};

			quadA.colours[0] = node.colours[0];
			quadA.colours[1] = node.colours[1];
			quadA.colours[2] = midColour02;
			quadA.colours[3] = midColour13;

			quadA.fogColours[0] = node.fogColours[0];
			quadA.fogColours[1] = node.fogColours[1];
			quadA.fogColours[2] = midFogColour02;
			quadA.fogColours[3] = midFogColour13;

			quadA.irs[0] = node.irs[0];
			quadA.irs[1] = node.irs[1];
			quadA.irs[2] = midIR02;
			quadA.irs[3] = midIR13;

			// quadB
			quadB.verts[0] = midVert02;
			quadB.verts[1] = midVert13;
			quadB.verts[2] = node.verts[2];
			quadB.verts[3] = node.verts[3];

			quadB.projected[0] = midProjectedVert02;
			quadB.projected[1] = midProjectedVert13;
			quadB.projected[2] = node.projected[2];
			quadB.projected[3] = node.projected[3];

			quadB.projectedDepths[0] = midZ02;
			quadB.projectedDepths[1] = midZ13;
			quadB.projectedDepths[2] = node.projectedDepths[2];
			quadB.projectedDepths[3] = node.projectedDepths[3];

			quadB.uvA = midUVAC;
			quadB.uvB = midUVBD;
			quadB.uvC = node.uvC;
			quadB.uvD = node.uvD;

			quadB.colours[0] = midColour02;
			quadB.colours[1] = midColour13;
			quadB.colours[2] = node.colours[2];
			quadB.colours[3] = node.colours[3];

			quadB.fogColours[0] = midFogColour02;
			quadB.fogColours[1] = midFogColour13;
			quadB.fogColours[2] = node.fogColours[2];
			quadB.fogColours[3] = node.fogColours[3];

			quadB.irs[0] = midIR02;
			quadB.irs[1] = midIR13;
			quadB.irs[2] = node.irs[2];
			quadB.irs[3] = node.irs[3];
			// end quadB
		}

		ProcessQuadNode(quadA, subDivsionDepth + 1, faceData);
		ProcessQuadNode(quadB, subDivsionDepth + 1, faceData);
		return;
	} // OTHERWISE continue to process and add to OT

	bool isAdvancedFog = faceData.isAdvancedFog &&
						 (node.irs[0] > 0 || node.irs[1] > 0 || node.irs[2] > 0 || node.irs[3] > 0) &&
						 !faceData.useQuickAdvancedFogPass;

	// update overlay min/max points
	if (faceData.isAdvancedFog && faceData.useQuickAdvancedFogPass) {
		for (int i = 0; i < 4; i++) {
			m_ovMinX = eastl::min(m_ovMinX, node.projected[i].x);
			m_ovMinY = eastl::min(m_ovMinY, node.projected[i].y);
			m_ovMaxX = eastl::max(m_ovMaxX, node.projected[i].x);
			m_ovMaxY = eastl::max(m_ovMaxY, node.projected[i].y);
		}
		m_quickAdvancedFogSmallestZ = eastl::min(m_quickAdvancedFogSmallestZ, zIndex);
	}

	// textured
	if (faceData.texture != nullptr) {
		if (!isAdvancedFog || !(faceData.texture->flags & (1 << 0))) {
			auto& quadPrim = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
			quadPrim.primitive.pointA = node.projected[0];
			quadPrim.primitive.pointB = node.projected[1];
			quadPrim.primitive.pointC = node.projected[2];
			quadPrim.primitive.pointD = node.projected[3];

			quadPrim.primitive.setColorA(node.colours[0]);
			quadPrim.primitive.setColorB(node.colours[1]);
			quadPrim.primitive.setColorC(node.colours[2]);
			quadPrim.primitive.setColorD(node.colours[3]);
			if (!isAdvancedFog) {
				quadPrim.primitive.setOpaque();
			} else {
				quadPrim.primitive.setSemiTrans();
			}

			quadPrim.primitive.tpage = faceData.tpage;

			if (faceData.texture->hasClut) {
				quadPrim.primitive.clutIndex = {faceData.texture->clutX, faceData.texture->clutY};
			}

			quadPrim.primitive.uvA = {static_cast<uint8_t>(faceData.uvOffset.pos.x + node.uvA.u),
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - node.uvA.v)};
			quadPrim.primitive.uvB = {static_cast<uint8_t>(faceData.uvOffset.pos.x + node.uvB.u),
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - node.uvB.v)};
			quadPrim.primitive.uvC = {static_cast<uint8_t>(faceData.uvOffset.pos.x + node.uvC.u),
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - node.uvC.v)};
			quadPrim.primitive.uvD = {static_cast<uint8_t>(faceData.uvOffset.pos.x + node.uvD.u),
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - node.uvD.v)};

			faceData.orderingTable.insert(quadPrim, zIndex);

			// we insert the untextured fog prim last because `insert` puts each new prim at the front of the bucket.
			if (isAdvancedFog) {
				auto& quadFogPrim = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudQuad>();
				quadFogPrim.primitive.pointA = node.projected[0];
				quadFogPrim.primitive.pointB = node.projected[1];
				quadFogPrim.primitive.pointC = node.projected[2];
				quadFogPrim.primitive.pointD = node.projected[3];

				quadFogPrim.primitive.setColorA(node.fogColours[0]);
				quadFogPrim.primitive.setColorB(node.fogColours[1]);
				quadFogPrim.primitive.setColorC(node.fogColours[2]);
				quadFogPrim.primitive.setColorD(node.fogColours[3]);
				quadFogPrim.primitive.setOpaque();

				faceData.orderingTable.insert(quadFogPrim, zIndex);
			}
		} else {
			// when we're handling advanced fog for a textured primitive that has holes in
			// we need to: setup the tpage, force set mask, set a marker (black verts),
			// set mask control to from source, set the textured prim,
			// do another from source (Test=1), do the fog, and finally a reset
			// we insert this into the OT in reverse order.
			auto& reset = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
			reset.primitive = {psyqo::Prim::MaskControl::FromSource};
			faceData.orderingTable.insert(reset, zIndex);

			auto& fog = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudQuad>();
			fog.primitive.pointA = node.projected[0];
			fog.primitive.pointB = node.projected[1];
			fog.primitive.pointC = node.projected[2];
			fog.primitive.pointD = node.projected[3];

			fog.primitive.setColorA(node.fogColours[0]);
			fog.primitive.setColorB(node.fogColours[1]);
			fog.primitive.setColorC(node.fogColours[2]);
			fog.primitive.setColorD(node.fogColours[3]);
			fog.primitive.setSemiTrans();
			faceData.orderingTable.insert(fog, zIndex);

			auto& test = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
			test.primitive = {psyqo::Prim::MaskControl::FromSource, psyqo::Prim::MaskControl::Test::Yes};
			faceData.orderingTable.insert(test, zIndex);

			auto& textured = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
			textured.primitive.pointA = node.projected[0];
			textured.primitive.pointB = node.projected[1];
			textured.primitive.pointC = node.projected[2];
			textured.primitive.pointD = node.projected[3];

			textured.primitive.setColorA(node.colours[0]);
			textured.primitive.setColorB(node.colours[1]);
			textured.primitive.setColorC(node.colours[2]);
			textured.primitive.setColorD(node.colours[3]);
			textured.primitive.setOpaque();

			textured.primitive.tpage = faceData.tpage;
			if (faceData.texture->hasClut) {
				textured.primitive.clutIndex = {faceData.texture->clutX, faceData.texture->clutY};
			}

			textured.primitive.uvA = {static_cast<uint8_t>(faceData.uvOffset.pos.x + node.uvA.u),
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - node.uvA.v)};
			textured.primitive.uvB = {static_cast<uint8_t>(faceData.uvOffset.pos.x + node.uvB.u),
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - node.uvB.v)};
			textured.primitive.uvC = {static_cast<uint8_t>(faceData.uvOffset.pos.x + node.uvC.u),
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - node.uvC.v)};
			textured.primitive.uvD = {static_cast<uint8_t>(faceData.uvOffset.pos.x + node.uvD.u),
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - node.uvD.v)};
			faceData.orderingTable.insert(textured, zIndex);

			auto& fromSource = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
			fromSource.primitive = {psyqo::Prim::MaskControl::FromSource};
			faceData.orderingTable.insert(fromSource, zIndex);

			auto& marker = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudQuad>();
			marker.primitive.pointA = node.projected[0];
			marker.primitive.pointB = node.projected[1];
			marker.primitive.pointC = node.projected[2];
			marker.primitive.pointD = node.projected[3];

			marker.primitive.setColorA({
				0,
				0,
				0,
			});
			marker.primitive.setColorB({
				0,
				0,
				0,
			});
			marker.primitive.setColorC({
				0,
				0,
				0,
			});
			marker.primitive.setColorD({
				0,
				0,
				0,
			});
			marker.primitive.setSemiTrans();
			faceData.orderingTable.insert(marker, zIndex);

			auto& forceSet = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
			forceSet.primitive = {psyqo::Prim::MaskControl::ForceSet};
			faceData.orderingTable.insert(forceSet, zIndex);

			auto& tpage = faceData.bumpAllocator.allocateFragment<psyqo::Prim::TPage>();
			tpage.primitive.attr = faceData.tpage;
			faceData.orderingTable.insert(tpage, zIndex);
		}
	} else { // untextured
		auto& quadPrim = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudQuad>();
		quadPrim.primitive.pointA = node.projected[0];
		quadPrim.primitive.pointB = node.projected[1];
		quadPrim.primitive.pointC = node.projected[2];
		quadPrim.primitive.pointD = node.projected[3];

		quadPrim.primitive.setColorA(node.colours[0]);
		quadPrim.primitive.setColorB(node.colours[1]);
		quadPrim.primitive.setColorC(node.colours[2]);
		quadPrim.primitive.setColorD(node.colours[3]);
		quadPrim.primitive.setOpaque();

		faceData.orderingTable.insert(quadPrim, zIndex);
	}
}

void Renderer::ProcessMeshTri(const RendererTri& tri, int8_t depth, const FaceData& faceData) {
	// projections for each vert
	FaceNode node;

	auto* verts = tri.boneAdjustedVerts ? tri.boneAdjustedVerts : tri.verts;

	// vertex 1->3
	for (int i = 0; i < 3; i++) {
		psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(verts[i]);
		psyqo::GTE::Kernels::rtps();
		node.irs[i] = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
	}

	// figure out nclip and skip rendering if backfaced
	psyqo::GTE::Kernels::nclip();
	if ((int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0>() == 0) {
		return;
	}

	// read projected verts
	psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&node.projected[0].packed);
	psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&node.projected[1].packed);
	psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&node.projected[2].packed);

	// read all the SZ data
	psyqo::GTE::read<psyqo::GTE::Register::SZ1>(&node.projectedDepths[0]);
	psyqo::GTE::read<psyqo::GTE::Register::SZ2>(&node.projectedDepths[1]);
	psyqo::GTE::read<psyqo::GTE::Register::SZ3>(&node.projectedDepths[2]);

	// we can skip a node if its going to be fully obscured by fog anyway
	if (faceData.isAdvancedFog && node.irs[0] >= 4096 && node.irs[1] >= 4096 && node.irs[2] >= 4096) {
		return;
	}

	auto shouldSubDivide =
		!tri.boneAdjustedVerts &&
		(node.projectedDepths[0] >= SUBDIVISION_MIN_DISTANCE || node.projectedDepths[1] >= SUBDIVISION_MIN_DISTANCE ||
		 node.projectedDepths[2] >= SUBDIVISION_MIN_DISTANCE) &&
		(node.projectedDepths[0] <= SUBDIVISION_MAX_DISTANCE || node.projectedDepths[1] <= SUBDIVISION_MAX_DISTANCE ||
		 node.projectedDepths[2] <= SUBDIVISION_MAX_DISTANCE);

	if (shouldSubDivide) {
		// copy any remaining data we couldn't read straight from the GTE
		for (int i = 0; i < 3; i++) {
			node.verts[i] = tri.verts[i];
			node.colours[i] = tri.colours[i];
		}

		node.uvA = tri.uvA;
		node.uvB = tri.uvB;
		node.uvC = tri.uvC;

		ProcessTriNode(node, depth, faceData);
	} else {
		// it won't be sub divided so just emit it out
		// figure out avg z index
		psyqo::GTE::Kernels::avsz3();

		// keep inside OT bounds
		uint32_t zIndex = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ>();
		if (zIndex == 0 || zIndex >= ORDERING_TABLE_SIZE) {
			return;
		}

		// clip screen space
		if (tri_clip(&SCREEN_SPACE, &node.projected[0], &node.projected[1], &node.projected[2])) {
			return;
		}

		// apply fog colours
		eastl::array<psyqo::Color, 3> vertexColours = {tri.colours[0], tri.colours[1], tri.colours[2]};
		eastl::array<psyqo::Color, 3> fogColours;

		if (!faceData.isAdvancedFog) {
			for (int i = 0; i < 3; i++) {
				ApplyAmbientToColour(&vertexColours[i]);
				vertexColours[i] = ApplyFogToColourGTE(vertexColours[i], node.irs[i]);
			}
		} else {
			GTESetFarColour({0, 0, 0});
			for (int i = 0; i < 3; i++) {
				ApplyAmbientToColour(&vertexColours[i]);
				vertexColours[i] = GTEColourLerp(vertexColours[i], node.irs[i]);
			}

			GTESetFarColour(m_lighting->GetFogColour());
			for (int i = 0; i < 3; i++) {
				fogColours[i] = GTEColourLerp({0, 0, 0}, node.irs[i]);
			}
		}

		bool isAdvancedFog = faceData.isAdvancedFog && (node.irs[0] > 0 || node.irs[1] > 0 || node.irs[2] > 0);

		if (faceData.texture != nullptr) {
			if (!isAdvancedFog || !(faceData.texture->flags & (1 << 0))) {
				auto& triPrim = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
				triPrim.primitive.pointA = node.projected[0];
				triPrim.primitive.pointB = node.projected[1];
				triPrim.primitive.pointC = node.projected[2];

				triPrim.primitive.setColorA(vertexColours[0]);
				triPrim.primitive.setColorB(vertexColours[1]);
				triPrim.primitive.setColorC(vertexColours[2]);
				if (!isAdvancedFog) {
					triPrim.primitive.setOpaque();
				} else {
					triPrim.primitive.setSemiTrans();
				}

				triPrim.primitive.tpage = faceData.tpage;

				if (faceData.texture->hasClut) {
					triPrim.primitive.clutIndex = {faceData.texture->clutX, faceData.texture->clutY};
				}

				triPrim.primitive.uvA = {static_cast<uint8_t>(faceData.uvOffset.pos.x + tri.uvA.u),
										 static_cast<uint8_t>(faceData.uvOffset.pos.y - tri.uvA.v)};
				triPrim.primitive.uvB = {static_cast<uint8_t>(faceData.uvOffset.pos.x + tri.uvB.u),
										 static_cast<uint8_t>(faceData.uvOffset.pos.y - tri.uvB.v)};
				triPrim.primitive.uvC = {static_cast<uint8_t>(faceData.uvOffset.pos.x + tri.uvC.u),
										 static_cast<uint8_t>(faceData.uvOffset.pos.y - tri.uvC.v)};

				faceData.orderingTable.insert(triPrim, zIndex);

				// we insert the untextured fog prim last because `insert` puts each new prim at the front of the
				// bucket.
				if (isAdvancedFog) {
					auto& triFogPrim = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTriangle>();
					triFogPrim.primitive.pointA = node.projected[0];
					triFogPrim.primitive.pointB = node.projected[1];
					triFogPrim.primitive.pointC = node.projected[2];

					triFogPrim.primitive.setColorA(fogColours[0]);
					triFogPrim.primitive.setColorB(fogColours[1]);
					triFogPrim.primitive.setColorC(fogColours[2]);
					triFogPrim.primitive.setOpaque();

					faceData.orderingTable.insert(triFogPrim, zIndex);
				}
			} else {
				// when we're handling advanced fog for a textured primitive that has holes in
				// we need to: setup the tpage, force set mask, set a marker (black verts),
				// set mask control to from source, set the textured prim,
				// do another from source (Test=1), do the fog, and finally a reset
				// we insert this into the OT in reverse order.
				auto& reset = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
				reset.primitive = {psyqo::Prim::MaskControl::FromSource};
				faceData.orderingTable.insert(reset, zIndex);

				auto& fog = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTriangle>();
				fog.primitive.pointA = node.projected[0];
				fog.primitive.pointB = node.projected[1];
				fog.primitive.pointC = node.projected[2];

				fog.primitive.setColorA(fogColours[0]);
				fog.primitive.setColorB(fogColours[1]);
				fog.primitive.setColorC(fogColours[2]);
				fog.primitive.setSemiTrans();
				faceData.orderingTable.insert(fog, zIndex);

				auto& test = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
				test.primitive = {psyqo::Prim::MaskControl::FromSource, psyqo::Prim::MaskControl::Test::Yes};
				faceData.orderingTable.insert(test, zIndex);

				auto& textured = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
				textured.primitive.pointA = node.projected[0];
				textured.primitive.pointB = node.projected[1];
				textured.primitive.pointC = node.projected[2];

				textured.primitive.setColorA(vertexColours[0]);
				textured.primitive.setColorB(vertexColours[1]);
				textured.primitive.setColorC(vertexColours[2]);
				textured.primitive.setOpaque();

				textured.primitive.tpage = faceData.tpage;
				if (faceData.texture->hasClut) {
					textured.primitive.clutIndex = {faceData.texture->clutX, faceData.texture->clutY};
				}

				textured.primitive.uvA = {static_cast<uint8_t>(faceData.uvOffset.pos.x + tri.uvA.u),
										  static_cast<uint8_t>(faceData.uvOffset.pos.y - tri.uvA.v)};
				textured.primitive.uvB = {static_cast<uint8_t>(faceData.uvOffset.pos.x + tri.uvB.u),
										  static_cast<uint8_t>(faceData.uvOffset.pos.y - tri.uvB.v)};
				textured.primitive.uvC = {static_cast<uint8_t>(faceData.uvOffset.pos.x + tri.uvC.u),
										  static_cast<uint8_t>(faceData.uvOffset.pos.y - tri.uvC.v)};
				faceData.orderingTable.insert(textured, zIndex);

				auto& fromSource = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
				fromSource.primitive = {psyqo::Prim::MaskControl::FromSource};
				faceData.orderingTable.insert(fromSource, zIndex);

				auto& marker = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTriangle>();
				marker.primitive.pointA = node.projected[0];
				marker.primitive.pointB = node.projected[1];
				marker.primitive.pointC = node.projected[2];

				marker.primitive.setColorA({
					0,
					0,
					0,
				});
				marker.primitive.setColorB({
					0,
					0,
					0,
				});
				marker.primitive.setColorC({
					0,
					0,
					0,
				});
				marker.primitive.setSemiTrans();
				faceData.orderingTable.insert(marker, zIndex);

				auto& forceSet = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
				forceSet.primitive = {psyqo::Prim::MaskControl::ForceSet};
				faceData.orderingTable.insert(forceSet, zIndex);

				auto& tpage = faceData.bumpAllocator.allocateFragment<psyqo::Prim::TPage>();
				tpage.primitive.attr = faceData.tpage;
				faceData.orderingTable.insert(tpage, zIndex);
			}
		} else { // untextured
			auto& triPrim = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTriangle>();
			triPrim.primitive.pointA = node.projected[0];
			triPrim.primitive.pointB = node.projected[1];
			triPrim.primitive.pointC = node.projected[2];

			triPrim.primitive.setColorA(vertexColours[0]);
			triPrim.primitive.setColorB(vertexColours[1]);
			triPrim.primitive.setColorC(vertexColours[2]);
			triPrim.primitive.setOpaque();

			faceData.orderingTable.insert(triPrim, zIndex);
		}
	}
}

void Renderer::ProcessTriNode(FaceNode& node, uint8_t subDivsionDepth, const FaceData& faceData) {
	// depth == 0 is handled by `ProcessMeshTri`
	if (subDivsionDepth > 0) {
		// write the projected values back into sxy*
		psyqo::GTE::write<psyqo::GTE::Register::SXY0, psyqo::GTE::Unsafe>(node.projected[0].packed);
		psyqo::GTE::write<psyqo::GTE::Register::SXY1, psyqo::GTE::Unsafe>(node.projected[1].packed);
		psyqo::GTE::write<psyqo::GTE::Register::SXY2, psyqo::GTE::Unsafe>(node.projected[2].packed);

		// write the projected values back into sz*
		psyqo::GTE::write<psyqo::GTE::Register::SZ1, psyqo::GTE::Unsafe>(node.projectedDepths[0]);
		psyqo::GTE::write<psyqo::GTE::Register::SZ2, psyqo::GTE::Unsafe>(node.projectedDepths[1]);
		psyqo::GTE::write<psyqo::GTE::Register::SZ3, psyqo::GTE::Safe>(node.projectedDepths[2]);

		// figure out nclip and skip rendering if backfaced
		psyqo::GTE::Kernels::nclip();
		if ((int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0>() == 0) {
			return;
		}
	}

	// figure out avg z index
	psyqo::GTE::Kernels::avsz3();

	// keep inside OT bounds
	uint32_t zIndex = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ>();
	if (zIndex == 0 || zIndex >= ORDERING_TABLE_SIZE) {
		return;
	}

	// clip screen space
	if (tri_clip(&SCREEN_SPACE, &node.projected[0], &node.projected[1], &node.projected[2])) {
		return;
	}

	// if its the parent then do colour work that can be propogated
	if (subDivsionDepth == 0) {
		// apply fog colours
		if (!faceData.isAdvancedFog) {
			for (int i = 0; i < 3; i++) {
				ApplyAmbientToColour(&node.colours[i]);
				node.colours[i] = ApplyFogToColourGTE(node.colours[i], node.irs[i]);
			}
		} else {
			GTESetFarColour({0, 0, 0});
			for (int i = 0; i < 3; i++) {
				ApplyAmbientToColour(&node.colours[i]);
				node.colours[i] = GTEColourLerp(node.colours[i], node.irs[i]);
			}

			GTESetFarColour(m_lighting->GetFogColour());
			for (int i = 0; i < 3; i++) {
				node.fogColours[i] = GTEColourLerp({0, 0, 0}, node.irs[i]);
			}
		}
	}

	// sub divide if we have depth and we're close enough
	if (subDivsionDepth <= SUBDIVISION_MAX_DEPTH) {
		auto abs = [](int32_t v) -> int32_t { return v < 0 ? -v : v; };
		auto edgeLength = [&abs](const psyqo::GTE::PackedVec3& a, const psyqo::GTE::PackedVec3& b) -> auto {
			return abs(a.x.value - b.x.value) + abs(a.y.value - b.y.value) + abs(a.z.value - b.z.value);
		};

		// measure the edges in 3d, not just 2d.
		const int32_t len01 = edgeLength(node.verts[0], node.verts[1]);
		const int32_t len12 = edgeLength(node.verts[1], node.verts[2]);
		const int32_t len20 = edgeLength(node.verts[2], node.verts[0]);

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

		FaceNode triA, triB;
		if (len01 >= len12 && len01 >= len20) {
			// split mid points of v0 and v1
			psyqo::GTE::PackedVec3 midVert01 = midVert(node.verts[0], node.verts[1]);

			psyqo::Vertex midProjectedVert01 = {};
			uint32_t midIR01 = 0;
			uint32_t midZ01 = 0;

			psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(midVert01);
			psyqo::GTE::Kernels::rtps();
			midIR01 = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
			psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&midProjectedVert01.packed);
			psyqo::GTE::read<psyqo::GTE::Register::SZ3>(&midZ01);

			psyqo::PrimPieces::UVCoords midUV01;
			midUV01.u = (node.uvA.u + node.uvB.u) >> 1;
			midUV01.v = (node.uvA.v + node.uvB.v) >> 1;

			psyqo::Color midColour01 = midColour(node.colours[0], node.colours[1]);
			psyqo::Color midFogColour01 = midColour(node.fogColours[0], node.fogColours[1]);

			// tri A
			triA.verts[0] = node.verts[0];
			triA.verts[1] = midVert01;
			triA.verts[2] = node.verts[2];

			triA.projected[0] = node.projected[0];
			triA.projected[1] = midProjectedVert01;
			triA.projected[2] = node.projected[2];

			triA.projectedDepths[0] = node.projectedDepths[0];
			triA.projectedDepths[1] = midZ01;
			triA.projectedDepths[2] = node.projectedDepths[2];

			triA.uvA = node.uvA;
			triA.uvB = midUV01;
			triA.uvC = node.uvC;

			triA.colours[0] = node.colours[0];
			triA.colours[1] = midColour01;
			triA.colours[2] = node.colours[2];

			triA.fogColours[0] = node.fogColours[0];
			triA.fogColours[1] = midFogColour01;
			triA.fogColours[2] = node.fogColours[2];

			triA.irs[0] = node.irs[0];
			triA.irs[1] = midIR01;
			triA.irs[2] = node.irs[2];

			// tri B
			triB.verts[0] = midVert01;
			triB.verts[1] = node.verts[1];
			triB.verts[2] = node.verts[2];

			triB.projected[0] = midProjectedVert01;
			triB.projected[1] = node.projected[1];
			triB.projected[2] = node.projected[2];

			triB.projectedDepths[0] = midZ01;
			triB.projectedDepths[1] = node.projectedDepths[1];
			triB.projectedDepths[2] = node.projectedDepths[2];

			triB.uvA = midUV01;
			triB.uvB = node.uvB;
			triB.uvC = node.uvC;

			triB.colours[0] = midColour01;
			triB.colours[1] = node.colours[1];
			triB.colours[2] = node.colours[2];

			triB.fogColours[0] = midFogColour01;
			triB.fogColours[1] = node.fogColours[1];
			triB.fogColours[2] = node.fogColours[2];

			triB.irs[0] = midIR01;
			triB.irs[1] = node.irs[1];
			triB.irs[2] = node.irs[2];
		} else if (len12 >= len01 && len12 >= len20) {
			// split mid points of v1 and v2
			psyqo::GTE::PackedVec3 midVert12 = midVert(node.verts[1], node.verts[2]);

			psyqo::Vertex midProjectedVert12 = {};
			uint32_t midIR12 = 0;
			uint32_t midZ12 = 0;

			psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(midVert12);
			psyqo::GTE::Kernels::rtps();
			midIR12 = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
			psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&midProjectedVert12.packed);
			psyqo::GTE::read<psyqo::GTE::Register::SZ3>(&midZ12);

			psyqo::PrimPieces::UVCoords midUV12;
			midUV12.u = (node.uvB.u + node.uvC.u) >> 1;
			midUV12.v = (node.uvB.v + node.uvC.v) >> 1;

			psyqo::Color midColour12 = midColour(node.colours[1], node.colours[2]);
			psyqo::Color midFogColour12 = midColour(node.fogColours[1], node.fogColours[2]);

			// tri A
			triA.verts[0] = node.verts[0];
			triA.verts[1] = node.verts[1];
			triA.verts[2] = midVert12;

			triA.projected[0] = node.projected[0];
			triA.projected[1] = node.projected[1];
			triA.projected[2] = midProjectedVert12;

			triA.projectedDepths[0] = node.projectedDepths[0];
			triA.projectedDepths[1] = node.projectedDepths[1];
			triA.projectedDepths[2] = midZ12;

			triA.uvA = node.uvA;
			triA.uvB = node.uvB;
			triA.uvC = {midUV12.u, midUV12.v, 0};

			triA.colours[0] = node.colours[0];
			triA.colours[1] = node.colours[1];
			triA.colours[2] = midColour12;

			triA.fogColours[0] = node.fogColours[0];
			triA.fogColours[1] = node.fogColours[1];
			triA.fogColours[2] = midFogColour12;

			triA.irs[0] = node.irs[0];
			triA.irs[1] = node.irs[1];
			triA.irs[2] = midIR12;

			// tri B
			triB.verts[0] = node.verts[0];
			triB.verts[1] = midVert12;
			triB.verts[2] = node.verts[2];

			triB.projected[0] = node.projected[0];
			triB.projected[1] = midProjectedVert12;
			triB.projected[2] = node.projected[2];

			triB.projectedDepths[0] = node.projectedDepths[0];
			triB.projectedDepths[1] = midZ12;
			triB.projectedDepths[2] = node.projectedDepths[2];

			triB.uvA = node.uvA;
			triB.uvB = midUV12;
			triB.uvC = node.uvC;

			triB.colours[0] = node.colours[0];
			triB.colours[1] = midColour12;
			triB.colours[2] = node.colours[2];

			triB.fogColours[0] = node.fogColours[0];
			triB.fogColours[1] = midFogColour12;
			triB.fogColours[2] = node.fogColours[2];

			triB.irs[0] = node.irs[0];
			triB.irs[1] = midIR12;
			triB.irs[2] = node.irs[2];
		} else {
			// split mid points of v0 and v2
			psyqo::GTE::PackedVec3 midVert02 = midVert(node.verts[0], node.verts[2]);

			psyqo::Vertex midProjectedVert02 = {};
			uint32_t midIR02 = 0;
			uint32_t midZ02 = 0;

			psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(midVert02);
			psyqo::GTE::Kernels::rtps();
			midIR02 = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
			psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&midProjectedVert02.packed);
			psyqo::GTE::read<psyqo::GTE::Register::SZ3>(&midZ02);

			psyqo::PrimPieces::UVCoords midUV02;
			midUV02.u = (node.uvA.u + node.uvC.u) >> 1;
			midUV02.v = (node.uvA.v + node.uvC.v) >> 1;

			psyqo::Color midColour02 = midColour(node.colours[0], node.colours[2]);
			psyqo::Color midFogColour02 = midColour(node.fogColours[0], node.fogColours[2]);

			// tri A
			triA.verts[0] = node.verts[0];
			triA.verts[1] = node.verts[1];
			triA.verts[2] = midVert02;

			triA.projected[0] = node.projected[0];
			triA.projected[1] = node.projected[1];
			triA.projected[2] = midProjectedVert02;

			triA.projectedDepths[0] = node.projectedDepths[0];
			triA.projectedDepths[1] = node.projectedDepths[1];
			triA.projectedDepths[2] = midZ02;

			triA.uvA = node.uvA;
			triA.uvB = node.uvB;
			triA.uvC = {midUV02.u, midUV02.v, 0};

			triA.colours[0] = node.colours[0];
			triA.colours[1] = node.colours[1];
			triA.colours[2] = midColour02;

			triA.fogColours[0] = node.fogColours[0];
			triA.fogColours[1] = node.fogColours[1];
			triA.fogColours[2] = midFogColour02;

			triA.irs[0] = node.irs[0];
			triA.irs[1] = node.irs[1];
			triA.irs[2] = midIR02;

			// tri B
			triB.verts[0] = midVert02;
			triB.verts[1] = node.verts[1];
			triB.verts[2] = node.verts[2];

			triB.projected[0] = midProjectedVert02;
			triB.projected[1] = node.projected[1];
			triB.projected[2] = node.projected[2];

			triB.projectedDepths[0] = midZ02;
			triB.projectedDepths[1] = node.projectedDepths[1];
			triB.projectedDepths[2] = node.projectedDepths[2];

			triB.uvA = midUV02;
			triB.uvB = node.uvB;
			triB.uvC = node.uvC;

			triB.colours[0] = midColour02;
			triB.colours[1] = node.colours[1];
			triB.colours[2] = node.colours[2];

			triB.fogColours[0] = midFogColour02;
			triB.fogColours[1] = node.fogColours[1];
			triB.fogColours[2] = node.fogColours[2];

			triB.irs[0] = midIR02;
			triB.irs[1] = node.irs[1];
			triB.irs[2] = node.irs[2];
		}

		ProcessTriNode(triA, subDivsionDepth + 1, faceData);
		ProcessTriNode(triB, subDivsionDepth + 1, faceData);
		return;
	} // OTHERWISE continue to process and add to OT

	// textured
	bool isAdvancedFog = faceData.isAdvancedFog && (node.irs[0] > 0 || node.irs[1] > 0 || node.irs[2] > 0);

	if (faceData.texture != nullptr) {
		if (!isAdvancedFog || !(faceData.texture->flags & (1 << 0))) {
			auto& triPrim = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
			triPrim.primitive.pointA = node.projected[0];
			triPrim.primitive.pointB = node.projected[1];
			triPrim.primitive.pointC = node.projected[2];

			triPrim.primitive.setColorA(node.colours[0]);
			triPrim.primitive.setColorB(node.colours[1]);
			triPrim.primitive.setColorC(node.colours[2]);
			if (!isAdvancedFog) {
				triPrim.primitive.setOpaque();
			} else {
				triPrim.primitive.setSemiTrans();
			}

			triPrim.primitive.tpage = faceData.tpage;

			if (faceData.texture->hasClut) {
				triPrim.primitive.clutIndex = {faceData.texture->clutX, faceData.texture->clutY};
			}

			triPrim.primitive.uvA = {static_cast<uint8_t>(faceData.uvOffset.pos.x + node.uvA.u),
									 static_cast<uint8_t>(faceData.uvOffset.pos.y - node.uvA.v)};
			triPrim.primitive.uvB = {static_cast<uint8_t>(faceData.uvOffset.pos.x + node.uvB.u),
									 static_cast<uint8_t>(faceData.uvOffset.pos.y - node.uvB.v)};
			triPrim.primitive.uvC = {static_cast<uint8_t>(faceData.uvOffset.pos.x + node.uvC.u),
									 static_cast<uint8_t>(faceData.uvOffset.pos.y - node.uvC.v)};

			faceData.orderingTable.insert(triPrim, zIndex);

			// we insert the untextured fog prim last because `insert` puts each new prim at the front of the bucket.
			if (isAdvancedFog) {
				auto& triFogPrim = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTriangle>();
				triFogPrim.primitive.pointA = node.projected[0];
				triFogPrim.primitive.pointB = node.projected[1];
				triFogPrim.primitive.pointC = node.projected[2];

				triFogPrim.primitive.setColorA(node.fogColours[0]);
				triFogPrim.primitive.setColorB(node.fogColours[1]);
				triFogPrim.primitive.setColorC(node.fogColours[2]);
				triFogPrim.primitive.setOpaque();

				faceData.orderingTable.insert(triFogPrim, zIndex);
			}
		} else {
			// when we're handling advanced fog for a textured primitive that has holes in
			// we need to: setup the tpage, force set mask, set a marker (black verts),
			// set mask control to from source, set the textured prim,
			// do another from source (Test=1), do the fog, and finally a reset
			// we insert this into the OT in reverse order.
			auto& reset = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
			reset.primitive = {psyqo::Prim::MaskControl::FromSource};
			faceData.orderingTable.insert(reset, zIndex);

			auto& fog = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTriangle>();
			fog.primitive.pointA = node.projected[0];
			fog.primitive.pointB = node.projected[1];
			fog.primitive.pointC = node.projected[2];

			fog.primitive.setColorA(node.fogColours[0]);
			fog.primitive.setColorB(node.fogColours[1]);
			fog.primitive.setColorC(node.fogColours[2]);
			fog.primitive.setSemiTrans();
			faceData.orderingTable.insert(fog, zIndex);

			auto& test = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
			test.primitive = {psyqo::Prim::MaskControl::FromSource, psyqo::Prim::MaskControl::Test::Yes};
			faceData.orderingTable.insert(test, zIndex);

			auto& textured = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
			textured.primitive.pointA = node.projected[0];
			textured.primitive.pointB = node.projected[1];
			textured.primitive.pointC = node.projected[2];

			textured.primitive.setColorA(node.colours[0]);
			textured.primitive.setColorB(node.colours[1]);
			textured.primitive.setColorC(node.colours[2]);
			textured.primitive.setOpaque();

			textured.primitive.tpage = faceData.tpage;
			if (faceData.texture->hasClut) {
				textured.primitive.clutIndex = {faceData.texture->clutX, faceData.texture->clutY};
			}

			textured.primitive.uvA = {static_cast<uint8_t>(faceData.uvOffset.pos.x + node.uvA.u),
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - node.uvA.v)};
			textured.primitive.uvB = {static_cast<uint8_t>(faceData.uvOffset.pos.x + node.uvB.u),
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - node.uvB.v)};
			textured.primitive.uvC = {static_cast<uint8_t>(faceData.uvOffset.pos.x + node.uvC.u),
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - node.uvC.v)};
			faceData.orderingTable.insert(textured, zIndex);

			auto& fromSource = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
			fromSource.primitive = {psyqo::Prim::MaskControl::FromSource};
			faceData.orderingTable.insert(fromSource, zIndex);

			auto& marker = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTriangle>();
			marker.primitive.pointA = node.projected[0];
			marker.primitive.pointB = node.projected[1];
			marker.primitive.pointC = node.projected[2];

			marker.primitive.setColorA({
				0,
				0,
				0,
			});
			marker.primitive.setColorB({
				0,
				0,
				0,
			});
			marker.primitive.setColorC({
				0,
				0,
				0,
			});
			marker.primitive.setSemiTrans();
			faceData.orderingTable.insert(marker, zIndex);

			auto& forceSet = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
			forceSet.primitive = {psyqo::Prim::MaskControl::ForceSet};
			faceData.orderingTable.insert(forceSet, zIndex);

			auto& tpage = faceData.bumpAllocator.allocateFragment<psyqo::Prim::TPage>();
			tpage.primitive.attr = faceData.tpage;
			faceData.orderingTable.insert(tpage, zIndex);
		}
	} else { // untextured
		auto& triPrim = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTriangle>();
		triPrim.primitive.pointA = node.projected[0];
		triPrim.primitive.pointB = node.projected[1];
		triPrim.primitive.pointC = node.projected[2];

		triPrim.primitive.setColorA(node.colours[0]);
		triPrim.primitive.setColorB(node.colours[1]);
		triPrim.primitive.setColorC(node.colours[2]);
		triPrim.primitive.setOpaque();

		faceData.orderingTable.insert(triPrim, zIndex);
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

psyqo::Color Renderer::GTEColourLerp(const psyqo::Color& input, uint32_t p) {
	psyqo::GTE::write<psyqo::GTE::Register::IR0, psyqo::GTE::Unsafe>(p);
	psyqo::GTE::write<psyqo::GTE::Register::RGB, psyqo::GTE::Safe>(input.packed);
	psyqo::GTE::Kernels::dpcs();
	return {.packed = psyqo::GTE::readRaw<psyqo::GTE::Register::RGB2>()};
}

// Interpolate from input to FC
psyqo::Color Renderer::ApplyFogToColourGTE(psyqo::Color input, uint32_t p) {
	if (m_lighting->IsAdvancedFogEnabled() || !m_lighting->IsSimpleFogEnabled()) {
		return input;
	}

	return GTEColourLerp(input, p);
}
