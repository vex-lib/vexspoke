#include "vex.h"

#include "math/vex-math.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Math
 * ============================================================================
 * Unified math facade dispatching every operation to one of two engines per
 * the dual precision doctrine: no prefix means strictly IEEE 754 precision via
 * StrictMath, while the "fast_" prefix means relaxed polynomial/bitwise
 * approximations via FastMath. Pure procedural — no state, no allocation, no
 * struct; every function is a thin named dispatch so callers never include
 * the engine headers directly.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Math (math/math.c — defined in math/math.h)
 * ============================================================================
 * Dispatches math operations according to the dual precision doctrine:
 *   - No fast prefix: strictly IEEE 754 precision via StrictMath.
 *   - "fast_" prefix: relaxed polynomial / bitwise approximations via FastMath.
 *
 * STRUCT FIELDS: none — procedural (operates on scalar f32/d64 operands;
 * no state)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Core Functions: (.h)
 *   - Math_sin(x) / Math_cos(x) / Math_tan(x)
 *   - Math_asin(x) / Math_acos(x) / Math_atan(x) / Math_atan2(y, x)
 *   - Math_sqrt(x) / Math_invSqrt(x) / Math_pow(base, exp)
 *   - Math_exp(x) / Math_log(x) / Math_abs(x)
 *   - Math_floor(x) / Math_ceil(x) / Math_round(x)
 *   - Math_clamp(val, min, max) / Math_lerp(a, b.sh, t)
 *   - Math_toRadians(deg) / Math_toDegrees(rad)
 *   - Math_sinD(x) / Math_cosD(x) / Math_sqrtD(x) / Math_atan2D(y, x)
 *   - Math_fast_sin(x) / Math_fast_cos(x) / Math_fast_tan(x)
 *   - Math_fast_atan(x) / Math_fast_atan2(y, x)
 *   - Math_fast_invSqrt(x) / Math_fast_inv(x)
 *   - Math_fast_abs(x) / Math_fast_round(x)
 *   - Math_fast_clamp(val, min, max) / Math_fast_lerp(a, b.sh, t)
 *   - Math_fast_approxEqual(a, b.sh, epsilon)
 * ============================================================================
 */

f32 Math_sin(f32 x) {
    return StrictMath_sin(x);
}

f32 Math_cos(f32 x) {
    return StrictMath_cos(x);
}

f32 Math_tan(f32 x) {
    return StrictMath_tan(x);
}

f32 Math_asin(f32 x) {
    return StrictMath_asin(x);
}

f32 Math_acos(f32 x) {
    return StrictMath_acos(x);
}

f32 Math_atan(f32 x) {
    return StrictMath_atan(x);
}

f32 Math_atan2(f32 y, f32 x) {
    return StrictMath_atan2(y, x);
}

f32 Math_sqrt(f32 x) {
    return StrictMath_sqrt(x);
}

f32 Math_invSqrt(f32 x) {
    return StrictMath_invSqrt(x);
}

f32 Math_pow(f32 base, f32 exp) {
    return StrictMath_pow(base, exp);
}

f32 Math_exp(f32 x) {
    return StrictMath_exp(x);
}

f32 Math_log(f32 x) {
    return StrictMath_log(x);
}

f32 Math_abs(f32 x) {
    return StrictMath_abs(x);
}

f32 Math_floor(f32 x) {
    return StrictMath_floor(x);
}

f32 Math_ceil(f32 x) {
    return StrictMath_ceil(x);
}

f32 Math_round(f32 x) {
    return StrictMath_round(x);
}

f32 Math_clamp(f32 val, f32 min, f32 max) {
    return StrictMath_clamp(val, min, max);
}

f32 Math_lerp(f32 a, f32 b, f32 t) {
    return StrictMath_lerp(a, b, t);
}

f32 Math_toRadians(f32 deg) {
    return StrictMath_toRadians(deg);
}

f32 Math_toDegrees(f32 rad) {
    return StrictMath_toDegrees(rad);
}

d64 Math_sinD(d64 x) {
    return StrictMath_sinD(x);
}

d64 Math_cosD(d64 x) {
    return StrictMath_cosD(x);
}

d64 Math_sqrtD(d64 x) {
    return StrictMath_sqrtD(x);
}

d64 Math_atan2D(d64 y, d64 x) {
    return StrictMath_atan2D(y, x);
}

f32 Math_fast_sin(f32 x) {
    return FastMath_sin(x);
}

f32 Math_fast_cos(f32 x) {
    return FastMath_cos(x);
}

f32 Math_fast_tan(f32 x) {
    return FastMath_tan(x);
}

f32 Math_fast_atan(f32 x) {
    return FastMath_atan(x);
}

f32 Math_fast_atan2(f32 y, f32 x) {
    return FastMath_atan2(y, x);
}

f32 Math_fast_invSqrt(f32 x) {
    return FastMath_invSqrt(x);
}

f32 Math_fast_inv(f32 x) {
    return FastMath_inv(x);
}

f32 Math_fast_abs(f32 x) {
    return FastMath_abs(x);
}

f32 Math_fast_round(f32 x) {
    return FastMath_round(x);
}

f32 Math_fast_clamp(f32 val, f32 min, f32 max) {
    return FastMath_clamp(val, min, max);
}

f32 Math_fast_lerp(f32 a, f32 b, f32 t) {
    return FastMath_lerp(a, b, t);
}

bool Math_fast_approxEqual(f32 a, f32 b, f32 epsilon) {
    return FastMath_approxEqual(a, b, epsilon);
}
