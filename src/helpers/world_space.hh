#ifndef _WORLD_SPACE_H
#define _WORLD_SPACE_H

#include "psyqo/fixed-point.hh"

using namespace psyqo::fixed_point_literals;

// right now exporting from blender, 0.5 is 1m, 1.0 is 2m, etc.
// 1m, 0.5 blender units = 0.5_ws
// 2m, 1 blender units = 1.0_ws
consteval psyqo::FixedPoint<> operator""_ws(long double blenderUnits) {
    psyqo::FixedPoint<> output;
    output.value = blenderUnits * 128;
    return output;
}

#endif
