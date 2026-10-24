#include "animation/animation.hh"
#include "mesh/skeleton/skeleton.hh"
#include "quaternion.hh"

#include <third_party/snitch/snitch_all.hpp>

using namespace psyqo::fixed_point_literals;

// Unit quaternions in raw FixedPoint<12> units (4096 = 1.0), computed in double precision.
// Identity, and 90 degrees about z: (cos 45, 0, 0, sin 45) = 2896.31.
static constexpr int32_t ROT_Z90_W = 2896;
static constexpr int32_t ROT_Z90_Z = 2896;
// nlerp halfway between those is 45 degrees about z: (cos 22.5, 0, 0, sin 22.5).
static constexpr int32_t ROT_Z45_W = 3784; // 3784.21
static constexpr int32_t ROT_Z45_Z = 1567; // 1567.47

// Fixed-point products and the soft square root in Normalize round a little.
static constexpr int32_t QUAT_TOLERANCE = 8;

static int32_t Distance(int32_t a, int32_t b) { return a > b ? a - b : b - a; }

static Quaternion Identity() { return Quaternion{1.0_fp, 0.0_fp, 0.0_fp, 0.0_fp}; }

static Quaternion RotZ90() {
	Quaternion q;
	q.w.value = ROT_Z90_W;
	q.x = 0.0_fp;
	q.y = 0.0_fp;
	q.z.value = ROT_Z90_Z;
	return q;
}

static bool QuatNear(const Quaternion& q, int32_t w, int32_t x, int32_t y, int32_t z) {
	return Distance(q.w.value, w) <= QUAT_TOLERANCE && Distance(q.x.value, x) <= QUAT_TOLERANCE &&
		   Distance(q.y.value, y) <= QUAT_TOLERANCE && Distance(q.z.value, z) <= QUAT_TOLERANCE;
}

// Animation and Skeleton are large, so keep them off the stack.
static Animation s_animation;
static Skeleton s_skeleton;

// One track on bone 0: identity at frame 0, 90 degrees about z at frame 10.
static void SetUpAnimation(bool looped) {
	s_animation = Animation{};
	s_animation.flags = looped ? 1 : 0;
	s_animation.length = 20;
	s_animation.numTracks = 1;

	auto& track = s_animation.tracks[0];
	track.jointId = 0;
	track.numKeys = 2;
	track.keys[0].frame = 0;
	track.keys[0].keyType = KeyType::ROTATION;
	track.keys[0].rotation = Identity();
	track.keys[1].frame = 10;
	track.keys[1].keyType = KeyType::ROTATION;
	track.keys[1].rotation = RotZ90();

	s_skeleton = Skeleton{};
	s_skeleton.numBones = 1;
}

// --- Slerp (nlerp) ---

TEST_CASE("Slerp returns the endpoints at factor 0 and 1") {
	REQUIRE(QuatNear(Slerp(Identity(), RotZ90(), 0.0_fp), 4096, 0, 0, 0));
	REQUIRE(QuatNear(Slerp(Identity(), RotZ90(), 1.0_fp), ROT_Z90_W, 0, 0, ROT_Z90_Z));
}

TEST_CASE("Slerp halfway between identity and 90 degrees is 45 degrees") {
	REQUIRE(QuatNear(Slerp(Identity(), RotZ90(), 0.5_fp), ROT_Z45_W, 0, 0, ROT_Z45_Z));
}

// -q is the same rotation as q, so halfway from identity to -RotZ90 is still 45 degrees about z.
// KNOWN FAILURE: Slerp negates q2 into q2Actual when the dot product is negative, then
// interpolates with q2, so the short-way flip never takes effect.
TEST_CASE("Slerp takes the short way when the quaternions are in opposite hemispheres", "[!shouldfail]") {
	auto q = RotZ90();
	Quaternion negated{-q.w, -q.x, -q.y, -q.z};
	CHECK(QuatNear(Slerp(Identity(), negated, 0.5_fp), ROT_Z45_W, 0, 0, ROT_Z45_Z));
}

// --- SkeletonController animation playback ---

