#ifndef _QUATERNION_H
#define _QUATERNION_H

#include "psyqo/fixed-point.hh"
#include "psyqo/gte-registers.hh"
#include "psyqo/matrix.hh"
#include "psyqo/trigonometry.hh"

using namespace psyqo::fixed_point_literals;

struct Quaternion {
  psyqo::GTE::Short w{1.0}, x{0.0}, y{0.0}, z{0.0};

  auto operator<=>(const Quaternion &) const = default;

  psyqo::Matrix33 ToRotationMatrix() const;
  void Normalize();
};

Quaternion operator*(const Quaternion &q1, const Quaternion &q2);

psyqo::GTE::Short DotProduct(const Quaternion &a, const Quaternion &b);

/* Works for small rotations only for now - okay for animation interpolation */
Quaternion Slerp(const Quaternion &q1, const Quaternion &q2, psyqo::FixedPoint<> factor);

/* Finds a rotation quaternion from v1 to v2 */
Quaternion FindRotationQuat(const psyqo::Vec3 &v1, const psyqo::Vec3 &v2, psyqo::Trig<> &trig);
Quaternion FromEulerAngles(psyqo::Angle pitch, psyqo::Angle yaw, const psyqo::Trig<> &trig);

#endif
