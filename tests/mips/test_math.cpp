#include "math/gte-math.hh"
#include "math/lerp.hh"
#include "math/matrix.hh"
#include "math/trig.hh"
#include "math/vector.hh"

#include <third_party/snitch/snitch_all.hpp>

using namespace psyqo::fixed_point_literals;
using namespace psyqo::trig_literals;

static psyqo::Vec3 MakeVec3(psyqo::FixedPoint<> x, psyqo::FixedPoint<> y, psyqo::FixedPoint<> z) {
	psyqo::Vec3 v;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

static psyqo::Matrix33 MakeMatrix(psyqo::FixedPoint<> m00, psyqo::FixedPoint<> m01, psyqo::FixedPoint<> m02,
								  psyqo::FixedPoint<> m10, psyqo::FixedPoint<> m11, psyqo::FixedPoint<> m12,
								  psyqo::FixedPoint<> m20, psyqo::FixedPoint<> m21, psyqo::FixedPoint<> m22) {
	psyqo::Matrix33 m;
	m.vs[0] = MakeVec3(m00, m01, m02);
	m.vs[1] = MakeVec3(m10, m11, m12);
	m.vs[2] = MakeVec3(m20, m21, m22);
	return m;
}

static void RequireMatrixEquals(const psyqo::Matrix33& got, const psyqo::Matrix33& want) {
	for (int32_t row = 0; row < 3; row++) {
		REQUIRE(got.vs[row].x.value == want.vs[row].x.value);
		REQUIRE(got.vs[row].y.value == want.vs[row].y.value);
		REQUIRE(got.vs[row].z.value == want.vs[row].z.value);
	}
}

// --- Lerp / inverseLerp ---

TEST_CASE("Lerp returns the endpoints at t = 0 and t = 1") {
	REQUIRE(Lerp(2.0_fp, 6.0_fp, 0.0_fp).value == (2.0_fp).value);
	REQUIRE(Lerp(2.0_fp, 6.0_fp, 1.0_fp).value == (6.0_fp).value);
}

TEST_CASE("Lerp interpolates between the endpoints") {
	// 2 + (6 - 2) * 0.25 = 3, and 2 + (6 - 2) * 0.5 = 4
	REQUIRE(Lerp(2.0_fp, 6.0_fp, 0.25_fp).value == (3.0_fp).value);
	REQUIRE(Lerp(2.0_fp, 6.0_fp, 0.5_fp).value == (4.0_fp).value);
	// works in reverse and through zero: -1 + (3 - -1) * 0.75 = 2
	REQUIRE(Lerp(-1.0_fp, 3.0_fp, 0.75_fp).value == (2.0_fp).value);
}

TEST_CASE("inverseLerp gives the fraction of the way from a to b") {
	REQUIRE(inverseLerp(10, 30, 10).value == (0.0_fp).value);
	REQUIRE(inverseLerp(10, 30, 15).value == (0.25_fp).value);
	REQUIRE(inverseLerp(10, 30, 20).value == (0.5_fp).value);
	REQUIRE(inverseLerp(10, 30, 30).value == (1.0_fp).value);
}

// --- vectors ---

TEST_CASE("DotProduct of two vectors") {
	// 1*4 + 2*5 + 3*6 = 32
	REQUIRE(DotProduct(MakeVec3(1.0_fp, 2.0_fp, 3.0_fp), MakeVec3(4.0_fp, 5.0_fp, 6.0_fp)).value == (32.0_fp).value);
	// perpendicular unit axes
	REQUIRE(DotProduct(MakeVec3(1.0_fp, 0.0_fp, 0.0_fp), MakeVec3(0.0_fp, 1.0_fp, 0.0_fp)).value == 0);
	REQUIRE(DotProduct(MakeVec3(1.0_fp, 0.0_fp, 0.0_fp), MakeVec3(-1.0_fp, 0.0_fp, 0.0_fp)).value == (-1.0_fp).value);
}

TEST_CASE("Vec3 Lerp interpolates each component") {
	auto v = Lerp(MakeVec3(0.0_fp, 2.0_fp, -4.0_fp), MakeVec3(4.0_fp, 2.0_fp, 4.0_fp), 0.25_fp);
	REQUIRE(v.x.value == (1.0_fp).value);
	REQUIRE(v.y.value == (2.0_fp).value);
	REQUIRE(v.z.value == (-2.0_fp).value);
}

TEST_CASE("Vec2 Lerp interpolates each component") {
	psyqo::Vec2 a, b;
	a.x = 0.0_fp;
	a.y = 8.0_fp;
	b.x = 2.0_fp;
	b.y = 0.0_fp;
	auto v = Lerp(a, b, 0.5_fp);
	REQUIRE(v.x.value == (1.0_fp).value);
	REQUIRE(v.y.value == (4.0_fp).value);
}

TEST_CASE("IsVector3Zero is true for zero and false for a unit component") {
	REQUIRE(IsVector3Zero(MakeVec3(0.0_fp, 0.0_fp, 0.0_fp)));
	REQUIRE_FALSE(IsVector3Zero(MakeVec3(1.0_fp, 0.0_fp, 0.0_fp)));
	REQUIRE_FALSE(IsVector3Zero(MakeVec3(0.0_fp, -1.0_fp, 0.0_fp)));
	REQUIRE_FALSE(IsVector3Zero(MakeVec3(0.0_fp, 0.0_fp, 1.0_fp)));
}

// --- matrices ---

TEST_CASE("TransposeMatrix33 swaps rows and columns") {
	auto m = MakeMatrix(1.0_fp, 2.0_fp, 3.0_fp, 4.0_fp, 5.0_fp, 6.0_fp, 7.0_fp, 8.0_fp, 9.0_fp);
	RequireMatrixEquals(TransposeMatrix33(m),
						MakeMatrix(1.0_fp, 4.0_fp, 7.0_fp, 2.0_fp, 5.0_fp, 8.0_fp, 3.0_fp, 6.0_fp, 9.0_fp));
}

TEST_CASE("InverseMatrix33 inverts a scale matrix") {
	// det = 2 * 4 * 0.5 = 4
	auto m = MakeMatrix(2.0_fp, 0.0_fp, 0.0_fp, 0.0_fp, 4.0_fp, 0.0_fp, 0.0_fp, 0.0_fp, 0.5_fp);
	RequireMatrixEquals(InverseMatrix33(m),
						MakeMatrix(0.5_fp, 0.0_fp, 0.0_fp, 0.0_fp, 0.25_fp, 0.0_fp, 0.0_fp, 0.0_fp, 2.0_fp));
}

TEST_CASE("InverseMatrix33 inverts a shear matrix") {
	// [[1,1,0],[0,1,0],[0,0,1]] has det 1 and inverse [[1,-1,0],[0,1,0],[0,0,1]]
	auto m = MakeMatrix(1.0_fp, 1.0_fp, 0.0_fp, 0.0_fp, 1.0_fp, 0.0_fp, 0.0_fp, 0.0_fp, 1.0_fp);
	RequireMatrixEquals(InverseMatrix33(m),
						MakeMatrix(1.0_fp, -1.0_fp, 0.0_fp, 0.0_fp, 1.0_fp, 0.0_fp, 0.0_fp, 0.0_fp, 1.0_fp));
}

TEST_CASE("InverseMatrix33 of a rotation is its transpose") {
	// 90 degrees about z
	auto m = MakeMatrix(0.0_fp, -1.0_fp, 0.0_fp, 1.0_fp, 0.0_fp, 0.0_fp, 0.0_fp, 0.0_fp, 1.0_fp);
	RequireMatrixEquals(InverseMatrix33(m), TransposeMatrix33(m));
}

TEST_CASE("GTEMath::ProjectVectorOntoAxes multiplies by the axis matrix") {
	auto v = MakeVec3(0.5_fp, 0.25_fp, -0.75_fp);

	auto identity = MakeMatrix(1.0_fp, 0.0_fp, 0.0_fp, 0.0_fp, 1.0_fp, 0.0_fp, 0.0_fp, 0.0_fp, 1.0_fp);
	auto same = GTEMath::ProjectVectorOntoAxes(identity, v);
	REQUIRE(same.x.value == v.x.value);
	REQUIRE(same.y.value == v.y.value);
	REQUIRE(same.z.value == v.z.value);

	// rows are the axes: (0,-1,0).v = -0.25, (1,0,0).v = 0.5, (0,0,1).v = -0.75
	auto rotZ90 = MakeMatrix(0.0_fp, -1.0_fp, 0.0_fp, 1.0_fp, 0.0_fp, 0.0_fp, 0.0_fp, 0.0_fp, 1.0_fp);
	auto rotated = GTEMath::ProjectVectorOntoAxes(rotZ90, v);
	REQUIRE(rotated.x.value == (-0.25_fp).value);
	REQUIRE(rotated.y.value == (0.5_fp).value);
	REQUIRE(rotated.z.value == (-0.75_fp).value);
}

// --- angles ---

TEST_CASE("LerpAngle interpolates between two angles") {
	REQUIRE(LerpAngle(0.0_pi, 0.5_pi, psyqo::FixedPoint<10>(0.0)).value == (0.0_pi).value);
	REQUIRE(LerpAngle(0.0_pi, 0.5_pi, psyqo::FixedPoint<10>(0.5)).value == (0.25_pi).value);
	REQUIRE(LerpAngle(0.0_pi, 0.5_pi, psyqo::FixedPoint<10>(1.0)).value == (0.5_pi).value);
	REQUIRE(LerpAngle(-0.5_pi, 0.5_pi, psyqo::FixedPoint<10>(0.5)).value == (0.0_pi).value);
}

// From 162 degrees to -162 degrees the short way round is 36 degrees through 180, so the
// midpoint is +-1.0_pi. KNOWN FAILURE: LerpAngle interpolates the raw value and goes the long
// way, through 0.
TEST_CASE("LerpAngle takes the shortest way across the +-180 degree seam", "[!shouldfail]") {
	auto mid = LerpAngle(0.9_pi, -0.9_pi, psyqo::FixedPoint<10>(0.5));
	CHECK((mid.value == 1024 || mid.value == -1024));
}
