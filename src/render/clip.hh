#pragma once

#include <psyqo/primitives/common.hh>

#include <stdint.h>

static constexpr psyqo::Rect SCREEN_SPACE = {.pos = {0, 0}, .size = {320, 240}};

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
