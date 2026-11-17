/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "collision.hh"
#include "../madnight.hh"
#include "../math/gte-math.hh"
#include "../math/vector.hh"
#include "../mesh/mesh_manager.hh"
#include <psyqo/fixed-point.hh>
#include <psyqo/gte-math.hh>
#include <psyqo/vector.hh>

// TODO: cache these AABBs somewhere?
// TODO: this obviously gets weird with rotated objects so might need to update later
[[deprecated("This should be done offline. DO NOT USE")]]
void Collision::GenerateAABBForMesh(const GameObject* object, AABBCollision* collisionBoxOut) {
	const MeshBin* mesh = object->mesh();

	// make sure it has faces
	if (mesh->facesCount == 0) {
		return;
	}

	// vec for min/max points
	collisionBoxOut->min.x.value = UINT16_MAX;
	collisionBoxOut->min.y.value = UINT16_MAX;
	collisionBoxOut->min.z.value = UINT16_MAX;
	collisionBoxOut->max.x.value = -UINT16_MAX;
	collisionBoxOut->max.y.value = -UINT16_MAX;
	collisionBoxOut->max.z.value = -UINT16_MAX;

	psyqo::Vec3 transformedPos = {0};
	psyqo::GteMath::matrixVecMul3(object->rotationMatrix(), object->pos(), &transformedPos);

	// for each vertex on the mesh
	for (int face = 0; face < mesh->facesCount; face++) {

		auto faceData = mesh->facesData[face];
		for (int i = 0; i < 4; i++) {
			// tris are only 3 verts not 4.
			if (!faceData.isQuad && i == 3) {
				continue;
			}

			auto vertPos = faceData.isQuad ? mesh->quads[faceData.rendererFaceIx].verts[i]
										   : mesh->tris[faceData.rendererFaceIx].verts[i];

			// mins
			if (vertPos.x.value < collisionBoxOut->min.x.value) {
				collisionBoxOut->min.x.value = vertPos.x.value;
			}

			if (vertPos.y.value < collisionBoxOut->min.y.value) {
				collisionBoxOut->min.y.value = vertPos.y.value;
			}

			if (vertPos.z.value < collisionBoxOut->min.z.value) {
				collisionBoxOut->min.z.value = vertPos.z.value;
			}

			// maxes
			if (vertPos.x.value > collisionBoxOut->max.x.value) {
				collisionBoxOut->max.x.value = vertPos.x.value;
			}

			if (vertPos.y.value > collisionBoxOut->max.y.value) {
				collisionBoxOut->max.y.value = vertPos.y.value;
			}

			if (vertPos.z.value > collisionBoxOut->max.z.value) {
				collisionBoxOut->max.z.value = vertPos.z.value;
			}
		}
	}
}

// this is super simple and doesn't care about rotations
bool Collision::IsAABBCollision(const AABBCollision& collisionA, const AABBCollision& collisionB) {
	// AABB box: check the min/max X
	if (collisionA.max.x < collisionB.min.x || collisionA.min.x > collisionB.max.x) {
		return false;
	}

	// AABB box: check the min/max Y
	if (collisionA.max.y < collisionB.min.y || collisionA.min.y > collisionB.max.y) {
		return false;
	}

	// AABB box: check the min/max Z
	if (collisionA.max.z < collisionB.min.z || collisionA.min.z > collisionB.max.z) {
		return false;
	}

	// ok so we know we're colliding
	return true;
}

