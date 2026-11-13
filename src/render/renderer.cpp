/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "renderer.hh"
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
#include <psyqo/primitives/rectangles.hh>
#include <psyqo/primitives/sprites.hh>
#include <psyqo/primitives/triangles.hh>
#include <psyqo/vector.hh>

Renderer* Renderer::m_instance = nullptr;
psyqo::Font<100> Renderer::m_systemFont;
static constexpr psyqo::Matrix33 identityMatrix = {
	{{1.0_fp, 0.0_fp, 0.0_fp}, {0.0_fp, 1.0_fp, 0.0_fp}, {0.0_fp, 0.0_fp, 1.0_fp}}};

static constexpr psyqo::Rect SCREEN_SPACE = {.pos = {0, 0}, .size = {320, 240}};
static constexpr uint8_t PROJECTION_DISTANCE = 120;

static constexpr int16_t SCREEN_WIDTH_HALF = SCREEN_SPACE.size.x / 2, SCREEN_HEIGHT_HALF = SCREEN_SPACE.size.y / 2;
static constexpr int32_t WIDTH_PLANE_LENGTH = 200;	// sqrt(PROJECTION_DISTANCE squared + SCREEN_WIDTH_HALF squared)
static constexpr int32_t HEIGHT_PLANE_LENGTH = 170; // sqrt(PROJECTION_DISTANCE squared + SCREEN_HEIGHT_HALF squared)

static constexpr uint32_t SCRATCH_CORNER_COUNT = (4 + 2 * (SUBDIVISION_MAX_DEPTH + 1));
__attribute__((section(".scratchpad"))) FaceCorner stFaceCorners[SCRATCH_CORNER_COUNT];
__attribute__((section(".scratchpad"))) alignas(FaceData) uint8_t stFaceDataStorage[sizeof(FaceData)];

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

enum ScreenSpaceClip : uint8_t { CLIP_LEFT = 1, CLIP_RIGHT = 2, CLIP_TOP = 4, CLIP_BOTTOM = 8 };

inline constexpr int TestClip(int16_t x, int16_t y) {
	int result = 0;

	if (x < SCREEN_SPACE.pos.x) {
		result |= CLIP_LEFT;
	}

	if (x >= (SCREEN_SPACE.pos.x + SCREEN_SPACE.size.w)) {
		result |= CLIP_RIGHT;
	}

	if (y < SCREEN_SPACE.pos.y) {
		result |= CLIP_TOP;
	}

	if (y >= (SCREEN_SPACE.pos.y + SCREEN_SPACE.size.h)) {
		result |= CLIP_BOTTOM;
	}

	return result;
}

inline constexpr int QuadClip(const psyqo::Vertex& v0, const psyqo::Vertex& v1, const psyqo::Vertex& v2,
							  const psyqo::Vertex& v3) {
	int c[4] = {
		TestClip(v0.x, v0.y),
		TestClip(v1.x, v1.y),
		TestClip(v2.x, v2.y),
		TestClip(v3.x, v3.y),
	};

	if ((c[0] & c[1]) == 0) {
		return 0;
	}

	if ((c[1] & c[2]) == 0) {
		return 0;
	}

	if ((c[2] & c[3]) == 0) {
		return 0;
	}

	if ((c[3] & c[0]) == 0) {
		return 0;
	}

	if ((c[0] & c[2]) == 0) {
		return 0;
	}

	if ((c[1] & c[3]) == 0) {
		return 0;
	}

	return 1;
}

inline constexpr int TriClip(const psyqo::Vertex& v0, const psyqo::Vertex& v1, const psyqo::Vertex& v2) {
	int c[4] = {
		TestClip(v0.x, v0.y),
		TestClip(v1.x, v1.y),
		TestClip(v2.x, v2.y),
	};

	if ((c[0] & c[1]) == 0) {
		return 0;
	}

	if ((c[1] & c[2]) == 0) {
		return 0;
	}

	if ((c[2] & c[0]) == 0) {
		return 0;
	}

	return 1;
}

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

	ApplyFogMode();
}

