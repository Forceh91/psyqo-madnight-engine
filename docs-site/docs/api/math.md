---
title: Math
sidebar_position: 7
---

# Math

`src/math/` and `src/rand.hh` have the small helpers the engine uses for vectors, matrices, interpolation, angles and random numbers. They are free functions, apart from `GTEMath` (static) and `Rand` (an instance you seed).

## Vector

`src/math/vector.hh`

`DotProduct` returns the dot product of two `psyqo::Vec3`. `IsVector3Zero` tells you whether a vector is exactly zero, which is worth checking before you normalise anything. `Lerp` blends two `Vec2` or two `Vec3`.

```cpp
if (!IsVector3Zero(velocity)) {
  auto along = DotProduct(velocity, forward);
}
auto mid = Lerp(startPos, endPos, 0.5_fp); // halfway between two points
```

`t` is a `FixedPoint<>` where `0` gives `a` and `1.0_fp` gives `b`. It is not clamped, so a value outside that range extrapolates.

## Lerp

`src/math/lerp.hh`

`Lerp` blends two `FixedPoint<>` numbers the same way. `inverseLerp` goes the other way: given a value between `a` and `b`, it returns how far along that range it is, as a fraction from `0` to `1.0_fp`.

```cpp
// how far through a 120..240 frame window are we?
auto progress = inverseLerp(120, 240, frame);
auto brightness = Lerp(0.2_fp, 1.0_fp, progress);
```

`inverseLerp` takes plain integers and does not clamp. A value outside `a..b` gives a fraction outside `0..1`, and `a == b` divides by zero.

## Trig

`src/math/trig.hh`

`LerpAngle` blends two `psyqo::Angle` values the short way round. Going from `0.9_pi` to `-0.9_pi` passes through `±1.0_pi` instead of swinging back through `0`. Use it for turning an object towards a target.

```cpp
psyqo::FixedPoint<10> t = 0.1;
auto yaw = LerpAngle(object->rotation().y, targetYaw, t);
```

`t` is a `FixedPoint<10>` (10 fractional bits), not the usual `FixedPoint<>`. The result is not re-wrapped, so it can land just outside `±1.0_pi`.

:::caution[atan2_fixed is deprecated]
`atan2_fixed` will be removed in the next major release; use `psyqo::atan2` instead. The deprecation notice says it is not reliable: it drifts from the expected values (by about 3.5 degrees) or takes the long path instead of the short one.
:::

## Matrix

`src/math/matrix.hh`

`TransposeMatrix33` and `InverseMatrix33` both return a new `psyqo::Matrix33`. For a rotation matrix the transpose is the inverse, so use `TransposeMatrix33`. It is exact and cheap.

```cpp
auto toLocal = TransposeMatrix33(object->rotationMatrix()); // undo the object's rotation
```

`InverseMatrix33` is for a matrix that is not a pure rotation. It works out the determinant and divides, which costs more. If the determinant is close to zero it gives you the input matrix back unchanged, so the result is not an inverse in that case.

## GTEMath

`src/math/gte-math.hh`

`GTEMath::ProjectVectorOntoAxes(axisMatrix, normalizedVec)` projects a vector onto the three axes of a matrix, using the GTE. The collision code calls it for its overlap tests. You are unlikely to need it directly. For matrix and vector multiplication, call `psyqo::GteMath` yourself.

## Rand

`src/rand.hh`, reached as `g_madnightEngine.m_rand`.

A small random number generator. It is not secure, and `seed(0)` breaks it. Seed it once at start-up, or you get the same sequence every run.

```cpp
g_madnightEngine.m_rand.seed(Renderer::Instance().GPU().now());

auto roll = g_madnightEngine.m_rand.rand(1, 7); // 1 to 6
auto pick = g_madnightEngine.m_rand.rand<4>();  // 0 to 3
```

`rand(max)` and `rand<RANGE>()` give `0` up to but not including the limit. `rand(min, max)` also leaves out `max`, and needs `max` greater than `min`. `rand()` with no arguments gives a full 32-bit number and never returns 0.
