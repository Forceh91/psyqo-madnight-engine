#include "vector.hh"
#include "psyqo/vector.hh"

using namespace psyqo::fixed_point_literals;

psyqo::FixedPoint<> DotProduct(const psyqo::Vec3 &a, const psyqo::Vec3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

bool IsVector3Zero(const psyqo::Vec3 &v) {
    auto const eps = 100;
    // compare components against small threshold, not exact zero
    auto abs = [](int32_t x) { return x < 0 ? -x : x; };
    return abs(v.x.value) <= eps && abs(v.y.value) <= eps && abs(v.z.value) <= eps;
}

psyqo::Vec2 Lerp(const psyqo::Vec2 &a, const psyqo::Vec2 &b, const psyqo::FixedPoint<> &t) {
  return a * (1.0_fp - t) + b * t;
}

psyqo::Vec3 Lerp(const psyqo::Vec3 &a, const psyqo::Vec3 &b, const psyqo::FixedPoint<> &t) {
  return a * (1.0_fp - t) + b * t;
}
