#ifndef MATH_FAST_MATH_H
#define MATH_FAST_MATH_H

#include <stdbool.h>
#include <stdint.h>

// math/fast_math.h — Relaxed Fast Math Engine (Violates IEEE 754 for Speed).
//
// Single Class Per File Law: FastMath.
//
// High-performance polynomial and bitwise approximations tailored for real-time
// 3D graphics, particle systems, vertex shaders, and game logic. Intentionally
// relaxes IEEE 754 bit-exactness to eliminate division, square root, and
// trigonometric latency.

#define FAST_MATH_PI         3.1415927f
#define FAST_MATH_HALF_PI    1.5707964f
#define FAST_MATH_TWO_PI     6.2831855f
#define FAST_MATH_EPSILON    0.0001f
#define FAST_MATH_DEG_TO_RAD 0.0174532925f
#define FAST_MATH_RAD_TO_DEG 57.2957795f

// Quake III 0x5f3759df fast inverse square root (1 Newton-Raphson pass).
f32 FastMath_invSqrt(f32 x);

// Fast reciprocal approximation.
f32 FastMath_inv(f32 x);

// Bhaskara I rational polynomial sine (radians, wraps any periodic domain).
f32 FastMath_sin(f32 x);

// Bhaskara I rational polynomial cosine.
f32 FastMath_cos(f32 x);

// Fast tangent approximation.
f32 FastMath_tan(f32 x);

// Fast polynomial arc tangent.
f32 FastMath_atan(f32 x);
f32 FastMath_atan2(f32 y, f32 x);

// Branchless absolute value (IEEE sign-bit clear).
f32 FastMath_abs(f32 x);

// Magic-f32 fast rounding (2^23 trick).
f32 FastMath_round(f32 x);

// Branchless clamp.
f32 FastMath_clamp(f32 val, f32 min, f32 max);

// Linear interpolation.
f32 FastMath_lerp(f32 a, f32 b, f32 t);

// Tolerance-based approximate equality.
bool FastMath_approxEqual(f32 a, f32 b, f32 epsilon);

f32 FastMath_toRadians(f32 deg);
f32 FastMath_toDegrees(f32 rad);

#endif