TEST_CASE("SetAnimation sets the animation and restarts it from frame 0") {
	SetUpAnimation(true);
	SkeletonController controller;

	s_skeleton.animationCurrentFrame = 7;
	controller.SetAnimation(&s_skeleton, &s_animation);
	REQUIRE(s_skeleton.animation == &s_animation);
	REQUIRE(s_skeleton.animationCurrentFrame == 0);

	// null arguments leave the skeleton alone
	s_skeleton.animationCurrentFrame = 3;
	controller.SetAnimation(&s_skeleton, nullptr);
	REQUIRE(s_skeleton.animation == &s_animation);
	REQUIRE(s_skeleton.animationCurrentFrame == 3);
}

TEST_CASE("PlayAnimation samples the key on its own frame and advances by the delta") {
	SetUpAnimation(true);
	SkeletonController controller;
	controller.SetAnimation(&s_skeleton, &s_animation);

	controller.PlayAnimation(&s_skeleton, 2);
	REQUIRE(QuatNear(s_skeleton.bones[0].localRotation, 4096, 0, 0, 0));
	REQUIRE(s_skeleton.bones[0].isDirty);
	REQUIRE(s_skeleton.animationCurrentFrame == 2);

	s_skeleton.animationCurrentFrame = 10;
	controller.PlayAnimation(&s_skeleton, 1);
	REQUIRE(QuatNear(s_skeleton.bones[0].localRotation, ROT_Z90_W, 0, 0, ROT_Z90_Z));
	REQUIRE(s_skeleton.animationCurrentFrame == 11);
}

TEST_CASE("PlayAnimation holds the last key after it") {
	SetUpAnimation(true);
	SkeletonController controller;
	controller.SetAnimation(&s_skeleton, &s_animation);

	s_skeleton.animationCurrentFrame = 15;
	controller.PlayAnimation(&s_skeleton, 1);
	REQUIRE(QuatNear(s_skeleton.bones[0].localRotation, ROT_Z90_W, 0, 0, ROT_Z90_Z));
}

TEST_CASE("PlayAnimation wraps a looped animation back to frame 0 at its length") {
	SetUpAnimation(true);
	SkeletonController controller;
	controller.SetAnimation(&s_skeleton, &s_animation);

	s_skeleton.animationCurrentFrame = 20;
	controller.PlayAnimation(&s_skeleton, 1);
	REQUIRE(QuatNear(s_skeleton.bones[0].localRotation, 4096, 0, 0, 0));
	REQUIRE(s_skeleton.animationCurrentFrame == 1);
}

TEST_CASE("PlayAnimation parks an animation that does not loop on its last frame") {
	SetUpAnimation(false);
	SkeletonController controller;
	controller.SetAnimation(&s_skeleton, &s_animation);

	s_skeleton.animationCurrentFrame = 20;
	controller.PlayAnimation(&s_skeleton, 1);
	REQUIRE(QuatNear(s_skeleton.bones[0].localRotation, ROT_Z90_W, 0, 0, ROT_Z90_Z));
	REQUIRE(s_skeleton.animationCurrentFrame == 20);
}

TEST_CASE("PlayAnimation does nothing without a skeleton or an animation") {
	SetUpAnimation(true);
	SkeletonController controller;

	controller.PlayAnimation(nullptr, 1);
	controller.PlayAnimation(&s_skeleton, 1);
	REQUIRE(s_skeleton.animationCurrentFrame == 0);
}

// Frame 5 is halfway from the key at 0 to the key at 10, so the bone should be at 45 degrees.
// KNOWN FAILURE: PlayAnimation computes the factor as an integer division,
// (currentFrame - prev->frame) / frameDiff, which is 0 for every frame strictly between two keys,
// so the bone snaps to the earlier key instead of interpolating.
TEST_CASE("PlayAnimation interpolates between keys", "[!shouldfail]") {
	SetUpAnimation(true);
	SkeletonController controller;
	controller.SetAnimation(&s_skeleton, &s_animation);

	s_skeleton.animationCurrentFrame = 5;
	controller.PlayAnimation(&s_skeleton, 1);
	CHECK(QuatNear(s_skeleton.bones[0].localRotation, ROT_Z45_W, 0, 0, ROT_Z45_Z));
}
