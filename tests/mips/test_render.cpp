#include "render/clip.hh"
#include "render/renderer.hh"

#include <third_party/snitch/snitch_all.hpp>

static psyqo::Vertex V(int16_t x, int16_t y) {
	psyqo::Vertex v;
	v.x = x;
	v.y = y;
	return v;
}

TEST_CASE("TestClip is 0 on every pixel of the screen edge") {
	CHECK(TestClip(0, 0) == 0);
	CHECK(TestClip(319, 0) == 0);
	CHECK(TestClip(0, 239) == 0);
	CHECK(TestClip(319, 239) == 0);
}

TEST_CASE("TestClip flags each side one pixel off the screen") {
	CHECK(TestClip(-1, 100) == CLIP_LEFT);
	CHECK(TestClip(320, 100) == CLIP_RIGHT);
	CHECK(TestClip(100, -1) == CLIP_TOP);
	CHECK(TestClip(100, 240) == CLIP_BOTTOM);
	CHECK(TestClip(-1, -1) == (CLIP_LEFT | CLIP_TOP));
	CHECK(TestClip(320, 240) == (CLIP_RIGHT | CLIP_BOTTOM));
}

TEST_CASE("QuadClip keeps a quad with any part on screen") {
	CHECK(QuadClip(V(10, 10), V(20, 10), V(10, 20), V(20, 20)) == 0);
	// one corner inside
	CHECK(QuadClip(V(-50, -50), V(5, -50), V(-50, 5), V(5, 5)) == 0);
	// straddles the left edge with no corner inside
	CHECK(QuadClip(V(-10, -10), V(400, -10), V(-10, 300), V(400, 300)) == 0);
	// cuts across the top-left corner: one corner left, one above, two off the corner
	CHECK(QuadClip(V(-10, 50), V(50, -10), V(-10, -10), V(-20, -20)) == 0);
}

TEST_CASE("QuadClip drops a quad entirely past one edge") {
	CHECK(QuadClip(V(-40, 10), V(-1, 10), V(-40, 50), V(-1, 50)) == 1);
	CHECK(QuadClip(V(320, 10), V(400, 10), V(320, 50), V(400, 50)) == 1);
	CHECK(QuadClip(V(10, -40), V(50, -40), V(10, -1), V(50, -1)) == 1);
	CHECK(QuadClip(V(10, 240), V(50, 240), V(10, 300), V(50, 300)) == 1);
	// wider than the screen but all above it
	CHECK(QuadClip(V(-100, -20), V(500, -20), V(-100, -5), V(500, -5)) == 1);
}

TEST_CASE("TriClip keeps a triangle with any part on screen") {
	CHECK(TriClip(V(10, 10), V(20, 10), V(10, 20)) == 0);
	CHECK(TriClip(V(-50, -50), V(5, 5), V(-50, 5)) == 0);
	CHECK(TriClip(V(-10, 120), V(400, 100), V(400, 140)) == 0);
	CHECK(TriClip(V(-10, 50), V(50, -10), V(-10, -10)) == 0);
}

TEST_CASE("TriClip drops a triangle entirely past one edge") {
	CHECK(TriClip(V(-40, 10), V(-1, 10), V(-40, 50)) == 1);
	CHECK(TriClip(V(320, 10), V(400, 10), V(320, 50)) == 1);
	CHECK(TriClip(V(10, -40), V(50, -40), V(10, -1)) == 1);
	CHECK(TriClip(V(10, 240), V(50, 240), V(10, 300)) == 1);
}

TEST_CASE("IsOutsideOrderingTable rejects depth 0 and depths past the last bucket") {
	CHECK(IsOutsideOrderingTable(0));
	CHECK_FALSE(IsOutsideOrderingTable(1));
	CHECK_FALSE(IsOutsideOrderingTable(ORDERING_TABLE_SIZE - 1));
	CHECK(IsOutsideOrderingTable(ORDERING_TABLE_SIZE));
	CHECK(IsOutsideOrderingTable(0xffff));
}
