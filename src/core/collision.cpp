/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "collision.hh"
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

		// no overlap if distance is greater than radius
		if (distance > overlap) {
			return false;
		}

		// skip axis on negative penetration
		if (penetration <= 0) {
			continue;
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