void Renderer::ApplyFogMode(void) {
	// default the fog near/far depths
	m_fogNearDepth = 0;
	m_fogFarDepth = 0;

	// set fog colour (FC)
	if (m_lighting->IsAdvancedFogEnabled()) {
		SetFogNearFar(0.33_fp, 1.1_fp);
		m_appliedFogMode = FOG_ADVANCED;
	} else if (m_lighting->IsSimpleFogEnabled()) {
		SetFogColour(m_lighting->GetFogColour());
		SetFogNearFar(0.5_fp, 1.55_fp);
		m_appliedFogMode = FOG_SIMPLE;
	} else {
		m_appliedFogMode = FOG_OFF;
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

	m_fogNearDepth = (a * PROJECTION_DISTANCE / h);
	m_fogFarDepth = (b * PROJECTION_DISTANCE / h);
}

/* this must be called at the start of each frame */
uint32_t Renderer::Process(void) {
	uint32_t currentFrameCount = m_gpu.getFrameCount();

	// fog can be turned on or switched mode after `StartScene`, so set the near/far for the new mode
	if (m_lighting) {
		const FogMode fogMode = m_lighting->IsAdvancedFogEnabled()
									? FOG_ADVANCED
									: (m_lighting->IsSimpleFogEnabled() ? FOG_SIMPLE : FOG_OFF);
		if (fogMode != m_appliedFogMode) {
			ApplyFogMode();
		}
	}

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

psyqo::Vec3 Renderer::TransformObjectToViewSpace(const psyqo::Vec3& pos, const psyqo::Matrix33& cameraRotationMatrix,
												 const psyqo::Matrix33& finalCameraMatrix) {
	// restore camera rotation for delta transform
	psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(cameraRotationMatrix);
	psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(psyqo::Vec3{0, 0, 0});

	// calculate delta pos of the object compared to the camera
	auto objDeltaPos = (pos - m_activeCamera->pos());
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
		if (!IsGameObjectVisible(deltaCentre, gameObject->mesh()->bsphere.radius)) {
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

			// compute the bone rotation/translation offsets once per bone
			psyqo::Matrix33 boneRotationOffsets[MAX_BONES];
			psyqo::Vec3 boneTranslationOffsets[MAX_BONES];

			for (int i = 0; i < mesh->skeleton->numBones; i++) {
				auto& bone = mesh->skeleton->bones[i];
				if (!bone.isDirty) {
					continue;
				}

				psyqo::GteMath::multiplyMatrix33(bone.worldMatrix.rotationMatrix, bone.bindPoseInverse.rotationMatrix,
												 &boneRotationOffsets[i]);

				// translation offset = world_T + (R_world * bindPoseInverse.translation)
				psyqo::GteMath::matrixVecMul3(bone.worldMatrix.rotationMatrix, bone.bindPoseInverse.translation,
											  &boneTranslationOffsets[i]);
				boneTranslationOffsets[i] += bone.worldMatrix.translation;
			}

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

					auto& rotationOffset = boneRotationOffsets[bone.id];
					auto& translationOffset = boneTranslationOffsets[bone.id];

					// final position
					psyqo::Vec3 rotatedVert = {0, 0, 0};
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

			// give it its rotation data back since we just clobbered it doing math
			psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(finalCameraMatrix);
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
		// built in place in the scratch pad, FaceData has const and reference members so it can't be assigned
		const FaceData& faceData = *new (stFaceDataStorage) FaceData{
			texture, tpage, offset, allocator, ot, m_lighting->IsAdvancedFogEnabled(), quickAdvancedFog};

		// if we're doing a character then quick advanced fog is used (stp untextured quad on top)
		if (quickAdvancedFog) {
			m_quickAdvancedFogOverlayQuad = nullptr;

			// overlay
			m_quickAdvancedFogOverlayQuad = &faceData.bumpAllocator.allocateFragment<psyqo::Prim::Quad>();
			m_quickAdvancedFogOverlayQuad->primitive.setSemiTrans();
			m_qafSmallestZ = ORDERING_TABLE_SIZE - 1;

			m_qafOverlayMinX = INT16_MAX, m_qafOverlayMinY = INT16_MAX, m_qafOverlayMaxX = -INT16_MAX,
			m_qafOverlayMaxY = -INT16_MAX;
			m_qafIR0 = 0;
			m_qafIROCount = 0;
		}

		ProcessFaceData processData = {mesh, &faceData};
		auto processFace = [this, args = &processData] {
			for (int32_t i = 0; i < args->mesh->facesCount; i++) {
				const auto& rendererFace = args->mesh->facesData[i];

				if (rendererFace.isQuad) {
					ProcessMeshQuad(args->mesh->quads[rendererFace.rendererFaceIx], 0, *args->faceData);
				} else {
					ProcessMeshTri(args->mesh->tris[rendererFace.rendererFaceIx], 0, *args->faceData);
				}
			}
		};

		m_faceLoop.initialize(eastl::move(processFace));
		m_faceLoop.resume();

		if (quickAdvancedFog && m_quickAdvancedFogOverlayQuad && m_qafOverlayMinX != INT16_MAX &&
			m_qafOverlayMinX < m_qafOverlayMaxX) {
			auto& reset = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
			reset.primitive = {psyqo::Prim::MaskControl::FromSource};
			faceData.orderingTable.insert(reset, m_qafSmallestZ - 1);

			m_quickAdvancedFogOverlayQuad->primitive.pointA = psyqo::Vertex{m_qafOverlayMinX, m_qafOverlayMinY};
			m_quickAdvancedFogOverlayQuad->primitive.pointB = psyqo::Vertex{m_qafOverlayMaxX, m_qafOverlayMinY};
			m_quickAdvancedFogOverlayQuad->primitive.pointC = psyqo::Vertex{m_qafOverlayMinX, m_qafOverlayMaxY};
			m_quickAdvancedFogOverlayQuad->primitive.pointD = psyqo::Vertex{m_qafOverlayMaxX, m_qafOverlayMaxY};

			auto& fog = m_lighting->GetFogColour();
			auto t = m_qafIROCount > 0 ? eastl::min<uint32_t>(m_qafIR0 / m_qafIROCount, 4096) : 0;
			psyqo::Color add = {uint8_t((fog.r * t) >> 12), uint8_t((fog.g * t) >> 12), uint8_t((fog.b * t) >> 12)};
			m_quickAdvancedFogOverlayQuad->primitive.setColor(add);

			faceData.orderingTable.insert(*m_quickAdvancedFogOverlayQuad, m_qafSmallestZ - 1);

			auto& overlayTpage = faceData.bumpAllocator.allocateFragment<psyqo::Prim::TPage>();
			overlayTpage.primitive.attr = tpage;
			overlayTpage.primitive.attr.set(psyqo::Prim::TPageAttr::FullBackAndFullFront);
			faceData.orderingTable.insert(overlayTpage, m_qafSmallestZ - 1);

			auto& test = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
			test.primitive = {psyqo::Prim::MaskControl::FromSource, psyqo::Prim::MaskControl::Test::Yes};
			faceData.orderingTable.insert(test, m_qafSmallestZ - 1);
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

		// make sure we're not past screen bounds
		if (QuadClip(projected[0], projected[1], projected[2], projected[3])) {
			continue;
		}

		// handle colour + fog — all verts share same colour on a billboard
		auto colour = billboard->colour();
		ApplyAmbientToColour(&colour);
		if (m_lighting->IsAdvancedFogEnabled()) {
			GTESetFarColour(m_lighting->GetFogColour());
			colour = GTEColourLerp(colour, p);
		} else {
			colour = ApplyFogToColourGTE(colour, p);
		}

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
		auto const& particles = emitter->particles();

		// send tpage info to gpu
		auto texture = emitter->pParticleTexture();
		if (texture) {
			tpage = g_madnightEngine.m_textureManager.GetTPageAttr(texture);
		}

		// one time setup for each particle that wont change
		psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(cameraRotationMatrix);
		psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(psyqo::Vec3{0, 0, 0});

		// fog/far colour also wont change per particle
		if (m_lighting->IsAdvancedFogEnabled()) {
			GTESetFarColour(m_lighting->GetFogColour());
		}

		for (auto const& particle : particles) {
			if (emitter->AreParticles2D()) {
				auto particlePos = particle.pos() - m_activeCamera->pos();
				if (particlePos.z <= 0) {
					continue;
				}

				// we dont need to offset it
				psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(particlePos);

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

				// calculate size and make sure its at least one pixel
				auto w = (particle.size().x.value * PROJECTION_DISTANCE) / particlePos.z.value;
				auto h = (particle.size().y.value * PROJECTION_DISTANCE) / particlePos.z.value;
				auto scaledSize = psyqo::Vertex{eastl::clamp<int16_t>(w, 1, w), eastl::clamp<int16_t>(h, 1, h)};

				// make sure pos is sane, and the scaled size isnt gonna full the screen
				auto pos = psyqo::Vertex{static_cast<int16_t>(vertex.x - scaledSize.x / 2),
										 static_cast<int16_t>(vertex.y - scaledSize.y / 2)};
				static constexpr int16_t maxParticleSize = SCREEN_SPACE.size.y / 8;
				if (pos.x < 0 || pos.x >= SCREEN_SPACE.size.x || pos.y < 0 || pos.y >= SCREEN_SPACE.size.y ||
					scaledSize.y > maxParticleSize) {
					continue;
				}

				// handle fog, this doesn't change if its Sprite or Rectangle
				auto colour = particle.colour();
				ApplyAmbientToColour(&colour);

				if (!m_lighting->IsAdvancedFogEnabled() && m_lighting->IsSimpleFogEnabled()) {
					colour = GTEColourLerp(colour, GetFogFactor(sz).value);
				}

				// if its textured use a sprite
				if (texture) {
					auto& sprite = allocator.allocateFragment<psyqo::Prim::Sprite>();
					// update sprite with clut/uv data
					sprite.primitive.texInfo.clut = psyqo::PrimPieces::ClutIndex(texture->clutX, texture->clutY);

					// as particles can be quads they have 4 lots of uv data, so just take the first one
					sprite.primitive.texInfo.u = particle.uv()[0].u;
					sprite.primitive.texInfo.v = particle.uv()[0].v;

					sprite.primitive.position = pos;
					sprite.primitive.size = scaledSize;

					// set colour
					sprite.primitive.setColor(colour);

					auto& spriteTPage = allocator.allocateFragment<psyqo::Prim::TPage>();
					spriteTPage.primitive.attr = tpage;

					ot.insert(sprite, zIndex);
					ot.insert(spriteTPage, zIndex);
				} else {
					// otherwise use a rectangle
					auto& rectangle = allocator.allocateFragment<psyqo::Prim::Rectangle>();

					rectangle.primitive.position = pos;
					rectangle.primitive.size = scaledSize;

					// set colour
					rectangle.primitive.setColor(colour);

					ot.insert(rectangle, zIndex);
				}
			} else {
				auto finalParticlePos =
					TransformObjectToViewSpace(particle.pos(), cameraRotationMatrix, finalCameraMatrix);

				if (texture) {
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

				// make sure we're not past screen bounds
				if (QuadClip(projected[0], projected[1], projected[2], projected[3])) {
					continue;
				}

				// handle colour + fog — particles use same colour for all verts so one rtps for IR0 is enough
				// re-transform corner 0 to get a representative IR0 for fog
				psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(particle.corners()[0]);
				psyqo::GTE::Kernels::rtps();
				uint32_t p = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();

				auto colour = particle.colour();
				ApplyAmbientToColour(&colour);
				if (m_lighting->IsAdvancedFogEnabled()) {
					GTESetFarColour(m_lighting->GetFogColour());
					colour = GTEColourLerp(colour, p);
				} else if (m_lighting->IsSimpleFogEnabled()) {
					colour = ApplyFogToColourGTE(colour, p);
				}

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

bool Renderer::IsGameObjectVisible(const psyqo::Vec3& objectPos, const int32_t& boundingSphereRadius) {
	int32_t cx = objectPos.x.value;
	int32_t cy = objectPos.y.value;
	int32_t cz = objectPos.z.value;
	int32_t r = boundingSphereRadius;

	// reject if sphere is entirely behind near plane
	if (cz + r <= 0) {
		return false;
	}

	// reject if its too far away (and the fog is on)
	if ((cz - r) >= m_fogFarDepth && (m_lighting->IsSimpleFogEnabled() || m_lighting->IsAdvancedFogEnabled())) {
		return false;
	}

	// reject depending on the four left/right/top/bottom values
	auto projectedCX = PROJECTION_DISTANCE * cx, projectedCY = PROJECTION_DISTANCE * cy,
		 projectedWidthRadius = WIDTH_PLANE_LENGTH * r, projectedHeightRadius = HEIGHT_PLANE_LENGTH * r;

	// left
	if (projectedCX + (SCREEN_WIDTH_HALF * cz) < -projectedWidthRadius) {
		return false;
	}

	// right
	if ((SCREEN_WIDTH_HALF * cz) - projectedCX < -projectedWidthRadius) {
		return false;
	}

	// top
	if (projectedCY + (SCREEN_HEIGHT_HALF * cz) < -projectedHeightRadius) {
		return false;
	}

	// bottom
	if ((SCREEN_HEIGHT_HALF * cz) - projectedCY < -projectedHeightRadius) {
		return false;
	}

	return true;
}

void Renderer::ProcessMeshQuad(const RendererQuad& quad, int8_t depth, const FaceData& faceData) {
	auto* verts = quad.boneAdjustedVerts ? quad.boneAdjustedVerts : quad.verts;

	// vertex 1->3
	for (int i = 0; i < 3; i++) {
		psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(verts[i]);
		psyqo::GTE::Kernels::rtps();
		stFaceCorners[i].ir = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
	}

	// figure out nclip and skip rendering if backfaced
	psyqo::GTE::Kernels::nclip();
	if ((int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0>() == 0) {
		return;
	}

	// read projected verts
	psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&stFaceCorners[0].projected.packed);
	psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&stFaceCorners[1].projected.packed);
	psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&stFaceCorners[2].projected.packed);

	// final vert
	psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(verts[3]);
	psyqo::GTE::Kernels::rtps();
	stFaceCorners[3].ir = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
	psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&stFaceCorners[3].projected.packed);

	// read all the SZ data
	psyqo::GTE::read<psyqo::GTE::Register::SZ0>(&stFaceCorners[0].depth);
	psyqo::GTE::read<psyqo::GTE::Register::SZ1>(&stFaceCorners[1].depth);
	psyqo::GTE::read<psyqo::GTE::Register::SZ2>(&stFaceCorners[2].depth);
	psyqo::GTE::read<psyqo::GTE::Register::SZ3>(&stFaceCorners[3].depth);

	// we can skip a node if its going to be fully obscured by fog anyway
	if (faceData.isAdvancedFog && stFaceCorners[0].ir >= 4096 && stFaceCorners[1].ir >= 4096 &&
		stFaceCorners[2].ir >= 4096 && stFaceCorners[3].ir >= 4096) {
		return;
	}

	auto shouldSubDivide =
		!quad.boneAdjustedVerts &&
		(stFaceCorners[0].depth >= SUBDIVISION_MIN_DISTANCE || stFaceCorners[1].depth >= SUBDIVISION_MIN_DISTANCE ||
		 stFaceCorners[2].depth >= SUBDIVISION_MIN_DISTANCE || stFaceCorners[3].depth >= SUBDIVISION_MIN_DISTANCE) &&
		(stFaceCorners[0].depth <= SUBDIVISION_MAX_DISTANCE || stFaceCorners[1].depth <= SUBDIVISION_MAX_DISTANCE ||
		 stFaceCorners[2].depth <= SUBDIVISION_MAX_DISTANCE || stFaceCorners[3].depth <= SUBDIVISION_MAX_DISTANCE);

	if (shouldSubDivide) {
		// copy any remaining data we couldn't read straight from the GTE
		for (int i = 0; i < 4; i++) {
			stFaceCorners[i].vert = quad.verts[i];
			stFaceCorners[i].colour = quad.colours[i];
		}

		// apply fog colours (skipped if advanced fog is enabled)
		if (!faceData.isAdvancedFog) {
			for (int i = 0; i < 4; i++) {
				ApplyAmbientToColour(&stFaceCorners[i].colour);
				stFaceCorners[i].colour = ApplyFogToColourGTE(stFaceCorners[i].colour, stFaceCorners[i].ir);
			}
		} else {
			if (!faceData.useQuickAdvancedFogPass) {
				GTESetFarColour({0, 0, 0});
				for (int i = 0; i < 4; i++) {
					ApplyAmbientToColour(&stFaceCorners[i].colour);
					stFaceCorners[i].colour = GTEColourLerp(stFaceCorners[i].colour, stFaceCorners[i].ir);
				}

				GTESetFarColour(m_lighting->GetFogColour());
				for (int i = 0; i < 4; i++) {
					stFaceCorners[i].fogColour = GTEColourLerp({0, 0, 0}, stFaceCorners[i].ir);
				}
			} else {
				for (int i = 0; i < 4; i++) {
					auto density = 4096 - stFaceCorners[i].ir;
					ApplyAmbientToColour(&stFaceCorners[i].colour);
					stFaceCorners[i].colour = {uint8_t((stFaceCorners[i].colour.r * density) >> 12),
											   uint8_t((stFaceCorners[i].colour.g * density) >> 12),
											   uint8_t((stFaceCorners[i].colour.b * density) >> 12)};
					m_qafIR0 += stFaceCorners[i].ir;
				}

				// average the quick fog t scale
				m_qafIROCount += 4;
			}
		}

		stFaceCorners[0].uv = {static_cast<uint8_t>(quad.uvA.u + faceData.uvOffset.pos.x), quad.uvA.v};
		stFaceCorners[1].uv = {static_cast<uint8_t>(quad.uvB.u + faceData.uvOffset.pos.x), quad.uvB.v};
		stFaceCorners[2].uv = {static_cast<uint8_t>(quad.uvC.u + faceData.uvOffset.pos.x), quad.uvC.v};
		stFaceCorners[3].uv = {static_cast<uint8_t>(quad.uvD.u + faceData.uvOffset.pos.x), quad.uvD.v};

		FaceCorner* corners[4] = {&stFaceCorners[0], &stFaceCorners[1], &stFaceCorners[2], &stFaceCorners[3]};
		ProcessQuadNode(corners, depth, faceData);
	} else {
		// it won't be sub divided so just emit it out
		// figure out avg z index
		psyqo::GTE::Kernels::avsz4();

		// keep inside OT bounds
		uint32_t zIndex = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ>();
		if (zIndex == 0 || zIndex >= ORDERING_TABLE_SIZE) {
			return;
		}

		// make sure we're not past screen bounds
		if (QuadClip(stFaceCorners[0].projected, stFaceCorners[1].projected, stFaceCorners[2].projected,
					 stFaceCorners[3].projected)) {
			return;
		}

		// apply fog colours
		eastl::array<psyqo::Color, 4> vertexColours = {quad.colours[0], quad.colours[1], quad.colours[2],
													   quad.colours[3]};
		eastl::array<psyqo::Color, 4> fogColours;

		if (!faceData.isAdvancedFog) {
			for (int i = 0; i < 4; i++) {
				ApplyAmbientToColour(&vertexColours[i]);
				vertexColours[i] = ApplyFogToColourGTE(vertexColours[i], stFaceCorners[i].ir);
			}
		} else {
			if (!faceData.useQuickAdvancedFogPass) {
				GTESetFarColour({0, 0, 0});
				for (int i = 0; i < 4; i++) {
					ApplyAmbientToColour(&vertexColours[i]);
					vertexColours[i] = GTEColourLerp(vertexColours[i], stFaceCorners[i].ir);
				}

				GTESetFarColour(m_lighting->GetFogColour());
				for (int i = 0; i < 4; i++) {
					fogColours[i] = GTEColourLerp({0, 0, 0}, stFaceCorners[i].ir);
				}
			} else {
				for (int i = 0; i < 4; i++) {
					auto density = 4096 - stFaceCorners[i].ir;
					ApplyAmbientToColour(&vertexColours[i]);
					vertexColours[i] = {uint8_t((vertexColours[i].r * density) >> 12),
										uint8_t((vertexColours[i].g * density) >> 12),
										uint8_t((vertexColours[i].b * density) >> 12)};
					m_qafIR0 += stFaceCorners[i].ir;
				}

				m_qafIROCount += 4;
			}
		}

		bool isAdvancedFog = faceData.isAdvancedFog &&
							 (stFaceCorners[0].ir > 0 || stFaceCorners[1].ir > 0 || stFaceCorners[2].ir > 0 ||
							  stFaceCorners[3].ir > 0) &&
							 !faceData.useQuickAdvancedFogPass;

		// update overlay min/max points
		if (faceData.isAdvancedFog && faceData.useQuickAdvancedFogPass) {
			for (int i = 0; i < 4; i++) {
				m_qafOverlayMinX = eastl::min(m_qafOverlayMinX, stFaceCorners[i].projected.x);
				m_qafOverlayMinY = eastl::min(m_qafOverlayMinY, stFaceCorners[i].projected.y);
				m_qafOverlayMaxX = eastl::max(m_qafOverlayMaxX, stFaceCorners[i].projected.x);
				m_qafOverlayMaxY = eastl::max(m_qafOverlayMaxY, stFaceCorners[i].projected.y);
			}

			m_qafSmallestZ = eastl::min(m_qafSmallestZ, zIndex);
		}

		if (faceData.texture != nullptr) {
			if (!isAdvancedFog || !(faceData.texture->flags & (1 << 0))) {
				auto& quadPrim = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
				quadPrim.primitive.pointA = stFaceCorners[0].projected;
				quadPrim.primitive.pointB = stFaceCorners[1].projected;
				quadPrim.primitive.pointC = stFaceCorners[2].projected;
				quadPrim.primitive.pointD = stFaceCorners[3].projected;

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
					quadFogPrim.primitive.pointA = stFaceCorners[0].projected;
					quadFogPrim.primitive.pointB = stFaceCorners[1].projected;
					quadFogPrim.primitive.pointC = stFaceCorners[2].projected;
					quadFogPrim.primitive.pointD = stFaceCorners[3].projected;

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
				fog.primitive.pointA = stFaceCorners[0].projected;
				fog.primitive.pointB = stFaceCorners[1].projected;
				fog.primitive.pointC = stFaceCorners[2].projected;
				fog.primitive.pointD = stFaceCorners[3].projected;

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
				textured.primitive.pointA = stFaceCorners[0].projected;
				textured.primitive.pointB = stFaceCorners[1].projected;
				textured.primitive.pointC = stFaceCorners[2].projected;
				textured.primitive.pointD = stFaceCorners[3].projected;

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
				marker.primitive.pointA = stFaceCorners[0].projected;
				marker.primitive.pointB = stFaceCorners[1].projected;
				marker.primitive.pointC = stFaceCorners[2].projected;
				marker.primitive.pointD = stFaceCorners[3].projected;

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
			quadPrim.primitive.pointA = stFaceCorners[0].projected;
			quadPrim.primitive.pointB = stFaceCorners[1].projected;
			quadPrim.primitive.pointC = stFaceCorners[2].projected;
			quadPrim.primitive.pointD = stFaceCorners[3].projected;

			if (faceData.isAdvancedFog && !faceData.useQuickAdvancedFogPass) {
				psyqo::Color col[4];
				for (int i = 0; i < 4; i++) {
					col[i].r = eastl::min(255, vertexColours[i].r + fogColours[i].r);
					col[i].g = eastl::min(255, vertexColours[i].g + fogColours[i].g);
					col[i].b = eastl::min(255, vertexColours[i].b + fogColours[i].b);
				}

				quadPrim.primitive.setColorA(col[0]);
				quadPrim.primitive.setColorB(col[1]);
				quadPrim.primitive.setColorC(col[2]);
				quadPrim.primitive.setColorD(col[3]);
			} else {
				quadPrim.primitive.setColorA(vertexColours[0]);
				quadPrim.primitive.setColorB(vertexColours[1]);
				quadPrim.primitive.setColorC(vertexColours[2]);
				quadPrim.primitive.setColorD(vertexColours[3]);
			}
			quadPrim.primitive.setOpaque();

			faceData.orderingTable.insert(quadPrim, zIndex);
		}
	}
}

void Renderer::ProcessQuadNode(FaceCorner* const* corners, uint8_t subDivisionDepth, const FaceData& faceData) {
	// depth == 0 is handled by `ProcessMeshQuad`
	if (subDivisionDepth > 0) {
		// write the projected values back into sz*
		psyqo::GTE::write<psyqo::GTE::Register::SZ0, psyqo::GTE::Unsafe>(corners[0]->depth);
		psyqo::GTE::write<psyqo::GTE::Register::SZ1, psyqo::GTE::Unsafe>(corners[1]->depth);
		psyqo::GTE::write<psyqo::GTE::Register::SZ2, psyqo::GTE::Unsafe>(corners[2]->depth);
		psyqo::GTE::write<psyqo::GTE::Register::SZ3, psyqo::GTE::Safe>(corners[3]->depth);
	}

	// figure out avg z index
	psyqo::GTE::Kernels::avsz4();

	// keep inside OT bounds
	uint32_t zIndex = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ>();
	if (zIndex == 0 || zIndex >= ORDERING_TABLE_SIZE) {
		return;
	}

	// make sure we're not past screen bounds
	if (QuadClip(corners[0]->projected, corners[1]->projected, corners[2]->projected, corners[3]->projected)) {
		return;
	}

	// sub divide if we have depth and we're close enough, and the node is still big on screen
	auto bigOnScreen = [&corners]() -> bool {
		if constexpr (SUBDIVISION_SCREEN_CUTOFF == 0) {
			return true;
		} else {
			// the same edge pairs as the 3d width/height below, measured on screen
			auto screenEdgeLength = [](const psyqo::Vertex a, const psyqo::Vertex b) -> int32_t {
				int32_t dx = a.x - b.x, dy = a.y - b.y;
				return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
			};
			auto width = screenEdgeLength(corners[0]->projected, corners[1]->projected) +
						 screenEdgeLength(corners[2]->projected, corners[3]->projected);
			auto height = screenEdgeLength(corners[0]->projected, corners[2]->projected) +
						  screenEdgeLength(corners[1]->projected, corners[3]->projected);
			return eastl::max(width, height) >= 2 * SUBDIVISION_SCREEN_CUTOFF;
		}
	};

	if (subDivisionDepth <= SUBDIVISION_MAX_DEPTH && bigOnScreen()) {
		auto abs = [](int32_t v) -> int32_t { return v < 0 ? -v : v; };
		auto edgeLength = [&abs](const psyqo::GTE::PackedVec3 a, const psyqo::GTE::PackedVec3 b) -> auto {
			return abs(a.x.value - b.x.value) + abs(a.y.value - b.y.value) + abs(a.z.value - b.z.value);
		};

		// make sure to measure the width and height in 3d, not just 2d.
		auto width = edgeLength(corners[0]->vert, corners[1]->vert) + edgeLength(corners[2]->vert, corners[3]->vert);
		auto height = edgeLength(corners[0]->vert, corners[2]->vert) + edgeLength(corners[1]->vert, corners[3]->vert);

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

		FaceCorner* stackCorners[4] = {corners[0], corners[1], corners[2], corners[3]};
		if (width < height) {
			eastl::swap(stackCorners[1], stackCorners[2]);
		}

		auto scratchPadDepth = 2 * subDivisionDepth;

		auto& midPoints01 = stFaceCorners[4 + scratchPadDepth];
		auto& midPoints23 = stFaceCorners[5 + scratchPadDepth];

		// split left right, mid points of TL (v0), TR (v1)
		midPoints01.vert = midVert(stackCorners[0]->vert, stackCorners[1]->vert),
		midPoints23.vert = midVert(stackCorners[2]->vert, stackCorners[3]->vert);

		psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(midPoints01.vert);
		psyqo::GTE::Kernels::rtps();
		midPoints01.ir = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
		psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&midPoints01.projected.packed);
		psyqo::GTE::read<psyqo::GTE::Register::SZ3>(&midPoints01.depth);

		psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(midPoints23.vert);
		psyqo::GTE::Kernels::rtps();
		midPoints23.ir = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
		psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&midPoints23.projected.packed);
		psyqo::GTE::read<psyqo::GTE::Register::SZ3>(&midPoints23.depth);

		midPoints01.uv.u = (stackCorners[0]->uv.u + stackCorners[1]->uv.u) >> 1;
		midPoints01.uv.v = (stackCorners[0]->uv.v + stackCorners[1]->uv.v) >> 1;
		midPoints23.uv.u = (stackCorners[2]->uv.u + stackCorners[3]->uv.u) >> 1;
		midPoints23.uv.v = (stackCorners[2]->uv.v + stackCorners[3]->uv.v) >> 1;

		midPoints01.colour = midColour(stackCorners[0]->colour, stackCorners[1]->colour);
		midPoints23.colour = midColour(stackCorners[2]->colour, stackCorners[3]->colour);

		FaceCorner* quadA[4] = {stackCorners[0], &midPoints01, stackCorners[2], &midPoints23};
		FaceCorner* quadB[4] = {&midPoints01, stackCorners[1], &midPoints23, stackCorners[3]};

		if (faceData.isAdvancedFog && !faceData.useQuickAdvancedFogPass) {
			midPoints01.fogColour = midColour(stackCorners[0]->fogColour, stackCorners[1]->fogColour);
			midPoints23.fogColour = midColour(stackCorners[2]->fogColour, stackCorners[3]->fogColour);
		}

		if (width < height) {
			eastl::swap(quadA[1], quadA[2]);
			eastl::swap(quadB[1], quadB[2]);
		}

		ProcessQuadNode(quadA, subDivisionDepth + 1, faceData);
		ProcessQuadNode(quadB, subDivisionDepth + 1, faceData);
		return;
	} // OTHERWISE continue to process and add to OT

	bool isAdvancedFog = faceData.isAdvancedFog &&
						 (corners[0]->ir > 0 || corners[1]->ir > 0 || corners[2]->ir > 0 || corners[3]->ir > 0) &&
						 !faceData.useQuickAdvancedFogPass;

	// update overlay min/max points
	if (faceData.isAdvancedFog && faceData.useQuickAdvancedFogPass) {
		for (int i = 0; i < 4; i++) {
			m_qafOverlayMinX = eastl::min(m_qafOverlayMinX, corners[i]->projected.x);
			m_qafOverlayMinY = eastl::min(m_qafOverlayMinY, corners[i]->projected.y);
			m_qafOverlayMaxX = eastl::max(m_qafOverlayMaxX, corners[i]->projected.x);
			m_qafOverlayMaxY = eastl::max(m_qafOverlayMaxY, corners[i]->projected.y);
		}
		m_qafSmallestZ = eastl::min(m_qafSmallestZ, zIndex);
	}

	// textured
	if (faceData.texture != nullptr) {
		if (!isAdvancedFog || !(faceData.texture->flags & (1 << 0))) {
			auto& quadPrim = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
			quadPrim.primitive.pointA = corners[0]->projected;
			quadPrim.primitive.pointB = corners[1]->projected;
			quadPrim.primitive.pointC = corners[2]->projected;
			quadPrim.primitive.pointD = corners[3]->projected;

			quadPrim.primitive.setColorA(corners[0]->colour);
			quadPrim.primitive.setColorB(corners[1]->colour);
			quadPrim.primitive.setColorC(corners[2]->colour);
			quadPrim.primitive.setColorD(corners[3]->colour);
			if (!isAdvancedFog) {
				quadPrim.primitive.setOpaque();
			} else {
				quadPrim.primitive.setSemiTrans();
			}

			quadPrim.primitive.tpage = faceData.tpage;

			if (faceData.texture->hasClut) {
				quadPrim.primitive.clutIndex = {faceData.texture->clutX, faceData.texture->clutY};
			}

			quadPrim.primitive.uvA = {corners[0]->uv.u,
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - corners[0]->uv.v)};
			quadPrim.primitive.uvB = {corners[1]->uv.u,
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - corners[1]->uv.v)};
			quadPrim.primitive.uvC = {corners[2]->uv.u,
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - corners[2]->uv.v)};
			quadPrim.primitive.uvD = {corners[3]->uv.u,
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - corners[3]->uv.v)};

			faceData.orderingTable.insert(quadPrim, zIndex);

			// we insert the untextured fog prim last because `insert` puts each new prim at the front of the bucket.
			if (isAdvancedFog) {
				auto& quadFogPrim = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudQuad>();
				quadFogPrim.primitive.pointA = corners[0]->projected;
				quadFogPrim.primitive.pointB = corners[1]->projected;
				quadFogPrim.primitive.pointC = corners[2]->projected;
				quadFogPrim.primitive.pointD = corners[3]->projected;

				quadFogPrim.primitive.setColorA(corners[0]->fogColour);
				quadFogPrim.primitive.setColorB(corners[1]->fogColour);
				quadFogPrim.primitive.setColorC(corners[2]->fogColour);
				quadFogPrim.primitive.setColorD(corners[3]->fogColour);
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
			fog.primitive.pointA = corners[0]->projected;
			fog.primitive.pointB = corners[1]->projected;
			fog.primitive.pointC = corners[2]->projected;
			fog.primitive.pointD = corners[3]->projected;

			fog.primitive.setColorA(corners[0]->fogColour);
			fog.primitive.setColorB(corners[1]->fogColour);
			fog.primitive.setColorC(corners[2]->fogColour);
			fog.primitive.setColorD(corners[3]->fogColour);
			fog.primitive.setSemiTrans();
			faceData.orderingTable.insert(fog, zIndex);

			auto& test = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
			test.primitive = {psyqo::Prim::MaskControl::FromSource, psyqo::Prim::MaskControl::Test::Yes};
			faceData.orderingTable.insert(test, zIndex);

			auto& textured = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
			textured.primitive.pointA = corners[0]->projected;
			textured.primitive.pointB = corners[1]->projected;
			textured.primitive.pointC = corners[2]->projected;
			textured.primitive.pointD = corners[3]->projected;

			textured.primitive.setColorA(corners[0]->colour);
			textured.primitive.setColorB(corners[1]->colour);
			textured.primitive.setColorC(corners[2]->colour);
			textured.primitive.setColorD(corners[3]->colour);
			textured.primitive.setOpaque();

			textured.primitive.tpage = faceData.tpage;
			if (faceData.texture->hasClut) {
				textured.primitive.clutIndex = {faceData.texture->clutX, faceData.texture->clutY};
			}

			textured.primitive.uvA = {corners[0]->uv.u,
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - corners[0]->uv.v)};
			textured.primitive.uvB = {corners[1]->uv.u,
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - corners[1]->uv.v)};
			textured.primitive.uvC = {corners[2]->uv.u,
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - corners[2]->uv.v)};
			textured.primitive.uvD = {corners[3]->uv.u,
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - corners[3]->uv.v)};
			faceData.orderingTable.insert(textured, zIndex);

			auto& fromSource = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
			fromSource.primitive = {psyqo::Prim::MaskControl::FromSource};
			faceData.orderingTable.insert(fromSource, zIndex);

			auto& marker = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudQuad>();
			marker.primitive.pointA = corners[0]->projected;
			marker.primitive.pointB = corners[1]->projected;
			marker.primitive.pointC = corners[2]->projected;
			marker.primitive.pointD = corners[3]->projected;

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
		quadPrim.primitive.pointA = corners[0]->projected;
		quadPrim.primitive.pointB = corners[1]->projected;
		quadPrim.primitive.pointC = corners[2]->projected;
		quadPrim.primitive.pointD = corners[3]->projected;

		if (faceData.isAdvancedFog && !faceData.useQuickAdvancedFogPass) {
			psyqo::Color col[4];
			for (int i = 0; i < 4; i++) {
				col[i].r = eastl::min(255, corners[i]->colour.r + corners[i]->fogColour.r);
				col[i].g = eastl::min(255, corners[i]->colour.g + corners[i]->fogColour.g);
				col[i].b = eastl::min(255, corners[i]->colour.b + corners[i]->fogColour.b);
			}

			quadPrim.primitive.setColorA(col[0]);
			quadPrim.primitive.setColorB(col[1]);
			quadPrim.primitive.setColorC(col[2]);
			quadPrim.primitive.setColorD(col[3]);
		} else {
			quadPrim.primitive.setColorA(corners[0]->colour);
			quadPrim.primitive.setColorB(corners[1]->colour);
			quadPrim.primitive.setColorC(corners[2]->colour);
			quadPrim.primitive.setColorD(corners[3]->colour);
		}
		quadPrim.primitive.setOpaque();

		faceData.orderingTable.insert(quadPrim, zIndex);
	}
}

