#include "vector.hh"
#include "psyqo/fixed-point.hh"
#include "psyqo/vector.hh"

using namespace psyqo::fixed_point_literals;

psyqo::FixedPoint<> DotProduct(const psyqo::Vec3 &a, const psyqo::Vec3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

bool IsVector3Zero(const psyqo::Vec3 &v) {
  // compare components against small threshold, not exact zero
    auto const eps = 100;
    return (v.x.abs()) <= eps && v.y.abs() <= eps && v.z.abs() <= eps;
}

psyqo::Vec2 Lerp(const psyqo::Vec2 &a, const psyqo::Vec2 &b, const psyqo::FixedPoint<> &t) {
  return a * (1.0_fp - t) + b * t;
}

psyqo::Vec3 Lerp(const psyqo::Vec3 &a, const psyqo::Vec3 &b, const psyqo::FixedPoint<> &t) {
  return a * (1.0_fp - t) + b * t;
}
