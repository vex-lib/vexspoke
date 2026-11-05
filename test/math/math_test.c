// vexspoke/test/math/math_test.c — owner test for math/math (the facade).
//
// Math is a thin dispatch facade over StrictMath (no prefix) and FastMath
// ("fast_" prefix). This test proves the dispatch is faithful — every public
// facade function returns exactly what its engine returns — and that the
// facade constants alias the strict constants. That is the Public Surface
// Proof Law for a facade: no function silently routes to the wrong engine.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "math/vex-math.h"
#include "math/strict_math.h"
#include "math/fast_math.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

#define EQ(a, b) ((a) == (b) || (isnan(a) && isnan(b)))

// Asserts that facade constants exactly alias the strict-math constants.
// Checks the math constants against their defined values and special cases.
// Confirms facade constants are aliases of the strict-math constants.
static void test_constants(void) {
    CHECK(MATH_PI == STRICT_MATH_PI);
    CHECK(MATH_HALF_PI == STRICT_MATH_HALF_PI);
    CHECK(MATH_TWO_PI == STRICT_MATH_TWO_PI);
    CHECK(MATH_E == STRICT_MATH_E);
    CHECK(MATH_TAU == STRICT_MATH_TAU);
    CHECK(MATH_DEG_TO_RAD == STRICT_MATH_DEG_TO_RAD);
    CHECK(MATH_RAD_TO_DEG == STRICT_MATH_RAD_TO_DEG);
}

// Compares every strict facade operation with its StrictMath implementation.
// Verifies generic math operations dispatch to strict-math implementations.
// Compares every strict facade call with its StrictMath implementation.
static void test_strict_dispatch(void) {
    float xs[] = { -3.0f, -0.5f, 0.0f, 0.25f, 1.0f, 3.3f };
    for (size_t i = 0; i < sizeof(xs) / sizeof(xs[0]); i++) {
        float x = xs[i];
        CHECK(EQ(Math_sin(x), StrictMath_sin(x)));
        CHECK(EQ(Math_cos(x), StrictMath_cos(x)));
        CHECK(EQ(Math_tan(x), StrictMath_tan(x)));
        CHECK(EQ(Math_atan(x), StrictMath_atan(x)));
        CHECK(EQ(Math_sqrt(x), StrictMath_sqrt(x)));
        CHECK(EQ(Math_invSqrt(x), StrictMath_invSqrt(x)));
        CHECK(EQ(Math_exp(x), StrictMath_exp(x)));
        CHECK(EQ(Math_floor(x), StrictMath_floor(x)));
        CHECK(EQ(Math_ceil(x), StrictMath_ceil(x)));
        CHECK(EQ(Math_round(x), StrictMath_round(x)));
        CHECK(EQ(Math_abs(x), StrictMath_abs(x)));
        CHECK(EQ(Math_toRadians(x), StrictMath_toRadians(x)));
        CHECK(EQ(Math_toDegrees(x), StrictMath_toDegrees(x)));
        CHECK(EQ(Math_clamp(x, -1.0f, 1.0f), StrictMath_clamp(x, -1.0f, 1.0f)));
        CHECK(EQ(Math_lerp(0.0f, 4.0f, x), StrictMath_lerp(0.0f, 4.0f, x)));
    }
    CHECK(Math_asin(0.5f) == StrictMath_asin(0.5f));
    CHECK(Math_acos(0.5f) == StrictMath_acos(0.5f));
    CHECK(Math_pow(2.0f, 5.0f) == StrictMath_pow(2.0f, 5.0f));
    CHECK(Math_log(2.0f) == StrictMath_log(2.0f));
    CHECK(Math_atan2(1.0f, 3.0f) == StrictMath_atan2(1.0f, 3.0f));

    CHECK(Math_sinD(1.25) == StrictMath_sinD(1.25));
    CHECK(Math_cosD(1.25) == StrictMath_cosD(1.25));
    CHECK(Math_sqrtD(2.25) == StrictMath_sqrtD(2.25));
    CHECK(Math_atan2D(1.0, 2.0) == StrictMath_atan2D(1.0, 2.0));
}

// Compares every fast facade operation with its FastMath implementation.
// Verifies generic math operations dispatch to fast-math implementations.
// Compares each fast facade call with its FastMath implementation.
static void test_fast_dispatch(void) {
    float xs[] = { -2.5f, -0.5f, 0.0f, 0.5f, 2.5f };
    for (size_t i = 0; i < sizeof(xs) / sizeof(xs[0]); i++) {
        float x = xs[i];
        CHECK(Math_fast_sin(x) == FastMath_sin(x));
        CHECK(Math_fast_cos(x) == FastMath_cos(x));
        CHECK(Math_fast_tan(x) == FastMath_tan(x));
        CHECK(Math_fast_atan(x) == FastMath_atan(x));
        CHECK(Math_fast_invSqrt(x) == FastMath_invSqrt(x));
        CHECK(Math_fast_inv(x) == FastMath_inv(x));
        CHECK(Math_fast_abs(x) == FastMath_abs(x));
        CHECK(Math_fast_round(x) == FastMath_round(x));
        CHECK(Math_fast_clamp(x, -1.0f, 1.0f) == FastMath_clamp(x, -1.0f, 1.0f));
        CHECK(Math_fast_lerp(0.0f, 1.0f, x) == FastMath_lerp(0.0f, 1.0f, x));
    }
    CHECK(Math_fast_atan2(1.0f, -2.0f) == FastMath_atan2(1.0f, -2.0f));
    CHECK(Math_fast_approxEqual(1.0f, 1.001f, 0.01f) == FastMath_approxEqual(1.0f, 1.001f, 0.01f));
}

// Runs constant and strict/fast dispatch parity checks.
// Runs math constant and strict/fast generic-dispatch checks.
// Runs constant-alias and strict/fast facade dispatch checks.
int main(void) {
    test_constants();
    test_strict_dispatch();
    test_fast_dispatch();

    if (g_failures == 0) {
        printf("math_test: all assertions held\n");
        return 0;
    }
    printf("math_test: %d FAILURES\n", g_failures);
    return 1;
}