void Renderer::ProcessMeshTri(const RendererTri& tri, int8_t depth, const FaceData& faceData) {
	auto* verts = tri.boneAdjustedVerts ? tri.boneAdjustedVerts : tri.verts;
	static constexpr uint8_t kCornerCount = 3;

	// vertex 1->3
	for (int i = 0; i < kCornerCount; i++) {
		psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(verts[i]);
		psyqo::GTE::Kernels::rtps();
		stFaceCorners[i].ir = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
	}

	// figure out nclip and skip rendering if backfaced
	psyqo::GTE::Kernels::nclip();
	if ((int32_t)psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0>() == 0) {
		return;
	}

	// read projected verts
	psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&stFaceCorners[0].projected.packed);
	psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&stFaceCorners[1].projected.packed);
	psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&stFaceCorners[2].projected.packed);

	// read all the SZ data
	psyqo::GTE::read<psyqo::GTE::Register::SZ1>(&stFaceCorners[0].depth);
	psyqo::GTE::read<psyqo::GTE::Register::SZ2>(&stFaceCorners[1].depth);
	psyqo::GTE::read<psyqo::GTE::Register::SZ3>(&stFaceCorners[2].depth);

	// we can skip a node if its going to be fully obscured by fog anyway
	if (faceData.isAdvancedFog && stFaceCorners[0].ir >= 4096 && stFaceCorners[1].ir >= 4096 &&
		stFaceCorners[2].ir >= 4096) {
		return;
	}

	auto shouldSubDivide =
		!tri.boneAdjustedVerts &&
		(stFaceCorners[0].depth >= SUBDIVISION_MIN_DISTANCE || stFaceCorners[1].depth >= SUBDIVISION_MIN_DISTANCE ||
		 stFaceCorners[2].depth >= SUBDIVISION_MIN_DISTANCE) &&
		(stFaceCorners[0].depth <= SUBDIVISION_MAX_DISTANCE || stFaceCorners[1].depth <= SUBDIVISION_MAX_DISTANCE ||
		 stFaceCorners[2].depth <= SUBDIVISION_MAX_DISTANCE);

	if (shouldSubDivide) {
		// copy any remaining data we couldn't read straight from the GTE
		for (int i = 0; i < kCornerCount; i++) {
			stFaceCorners[i].vert = tri.verts[i];
			stFaceCorners[i].colour = tri.colours[i];
		}

		// apply fog colours (skipped if advanced fog is enabled)
		if (!faceData.isAdvancedFog) {
			for (int i = 0; i < kCornerCount; i++) {
				ApplyAmbientToColour(&stFaceCorners[i].colour);
				stFaceCorners[i].colour = ApplyFogToColourGTE(stFaceCorners[i].colour, stFaceCorners[i].ir);
			}
		} else {
			if (!faceData.useQuickAdvancedFogPass) {
				GTESetFarColour({0, 0, 0});
				for (int i = 0; i < kCornerCount; i++) {
					ApplyAmbientToColour(&stFaceCorners[i].colour);
					stFaceCorners[i].colour = GTEColourLerp(stFaceCorners[i].colour, stFaceCorners[i].ir);
				}

				GTESetFarColour(m_lighting->GetFogColour());
				for (int i = 0; i < kCornerCount; i++) {
					stFaceCorners[i].fogColour = GTEColourLerp({0, 0, 0}, stFaceCorners[i].ir);
				}
			} else {
				for (int i = 0; i < kCornerCount; i++) {
					auto density = 4096 - stFaceCorners[i].ir;
					ApplyAmbientToColour(&stFaceCorners[i].colour);
					stFaceCorners[i].colour = {uint8_t((stFaceCorners[i].colour.r * density) >> 12),
											   uint8_t((stFaceCorners[i].colour.g * density) >> 12),
											   uint8_t((stFaceCorners[i].colour.b * density) >> 12)};
					m_qafIR0 += stFaceCorners[i].ir;
				}

				// average the quick fog t scale
				m_qafIROCount += kCornerCount;
			}
		}

		stFaceCorners[0].uv = {static_cast<uint8_t>(tri.uvA.u + faceData.uvOffset.pos.x), tri.uvA.v};
		stFaceCorners[1].uv = {static_cast<uint8_t>(tri.uvB.u + faceData.uvOffset.pos.x), tri.uvB.v};
		stFaceCorners[2].uv = {static_cast<uint8_t>(tri.uvC.u + faceData.uvOffset.pos.x), tri.uvC.v};

		FaceCorner* corners[kCornerCount] = {&stFaceCorners[0], &stFaceCorners[1], &stFaceCorners[2]};
		ProcessTriNode(corners, depth, faceData);
	} else {
		// it won't be sub divided so just emit it out
		// figure out avg z index
		psyqo::GTE::Kernels::avsz3();

		// keep inside OT bounds
		uint32_t zIndex = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ>();
		if (zIndex == 0 || zIndex >= ORDERING_TABLE_SIZE) {
			return;
		}

		// make sure we're not past screen bounds
		if (TriClip(stFaceCorners[0].projected, stFaceCorners[1].projected, stFaceCorners[2].projected)) {
			return;
		}

		// apply fog colours
		eastl::array<psyqo::Color, kCornerCount> vertexColours = {tri.colours[0], tri.colours[1], tri.colours[2]};
		eastl::array<psyqo::Color, kCornerCount> fogColours;

		if (!faceData.isAdvancedFog) {
			for (int i = 0; i < kCornerCount; i++) {
				ApplyAmbientToColour(&vertexColours[i]);
				vertexColours[i] = ApplyFogToColourGTE(vertexColours[i], stFaceCorners[i].ir);
			}
		} else {
			if (!faceData.useQuickAdvancedFogPass) {
				GTESetFarColour({0, 0, 0});
				for (int i = 0; i < kCornerCount; i++) {
					ApplyAmbientToColour(&vertexColours[i]);
					vertexColours[i] = GTEColourLerp(vertexColours[i], stFaceCorners[i].ir);
				}

				GTESetFarColour(m_lighting->GetFogColour());
				for (int i = 0; i < kCornerCount; i++) {
					fogColours[i] = GTEColourLerp({0, 0, 0}, stFaceCorners[i].ir);
				}
			} else {
				for (int i = 0; i < kCornerCount; i++) {
					auto density = 4096 - stFaceCorners[i].ir;
					ApplyAmbientToColour(&vertexColours[i]);
					vertexColours[i] = {uint8_t((vertexColours[i].r * density) >> 12),
										uint8_t((vertexColours[i].g * density) >> 12),
										uint8_t((vertexColours[i].b * density) >> 12)};
					m_qafIR0 += stFaceCorners[i].ir;
				}

				m_qafIROCount += kCornerCount;
			}
		}

		bool isAdvancedFog = faceData.isAdvancedFog &&
							 (stFaceCorners[0].ir > 0 || stFaceCorners[1].ir > 0 || stFaceCorners[2].ir > 0) &&
							 !faceData.useQuickAdvancedFogPass;

		// update overlay min/max points
		if (faceData.isAdvancedFog && faceData.useQuickAdvancedFogPass) {
			for (int i = 0; i < kCornerCount; i++) {
				m_qafOverlayMinX = eastl::min(m_qafOverlayMinX, stFaceCorners[i].projected.x);
				m_qafOverlayMinY = eastl::min(m_qafOverlayMinY, stFaceCorners[i].projected.y);
				m_qafOverlayMaxX = eastl::max(m_qafOverlayMaxX, stFaceCorners[i].projected.x);
				m_qafOverlayMaxY = eastl::max(m_qafOverlayMaxY, stFaceCorners[i].projected.y);
			}

			m_qafSmallestZ = eastl::min(m_qafSmallestZ, zIndex);
		}

		if (faceData.texture != nullptr) {
			if (!isAdvancedFog || !(faceData.texture->flags & (1 << 0))) {
				auto& triPrim = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
				triPrim.primitive.pointA = stFaceCorners[0].projected;
				triPrim.primitive.pointB = stFaceCorners[1].projected;
				triPrim.primitive.pointC = stFaceCorners[2].projected;

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
					triFogPrim.primitive.pointA = stFaceCorners[0].projected;
					triFogPrim.primitive.pointB = stFaceCorners[1].projected;
					triFogPrim.primitive.pointC = stFaceCorners[2].projected;

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
				fog.primitive.pointA = stFaceCorners[0].projected;
				fog.primitive.pointB = stFaceCorners[1].projected;
				fog.primitive.pointC = stFaceCorners[2].projected;

				fog.primitive.setColorA(fogColours[0]);
				fog.primitive.setColorB(fogColours[1]);
				fog.primitive.setColorC(fogColours[2]);
				fog.primitive.setSemiTrans();
				faceData.orderingTable.insert(fog, zIndex);

				auto& test = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
				test.primitive = {psyqo::Prim::MaskControl::FromSource, psyqo::Prim::MaskControl::Test::Yes};
				faceData.orderingTable.insert(test, zIndex);

				auto& textured = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
				textured.primitive.pointA = stFaceCorners[0].projected;
				textured.primitive.pointB = stFaceCorners[1].projected;
				textured.primitive.pointC = stFaceCorners[2].projected;

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
				marker.primitive.pointA = stFaceCorners[0].projected;
				marker.primitive.pointB = stFaceCorners[1].projected;
				marker.primitive.pointC = stFaceCorners[2].projected;

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
			triPrim.primitive.pointA = stFaceCorners[0].projected;
			triPrim.primitive.pointB = stFaceCorners[1].projected;
			triPrim.primitive.pointC = stFaceCorners[2].projected;

			if (faceData.isAdvancedFog && !faceData.useQuickAdvancedFogPass) {
				psyqo::Color col[kCornerCount];
				for (int i = 0; i < kCornerCount; i++) {
					col[i].r = eastl::min(255, vertexColours[i].r + fogColours[i].r);
					col[i].g = eastl::min(255, vertexColours[i].g + fogColours[i].g);
					col[i].b = eastl::min(255, vertexColours[i].b + fogColours[i].b);
				}

				triPrim.primitive.setColorA(col[0]);
				triPrim.primitive.setColorB(col[1]);
				triPrim.primitive.setColorC(col[2]);
			} else {
				triPrim.primitive.setColorA(vertexColours[0]);
				triPrim.primitive.setColorB(vertexColours[1]);
				triPrim.primitive.setColorC(vertexColours[2]);
			}
			triPrim.primitive.setOpaque();

			faceData.orderingTable.insert(triPrim, zIndex);
		}
	}
}
void Renderer::ProcessTriNode(FaceCorner* const* corners, uint8_t subDivisionDepth, const FaceData& faceData) {
	// depth == 0 is handled by `ProcessMeshTri`
	if (subDivisionDepth > 0) {
		// write the projected values back into sz*
		psyqo::GTE::write<psyqo::GTE::Register::SZ1, psyqo::GTE::Unsafe>(corners[0]->depth);
		psyqo::GTE::write<psyqo::GTE::Register::SZ2, psyqo::GTE::Unsafe>(corners[1]->depth);
		psyqo::GTE::write<psyqo::GTE::Register::SZ3, psyqo::GTE::Safe>(corners[2]->depth);
	}

	// figure out avg z index
	psyqo::GTE::Kernels::avsz3();

	// keep inside OT bounds
	uint32_t zIndex = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ>();
	if (zIndex == 0 || zIndex >= ORDERING_TABLE_SIZE) {
		return;
	}

	// clip screen space
	if (TriClip(corners[0]->projected, corners[1]->projected, corners[2]->projected)) {
		return;
	}

	// sub divide if we have depth and we're close enough
	if (subDivisionDepth <= SUBDIVISION_MAX_DEPTH) {
		auto abs = [](int32_t v) -> int32_t { return v < 0 ? -v : v; };
		auto edgeLength = [&abs](const psyqo::GTE::PackedVec3& a, const psyqo::GTE::PackedVec3& b) -> auto {
			return abs(a.x.value - b.x.value) + abs(a.y.value - b.y.value) + abs(a.z.value - b.z.value);
		};

		// measure the edges in 3d, not just 2d.
		const int32_t len01 = edgeLength(corners[0]->vert, corners[1]->vert);
		const int32_t len12 = edgeLength(corners[1]->vert, corners[2]->vert);
		const int32_t len20 = edgeLength(corners[2]->vert, corners[0]->vert);

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

		// do some fancy starting vert index so we don't need 10 million if statements
		int k = 0; // k being the corner where the longest edge starts
		if (len12 > len01) {
			k = 1;
		}
		if (len20 > (k ? len12 : len01)) {
			k = 2;
		}

		static constexpr uint8_t next[5] = {0, 1, 2, 0, 1};
		FaceCorner* stackCorners[3] = {corners[k], corners[next[k + 1]], corners[next[k + 2]]};
		auto scratchPadDepth = 2 * subDivisionDepth;

		auto& midPoints01 = stFaceCorners[3 + (2 * subDivisionDepth)];
		midPoints01.vert = midVert(stackCorners[0]->vert, stackCorners[1]->vert);

		psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(midPoints01.vert);
		psyqo::GTE::Kernels::rtps();
		midPoints01.ir = psyqo::GTE::readRaw<psyqo::GTE::Register::IR0>();
		psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&midPoints01.projected.packed);
		psyqo::GTE::read<psyqo::GTE::Register::SZ3>(&midPoints01.depth);

		midPoints01.uv.u = (stackCorners[0]->uv.u + stackCorners[1]->uv.u) >> 1;
		midPoints01.uv.v = (stackCorners[0]->uv.v + stackCorners[1]->uv.v) >> 1;

		midPoints01.colour = midColour(stackCorners[0]->colour, stackCorners[1]->colour);
		FaceCorner* triA[3] = {stackCorners[0], &midPoints01, stackCorners[2]};
		FaceCorner* triB[3] = {&midPoints01, stackCorners[1], stackCorners[2]};

		if (faceData.isAdvancedFog && !faceData.useQuickAdvancedFogPass) {
			midPoints01.fogColour = midColour(stackCorners[0]->fogColour, stackCorners[1]->fogColour);
		}

		ProcessTriNode(triA, subDivisionDepth + 1, faceData);
		ProcessTriNode(triB, subDivisionDepth + 1, faceData);
		return;
	} // OTHERWISE continue to process and add to OT

	// textured
	bool isAdvancedFog = faceData.isAdvancedFog && (corners[0]->ir > 0 || corners[1]->ir > 0 || corners[2]->ir > 0) &&
						 !faceData.useQuickAdvancedFogPass;

	// update overlay min/max points
	if (faceData.isAdvancedFog && faceData.useQuickAdvancedFogPass) {
		for (int i = 0; i < 3; i++) {
			m_qafOverlayMinX = eastl::min(m_qafOverlayMinX, corners[i]->projected.x);
			m_qafOverlayMinY = eastl::min(m_qafOverlayMinY, corners[i]->projected.y);
			m_qafOverlayMaxX = eastl::max(m_qafOverlayMaxX, corners[i]->projected.x);
			m_qafOverlayMaxY = eastl::max(m_qafOverlayMaxY, corners[i]->projected.y);
		}
		m_qafSmallestZ = eastl::min(m_qafSmallestZ, zIndex);
	}

	if (faceData.texture != nullptr) {
		if (!isAdvancedFog || !(faceData.texture->flags & (1 << 0))) {
			auto& triPrim = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
			triPrim.primitive.pointA = corners[0]->projected;
			triPrim.primitive.pointB = corners[1]->projected;
			triPrim.primitive.pointC = corners[2]->projected;

			triPrim.primitive.setColorA(corners[0]->colour);
			triPrim.primitive.setColorB(corners[1]->colour);
			triPrim.primitive.setColorC(corners[2]->colour);
			if (!isAdvancedFog) {
				triPrim.primitive.setOpaque();
			} else {
				triPrim.primitive.setSemiTrans();
			}

			triPrim.primitive.tpage = faceData.tpage;

			if (faceData.texture->hasClut) {
				triPrim.primitive.clutIndex = {faceData.texture->clutX, faceData.texture->clutY};
			}

			triPrim.primitive.uvA = {corners[0]->uv.u,
									 static_cast<uint8_t>(faceData.uvOffset.pos.y - corners[0]->uv.v)};
			triPrim.primitive.uvB = {corners[1]->uv.u,
									 static_cast<uint8_t>(faceData.uvOffset.pos.y - corners[1]->uv.v)};
			triPrim.primitive.uvC = {corners[2]->uv.u,
									 static_cast<uint8_t>(faceData.uvOffset.pos.y - corners[2]->uv.v)};

			faceData.orderingTable.insert(triPrim, zIndex);

			// we insert the untextured fog prim last because `insert` puts each new prim at the front of the bucket.
			if (isAdvancedFog) {
				auto& triFogPrim = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTriangle>();
				triFogPrim.primitive.pointA = corners[0]->projected;
				triFogPrim.primitive.pointB = corners[1]->projected;
				triFogPrim.primitive.pointC = corners[2]->projected;

				triFogPrim.primitive.setColorA(corners[0]->fogColour);
				triFogPrim.primitive.setColorB(corners[1]->fogColour);
				triFogPrim.primitive.setColorC(corners[2]->fogColour);
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
			fog.primitive.pointA = corners[0]->projected;
			fog.primitive.pointB = corners[1]->projected;
			fog.primitive.pointC = corners[2]->projected;

			fog.primitive.setColorA(corners[0]->fogColour);
			fog.primitive.setColorB(corners[1]->fogColour);
			fog.primitive.setColorC(corners[2]->fogColour);
			fog.primitive.setSemiTrans();
			faceData.orderingTable.insert(fog, zIndex);

			auto& test = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
			test.primitive = {psyqo::Prim::MaskControl::FromSource, psyqo::Prim::MaskControl::Test::Yes};
			faceData.orderingTable.insert(test, zIndex);

			auto& textured = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
			textured.primitive.pointA = corners[0]->projected;
			textured.primitive.pointB = corners[1]->projected;
			textured.primitive.pointC = corners[2]->projected;

			textured.primitive.setColorA(corners[0]->colour);
			textured.primitive.setColorB(corners[1]->colour);
			textured.primitive.setColorC(corners[2]->colour);
			textured.primitive.setOpaque();

			textured.primitive.tpage = faceData.tpage;
			if (faceData.texture->hasClut) {
				textured.primitive.clutIndex = {faceData.texture->clutX, faceData.texture->clutY};
			}

			textured.primitive.uvA = {corners[0]->uv.u,
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - corners[0]->uv.v)};
			textured.primitive.uvB = {corners[1]->uv.u,
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - corners[1]->uv.v)};
			textured.primitive.uvC = {corners[2]->uv.u,
									  static_cast<uint8_t>(faceData.uvOffset.pos.y - corners[2]->uv.v)};
			faceData.orderingTable.insert(textured, zIndex);

			auto& fromSource = faceData.bumpAllocator.allocateFragment<psyqo::Prim::MaskControl>();
			fromSource.primitive = {psyqo::Prim::MaskControl::FromSource};
			faceData.orderingTable.insert(fromSource, zIndex);

			auto& marker = faceData.bumpAllocator.allocateFragment<psyqo::Prim::GouraudTriangle>();
			marker.primitive.pointA = corners[0]->projected;
			marker.primitive.pointB = corners[1]->projected;
			marker.primitive.pointC = corners[2]->projected;

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
		triPrim.primitive.pointA = corners[0]->projected;
		triPrim.primitive.pointB = corners[1]->projected;
		triPrim.primitive.pointC = corners[2]->projected;

		if (faceData.isAdvancedFog && !faceData.useQuickAdvancedFogPass) {
			psyqo::Color col[3];
			for (int i = 0; i < 3; i++) {
				col[i].r = eastl::min(255, corners[i]->colour.r + corners[i]->fogColour.r);
				col[i].g = eastl::min(255, corners[i]->colour.g + corners[i]->fogColour.g);
				col[i].b = eastl::min(255, corners[i]->colour.b + corners[i]->fogColour.b);
			}

			triPrim.primitive.setColorA(col[0]);
			triPrim.primitive.setColorB(col[1]);
			triPrim.primitive.setColorC(col[2]);
		} else {
			triPrim.primitive.setColorA(corners[0]->colour);
			triPrim.primitive.setColorB(corners[1]->colour);
			triPrim.primitive.setColorC(corners[2]->colour);
		}
		triPrim.primitive.setOpaque();

		faceData.orderingTable.insert(triPrim, zIndex);
	}
}

