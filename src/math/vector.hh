#ifndef _VECTOR_MATH_H
#define _VECTOR_MATH_H

#include "psyqo/fixed-point.hh"
#include "psyqo/vector.hh"

psyqo::FixedPoint<> DotProduct(const psyqo::Vec3 &a, const psyqo::Vec3 &b);
psyqo::Vec2 Lerp(const psyqo::Vec2 &a, const psyqo::Vec2 &b, const psyqo::FixedPoint<> &t);
psyqo::Vec3 Lerp(const psyqo::Vec3 &a, const psyqo::Vec3 &b, const psyqo::FixedPoint<> &t);
bool IsVector3Zero(const psyqo::Vec3 &v);

#endif