// this is more complicated and does take into account object rotation properly
bool Collision::IsSATCollision(const OBB& collisionA, const OBB& collisionB, CollisionTest* resultOut) {
	auto minACentre = 0.0_fp, minBCentre = 0.0_fp;

	resultOut->mtv = psyqo::Vec3::ZERO();
	resultOut->normal = psyqo::Vec3::ZERO();
	resultOut->penetration = 1000.0_fp;

	psyqo::Vec3 collisionA0B0 = {0}, collisionA0B1 = {0}, collisionA0B2 = {0}, collisionA1B0 = {0}, collisionA1B1 = {0},
				collisionA1B2 = {0}, collisionA2B0 = {0}, collisionA2B1 = {0}, collisionA2B2 = {0};

	psyqo::GteMath::crossProductVec3(collisionA.axes[0], collisionB.axes[0], &collisionA0B0);
	psyqo::GteMath::crossProductVec3(collisionA.axes[0], collisionB.axes[1], &collisionA0B1);
	psyqo::GteMath::crossProductVec3(collisionA.axes[0], collisionB.axes[2], &collisionA0B2);
	psyqo::GteMath::crossProductVec3(collisionA.axes[1], collisionB.axes[0], &collisionA1B0);
	psyqo::GteMath::crossProductVec3(collisionA.axes[1], collisionB.axes[1], &collisionA1B1);
	psyqo::GteMath::crossProductVec3(collisionA.axes[1], collisionB.axes[2], &collisionA1B2);
	psyqo::GteMath::crossProductVec3(collisionA.axes[2], collisionB.axes[0], &collisionA2B0);
	psyqo::GteMath::crossProductVec3(collisionA.axes[2], collisionB.axes[1], &collisionA2B1);
	psyqo::GteMath::crossProductVec3(collisionA.axes[2], collisionB.axes[2], &collisionA2B2);

	eastl::array<psyqo::Vec3, 15> axesToCheck = {
		collisionA.axes[0], collisionA.axes[1], collisionA.axes[2], collisionB.axes[0], collisionB.axes[1],
		collisionB.axes[2], collisionA0B0,		collisionA0B1,		collisionA0B2,		collisionA1B0,
		collisionA1B1,		collisionA1B2,		collisionA2B0,		collisionA2B1,		collisionA2B2,
	};

	// we can skip this axes if we get a vec3 zero for any of these
	auto i = -1;
	for (auto& axis : axesToCheck) {
		i++;
		// skip this one if its zero
		if (axis.x == 0 && axis.y == 0 && axis.z == 0) {
			continue;
		}

		// normalize it
		auto normalizedAxis = axis;
		psyqo::GteMath::normalizeVec3(&normalizedAxis);

		// centre dot products
		auto aCentre = DotProduct(collisionA.center, normalizedAxis);
		auto bCentre = DotProduct(collisionB.center, normalizedAxis);

		// use the GTE to project the axes for us
		psyqo::Vec3 projectionA = GTEMath::ProjectVectorOntoAxes(
			{collisionA.axes[0], collisionA.axes[1], collisionA.axes[2]}, normalizedAxis);
		psyqo::Vec3 projectionB = GTEMath::ProjectVectorOntoAxes(
			{collisionB.axes[0], collisionB.axes[1], collisionB.axes[2]}, normalizedAxis);

		// now project the half extents
		auto aRadius = (projectionA.x.abs() * collisionA.halfExtents.x) +
					   (projectionA.y.abs() * collisionA.halfExtents.y) +
					   (projectionA.z.abs() * collisionA.halfExtents.z);
		auto bRadius = (projectionB.x.abs() * collisionB.halfExtents.x) +
					   (projectionB.y.abs() * collisionB.halfExtents.y) +
					   (projectionB.z.abs() * collisionB.halfExtents.z);

		// check for overlap
		auto distance = (aCentre - bCentre).abs();
		auto overlap = aRadius + bRadius;
		auto penetration = overlap - distance;

		// no overlap if distance is at least the radius. boxes that only touch are apart, otherwise
		// the touching axis would be skipped and the push would come from another one
		if (distance >= overlap) {
			return false;
		}

		// keep track of what the min penetration and axes we're pushed on is
		if (penetration < resultOut->penetration) {
			resultOut->penetration = penetration;

			resultOut->normal = normalizedAxis;
			if (aCentre < bCentre) {
				resultOut->normal = -resultOut->normal;
			}

			resultOut->mtv = resultOut->normal * resultOut->penetration;
		}
	}

	// no separated axis found, so we must be colliding
	return true;
}

void Collision::PotentialColbinWallCollisions(
	const OBB& obb, eastl::fixed_vector<OBB*, COLLISION_BROAD_PHASE_LIMIT>& potentialCollisions) {
	const auto& colBin = g_madnightEngine.m_colbinManager.Colbin();
	if (!colBin || !colBin->header.version) {
		return;
	}

	const auto& gridHeader = colBin->gridHeader;
	const auto& gridCells = colBin->gridCells;
	const auto& colliders = colBin->walls;

	potentialCollisions.clear();

	// convert the OBB into its reach of grid cells that it is overlapping
	// we only care about the ones under the obb, and this handles the obb being rotated
	int32_t reach[2] = {};		  // 0 = x, 1 = z, in raw fp12 value
	for (int i = 0; i < 3; i++) { // for each axis on the obb
		auto half = i == 0 ? obb.halfExtents.x : i == 1 ? obb.halfExtents.y : obb.halfExtents.z;
		reach[0] += (obb.axes[i].x.abs() * half).value;
		reach[1] += (obb.axes[i].z.abs() * half).value;
	}

	// now convert the world position to cell index (floor(position - origin) / cellSize), clamped to the grid
	auto cell = [&gridHeader](int32_t offset, int32_t count) {
		int32_t index = offset < 0 ? 0 : offset / int32_t(gridHeader.cellSize);
		return index < count ? index : count - 1;
	};

	// this is the block of cells that the obb touches, and will normally just be one of them
	auto x0 = cell(obb.center.x.value - reach[0] - gridHeader.originX, gridHeader.gridWidth);
	auto x1 = cell(obb.center.x.value + reach[0] - gridHeader.originX, gridHeader.gridWidth);
	auto z0 = cell(obb.center.z.value - reach[1] - gridHeader.originZ, gridHeader.gridHeight);
	auto z1 = cell(obb.center.z.value + reach[1] - gridHeader.originZ, gridHeader.gridHeight);

	// go and find the walls listed in those cells, making sure we only include each one once so we dont test
	// against the same one more than once and cause unexpected pushing
	for (auto x = x0; x <= x1; x++) {
		for (auto z = z0; z <= z1; z++) {
			const auto& gridCell = gridCells[x * gridHeader.gridHeight + z];
			for (int i = 0; i < gridCell.count; i++) {
				if (eastl::find(potentialCollisions.begin(), potentialCollisions.end(),
								&colliders[gridCell.indices[i]]) == potentialCollisions.end()) {
					potentialCollisions.push_back(&colliders[gridCell.indices[i]]);
				}
			}
		}
	}
}