psyqo::FixedPoint<> Renderer::GetFogFactor(uint32_t z) {
	if (m_fogFarDepth == 0) {
		return 0.0_fp;
	}

	if (z <= m_fogNearDepth) {
		return 0.0_fp;
	}

	if (z >= m_fogFarDepth) {
		return 1.0_fp;
	}

	return ((z - m_fogNearDepth) * 1.0_fp) / (m_fogFarDepth - m_fogNearDepth);
}

void Renderer::ApplyAmbientToColour(psyqo::Color* colA) {
	auto& ambient = m_lighting->GetAmbientColour();
	colA->r = eastl::min(255, (colA->r * ambient.r) >> 7);
	colA->g = eastl::min(255, (colA->g * ambient.g) >> 7);
	colA->b = eastl::min(255, (colA->b * ambient.b) >> 7);
}

void Renderer::ApplyAmbientToColours(psyqo::Color* colA, psyqo::Color* colB, psyqo::Color* colC) {
	auto& ambient = m_lighting->GetAmbientColour();
	colA->r = eastl::min(255, (colA->r * ambient.r) >> 7);
	colA->g = eastl::min(255, (colA->g * ambient.g) >> 7);
	colA->b = eastl::min(255, (colA->b * ambient.b) >> 7);
	colB->r = eastl::min(255, (colB->r * ambient.r) >> 7);
	colB->g = eastl::min(255, (colB->g * ambient.g) >> 7);
	colB->b = eastl::min(255, (colB->b * ambient.b) >> 7);
	colC->r = eastl::min(255, (colC->r * ambient.r) >> 7);
	colC->g = eastl::min(255, (colC->g * ambient.g) >> 7);
	colC->b = eastl::min(255, (colC->b * ambient.b) >> 7);
}

