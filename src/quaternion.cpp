#include "quaternion.hh"
#include "psyqo/fixed-point.hh"
#include "psyqo/gte-registers.hh"
#include "psyqo/matrix.hh"
#include "psyqo/soft-math.hh"
#include "psyqo/trigonometry.hh"

using namespace psyqo::trig_literals;

void Quaternion::Normalize() {
  auto s = w * w + x * x + y * y + z * z;
  // avoid a div by zero error
  if (s == 0) {
    w = 1;
    x = y = z = 0;
    return;
  }

  auto r = psyqo::GTE::Short(1 / psyqo::SoftMath::squareRoot(psyqo::FixedPoint<>(s)));
  w *= r;
  x *= r;
  y *= r;
  z *= r;
}

psyqo::Matrix33 Quaternion::ToRotationMatrix() const {
  auto xx = x * x;
  auto yy = y * y;
  auto zz = z * z;
  auto xy = x * y;
  auto xz = x * z;
  auto yz = y * z;
  auto wx = w * x;
  auto wy = w * y;
  auto wz = w * z;

  auto one = psyqo::GTE::Short(1);
  auto two = psyqo::GTE::Short(2);

  return psyqo::Matrix33{{{
                              .x = psyqo::FixedPoint<>(one - two * (yy + zz)),
                              .y = psyqo::FixedPoint<>(two * (xy - wz)),
                              .z = psyqo::FixedPoint<>(two * (xz + wy)),
                          },
                          {
                              .x = psyqo::FixedPoint<>(two * (xy + wz)),
                              .y = psyqo::FixedPoint<>(one - two * (xx + zz)),
                              .z = psyqo::FixedPoint<>(two * (yz - wx)),
                          },
                          {
                              .x = psyqo::FixedPoint<>(two * (xz - wy)),
                              .y = psyqo::FixedPoint<>(two * (yz + wx)),
                              .z = psyqo::FixedPoint<>(one - two * (xx + yy)),
                          }}};
}

Quaternion FromEulerAngles(psyqo::Angle pitch, psyqo::Angle yaw, const psyqo::Trig<> &trig) {
  psyqo::GTE::Short cr = 0;
  psyqo::GTE::Short sr = 0;

  auto cosPitch = psyqo::GTE::Short(trig.cos(pitch / 2));
  auto sinPitch = psyqo::GTE::Short(trig.sin(pitch / 2));

  auto cosYaw = psyqo::GTE::Short(trig.cos(yaw / 2));
  auto sinYaw = psyqo::GTE::Short(trig.sin(yaw / 2));

  Quaternion q;
  q.w = cr * cosPitch * cosYaw + sr * sinPitch * sinYaw;
  q.x = sr * cosPitch * cosYaw - cr * sinPitch * sinYaw;
  q.y = cr * sinPitch * cosYaw + sr * cosPitch * sinYaw;
  q.z = cr * cosPitch * sinYaw - sr * sinPitch * cosYaw;

  return q;
};

Quaternion Slerp(const Quaternion &q1, const Quaternion &q2, psyqo::FixedPoint<> factor) {
  // find the dot product
  auto dotProduct = DotProduct(q1, q2);
  auto q2Actual = q2;

  // if its negative then negate q2 to get the shortest path
  if (dotProduct < 0) {
    q2Actual = {-q2.w, -q2.x, -q2.y, -q2.z};
    dotProduct = -dotProduct;
  }

  // lerp if almost identical
  auto q1Factor = psyqo::GTE::Short(1.0_fp - factor);
  auto q2Factor = psyqo::GTE::Short(factor);
  Quaternion slerpedQ = {q1Factor * q1.w + q2.w * q2Factor, q1Factor * q1.x + q2.x * q2Factor,
                         q1Factor * q1.y + q2.y * q2Factor, q1Factor * q1.z + q2.z * q2Factor};

  // give back a normiized lerp
  slerpedQ.Normalize();
  return slerpedQ;
}

Quaternion FindRotationQuat(const psyqo::Vec3 &v1, const psyqo::Vec3 &v2, psyqo::Trig<> &trig) {}

Quaternion operator*(const Quaternion &q1, const Quaternion &q2) {
  return Quaternion{
      q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z, // w
      q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y, // x
      q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x, // y
      q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w  // z
  };
}

Quaternion operator-(const Quaternion &q) {
    return Quaternion{
        q.w,     // w stays the same
        -q.x,    // negate vector part
        -q.y,
        -q.z
    };
}

psyqo::GTE::Short DotProduct(const Quaternion &a, const Quaternion &b) {
  return a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
}

