#include "vex.h"
#include <math.h>

#include "math/coord_frame.h"
#include "math/strict_math.h"
#include "math/fast_math.h"
#include "math/vex-math.h"

static int g_failures = 0;

#define CHECK(name, cond) do { \
    if (cond) { printf("[math_coord_test] PASS %s\n", name); } \
    else { printf("[math_coord_test] FAIL %s\n", name); g_failures++; } \
} while (0)

// Covers the 12 coordinate frames and the strict/fast math facade. The numeric
// vector and matrix shapes belong to vex-c (vec2f/vec3f/vec4f/mat4f); the frame
// table stays here so an agnostic coordinate consumer can map spatial axes.
int main() {
    printf("=== Running 12-Frame Math & Vector Test Suite ===\n");

    // 1. The 12 Coordinate Frames
    CHECK("default frame is Y_UP_LEFT", COORD_FRAME_DEFAULT == COORD_FRAME_Y_UP_LEFT);
    CHECK("frame 0 is valid", CoordFrame_isValid(COORD_FRAME_Y_UP_LEFT));
    CHECK("frame 11 is valid", CoordFrame_isValid(COORD_FRAME_X_DOWN_RIGHT));
    CHECK("frame 12 is invalid", !CoordFrame_isValid(12));

    CoordBasis b0 = CoordFrame_getBasis(COORD_FRAME_Y_UP_LEFT);
    CHECK("Y_UP_LEFT basis X axis is horizontal", b0.axis[0] == 0 && b0.sign[0] == 1);
    CHECK("Y_UP_LEFT basis Y axis is vertical", b0.axis[1] == 1 && b0.sign[1] == 1);
    CHECK("Y_UP_LEFT basis Z axis is depth", b0.axis[2] == 2 && b0.sign[2] == 1);

    CoordBasis b1 = CoordFrame_getBasis(COORD_FRAME_Y_UP_RIGHT);
    CHECK("Y_UP_RIGHT Z sign is negative (Right-handed)", b1.sign[2] == -1);

    // 2. Strict Math vs Fast Math
    f32 s_sin = StrictMath_sin((f32) STRICT_MATH_HALF_PI);
    CHECK("strict sin(pi/2) is 1.0", fabsf(s_sin - 1.0f) < 1e-6f);

    f32 f_sin = FastMath_sin(FAST_MATH_HALF_PI);
    CHECK("fast sin(pi/2) is ~1.0", fabsf(f_sin - 1.0f) < 0.01f);

    f32 inv_strict = StrictMath_invSqrt(16.0f);
    f32 inv_fast = FastMath_invSqrt(16.0f);
    CHECK("strict invSqrt(16) is 0.25", fabsf(inv_strict - 0.25f) < 1e-6f);
    CHECK("fast invSqrt(16) is ~0.25", fabsf(inv_fast - 0.25f) < 0.01f);

    // 3. Unified Math Facade
    CHECK("Math_sin is strict by default", fabsf(Math_sin((f32)MATH_HALF_PI) - 1.0f) < 1e-6f);
    CHECK("Math_fast_sin uses approximation", fabsf(Math_fast_sin(FAST_MATH_HALF_PI) - 1.0f) < 0.01f);
    CHECK("Math_fast_invSqrt is fast approx", fabsf(Math_fast_invSqrt(4.0f) - 0.5f) < 0.01f);

    printf("\n=== 12-Frame Math Test Summary: %d failures ===\n", g_failures);
    return g_failures > 0 ? 1 : 0;
}
