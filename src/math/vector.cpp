#include "vector.hh"
#include "psyqo/vector.hh"

psyqo::FixedPoint<> DotProduct(const psyqo::Vec3 &a, const psyqo::Vec3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
