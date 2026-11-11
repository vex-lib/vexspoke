// vexspoke/test/math/coord_frame_test.c — owner test for math/coord_frame.
//
// CoordFrame is a pure lookup table over the 12 primary spatial frames. This
// test pins the axis permutation and sign for every frame against the
// documented table, checks the default fallback for an out-of-range frame,
// the per-axis out-of-range guards, validity, and human-readable names.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "math/coord_frame.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// axis[3] then sign[3] for each of the 12 frames, straight from the source
// table (the oracle that catches a silent permutation/sign drift).
static const  k_axis[12][3] = {
    {0,1,2},{0,1,2},{2,0,1},{0,2,1},{1,2,0},{1,0,2},
    {0,1,2},{0,1,2},{2,0,1},{0,2,1},{1,2,0},{1,0,2},
};
static const  k_sign[12][3] = {
    { 1, 1, 1},{ 1, 1,-1},{ 1, 1, 1},{ 1, 1, 1},{ 1, 1, 1},{ 1, 1, 1},
    { 1,-1, 1},{ 1,-1,-1},{ 1, 1,-1},{ 1, 1,-1},{-1, 1, 1},{-1, 1, 1},
};
static const char *k_names[12] = {
    "Y_UP_LEFT","Y_UP_RIGHT","Z_UP_LEFT","Z_UP_RIGHT","X_UP_LEFT","X_UP_RIGHT",
    "Y_DOWN_LEFT","Y_DOWN_RIGHT","Z_DOWN_LEFT","Z_DOWN_RIGHT","X_DOWN_LEFT","X_DOWN_RIGHT",
};

// Checks each coordinate frame's axis permutation and sign against the oracle tables.
// Checks basis-vector mappings for the supported coordinate-frame conventions.
// Compares each supported frame's axis permutation and signs against the oracle table.
static void test_basis(void) {
    for (int f = 0; f < 12; f++) {
        CoordBasis b = CoordFrame_getBasis((CoordFrame) f);
        for (int a = 0; a < 3; a++) {
            CHECK(b.axis[a] == k_axis[f][a]);
            CHECK(b.sign[a] == k_sign[f][a]);
            CHECK(CoordFrame_getAxisIndex((CoordFrame) f, a) == k_axis[f][a]);
            CHECK(CoordFrame_getAxisSign((CoordFrame) f, a) == (f32) k_sign[f][a]);
        }
    }
}

// Verifies the canonical frame and its identity basis.
// Verifies the default frame and its canonical basis mapping.
// Checks the default frame constant and its identity basis.
static void test_default_frame(void) {
    CHECK(COORD_FRAME_DEFAULT == COORD_FRAME_Y_UP_LEFT);
    CoordBasis def = CoordFrame_getBasis(COORD_FRAME_DEFAULT);
    CHECK(def.axis[0] == 0 && def.axis[1] == 1 && def.axis[2] == 2);
    CHECK(def.sign[0] == 1 && def.sign[1] == 1 && def.sign[2] == 1);
}

// Confirms invalid frames and axes resolve to the documented safe defaults.
// Confirms invalid frame or axis indices return the documented safe result.
// Verifies invalid frames and axis indices return documented fallback values.
static void test_out_of_range(void) {
    // An invalid frame resolves to the default basis.
    CoordBasis b = CoordFrame_getBasis((CoordFrame) 12);
    CHECK(b.axis[0] == 0 && b.axis[1] == 1 && b.axis[2] == 2 && b.sign[2] == 1);
    b = CoordFrame_getBasis((CoordFrame) 999);
    CHECK(b.axis[0] == 0);

    // Per-axis guards: bad frame or bad axis index.
    CHECK(CoordFrame_getAxisIndex((CoordFrame) 12, 1) == 0);
    CHECK(CoordFrame_getAxisSign((CoordFrame) 12, 1) == 1.0f);
    CHECK(CoordFrame_getAxisIndex(COORD_FRAME_Y_UP_LEFT, 3) == 0);
    CHECK(CoordFrame_getAxisSign(COORD_FRAME_Y_UP_LEFT, 3) == 1.0f);
    CHECK(CoordFrame_getAxisIndex(COORD_FRAME_Y_UP_LEFT, -1) == 0);
    CHECK(CoordFrame_getAxisSign(COORD_FRAME_Y_UP_LEFT, -1) == 1.0f);
}

// Checks the accepted frame range and stable names, including unknown values.
// Checks frame validity predicates and stable frame-name projections.
// Checks frame validity classification and stable human-readable names.
static void test_validity_and_names(void) {
    for (u32 f = 0; f < 12; f++)
        CHECK(CoordFrame_isValid(f));
    CHECK(!CoordFrame_isValid(12));
    CHECK(!CoordFrame_isValid(100));
    CHECK(!CoordFrame_isValid(UINT32_MAX));

    for (int f = 0; f < 12; f++)
        CHECK(strcmp(CoordFrame_name((CoordFrame) f), k_names[f]) == 0);
    CHECK(strcmp(CoordFrame_name((CoordFrame) 12), "UNKNOWN") == 0);
    CHECK(strcmp(CoordFrame_name((CoordFrame) -1), "UNKNOWN") == 0);
}

// Runs all coordinate-frame lookup, boundary, validity, and naming checks.
// Runs coordinate-frame basis, default, invalid-index, validity, and naming checks.
// Runs coordinate basis, fallback, validity, and name assertions.
int main(void) {
    test_basis();
    test_default_frame();
    test_out_of_range();
    test_validity_and_names();

    if (g_failures == 0) {
        printf("coord_frame_test: all assertions held\n");
        return 0;
    }
    printf("coord_frame_test: %d FAILURES\n", g_failures);
    return 1;
}
