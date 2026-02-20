#ifndef _VECTOR_MATH_H
#define _VECTOR_MATH_H

#include "psyqo/vector.hh"

psyqo::FixedPoint<> DotProduct(const psyqo::Vec3 &a, const psyqo::Vec3 &b);
bool IsVector3Zero(const psyqo::Vec3 &v);

#endif