void Renderer::ApplyAmbientToColours(psyqo::Color* colA, psyqo::Color* colB, psyqo::Color* colC, psyqo::Color* colD) {
	auto& ambient = m_lighting->GetAmbientColour();
	colA->r = eastl::min(255, (colA->r * ambient.r) >> 7);
	colA->g = eastl::min(255, (colA->g * ambient.g) >> 7);
	colA->b = eastl::min(255, (colA->b * ambient.b) >> 7);
	colB->r = eastl::min(255, (colB->r * ambient.r) >> 7);
	colB->g = eastl::min(255, (colB->g * ambient.g) >> 7);
	colB->b = eastl::min(255, (colB->b * ambient.b) >> 7);
	colC->r = eastl::min(255, (colC->r * ambient.r) >> 7);
	colC->g = eastl::min(255, (colC->g * ambient.g) >> 7);
	colC->b = eastl::min(255, (colC->b * ambient.b) >> 7);
	colD->r = eastl::min(255, (colD->r * ambient.r) >> 7);
	colD->g = eastl::min(255, (colD->g * ambient.g) >> 7);
	colD->b = eastl::min(255, (colD->b * ambient.b) >> 7);
}

void Renderer::ApplyFogToColour(psyqo::Color* col, psyqo::FixedPoint<> fogFactor) {
	if (m_lighting->IsAdvancedFogEnabled() || !m_lighting->IsSimpleFogEnabled()) {
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
