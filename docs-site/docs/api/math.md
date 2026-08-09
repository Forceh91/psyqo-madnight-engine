---
title: Math
sidebar_position: 7
---

# Math

`src/math/` and `src/rand.hh` — GTE-accelerated matrix/vector helpers, lerp, trig, and a lightweight RNG. All free functions or static-only classes; there's no state to manage beyond `Rand`, which you own an instance of yourself.

## GTEMath

`src/math/gte-math.hh`

```cpp
class GTEMath final {
public:
  static psyqo::Vec3 ProjectVectorOntoAxes(const psyqo::Matrix33 &axisMatrix, const psyqo::Vec3 &normalizedVec);
  static void MultiplyMatrix33(const psyqo::Matrix33 &rotationMatrixA, const psyqo::Matrix33 rotationMatrixB, psyqo::Matrix33 *out);
  static void MultiplyMatrixVec3(const psyqo::Matrix33 &rotationMatrix, const psyqo::Vec3 posVector, psyqo::Vec3 *out);
};
```

GTE-register-backed equivalents of the matrix math you'd otherwise do on the CPU — used wherever the engine needs matrix/vector multiplication in a hot path (camera, skeleton posing, collision).

## Matrix

`src/math/matrix.hh`

```cpp
psyqo::Matrix33 TransposeMatrix33(const psyqo::Matrix33 &rotationMatrix);
psyqo::Matrix33 InverseMatrix33(const psyqo::Matrix33 &rotationMatrix);
```

For an orthonormal rotation matrix, the transpose *is* the inverse — `InverseMatrix33` exists as the semantically clearer name to reach for at call sites (e.g. `Camera::inverseRotationMatrix`).

## Vector

`src/math/vector.hh`

```cpp
psyqo::FixedPoint<> DotProduct(const psyqo::Vec3 &a, const psyqo::Vec3 &b);
psyqo::Vec2 Lerp(const psyqo::Vec2 &a, const psyqo::Vec2 &b, const psyqo::FixedPoint<> &t);
psyqo::Vec3 Lerp(const psyqo::Vec3 &a, const psyqo::Vec3 &b, const psyqo::FixedPoint<> &t);
bool IsVector3Zero(const psyqo::Vec3 &v);
```

## Lerp

`src/math/lerp.hh`

```cpp
psyqo::FixedPoint<> Lerp(const psyqo::FixedPoint<>& a, const psyqo::FixedPoint<>& b, const psyqo::FixedPoint<>& t);
psyqo::FixedPoint<> inverseLerp(uint32_t a, uint32_t b, uint32_t value);
```

`inverseLerp` is the reverse operation — given a `value` between `a` and `b`, returns how far along that range it is as a `0..1` fixed-point fraction.

## Trig

`src/math/trig.hh`

```cpp
psyqo::Angle atan2_fixed(int16_t y, int16_t x);
psyqo::Angle LerpAngle(const psyqo::Angle &a, const psyqo::Angle &b, const psyqo::FixedPoint<10> &t);
```

`LerpAngle` interpolates around the shortest angular path between `a` and `b`, rather than doing a naive linear lerp that can wrap the long way round.

## Rand

`src/rand.hh`

A simple, **not** cryptographically secure PRNG (adapted from the pcsx-redux examples). You own your own instance — `MadnightEngine` exposes one at `g_madnightEngine.m_rand` for convenience.

```cpp
class Rand {
public:
  uint32_t rand();                       // 32-bit random number, never 0
  template <uint32_t RANGE> uint32_t rand();   // [0, RANGE)
  uint32_t rand(uint32_t max);           // [0, max)
  uint32_t rand(uint32_t min, uint32_t max); // [min, max)

  // seed == 0 breaks the generator — avoid it. Recommended: seed from
  // GPU::now() so you don't get the same sequence every run.
  void seed(uint32_t seed);
};
```
