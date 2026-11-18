#pragma once

#include <psyqo/fixed-point.hh>
#include <psyqo/primitives/common.hh>
#include <psyqo/vector.hh>

struct PointLight {
	psyqo::Vec3 pos = {};
	psyqo::FixedPoint<> radius = 0;
	psyqo::Color colour = {};
	bool active = false;
};
