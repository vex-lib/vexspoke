// vexspoke/test/struct/sphere_array_test.c — owner test for struct/sphere_array.
//
// Proves the 3D Pythagorean ball volume:
//   - construction: negative radius refused; the analytic voxel counts for
//     r = 0..2 (1, 7, 33) are the oracle for the precompute and forEach;
//   - diameter is 2*radius+1; the bounding CUBE is diameter^3 cells while only
//     the ball is valid;
//   - grid vs. offset systems agree; 3D distance math on center/axis/corner;
//   - get/set round trips over a stride-4 payload, refusals outside the ball
//     leave neighbours intact, nullptr src/dest refused;
//   - slot accessors return nullptr below/above the ball;
//   - forEach visits exactly the valid voxels with correct dx/dy/dz;
//   - every entry point tolerates nullptr.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "struct/sphere_array.h"
#include "oop/type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Checks sphere-array constructors, initial state, and rejected dimensions.
static void test_construction(void) {
    CHECK(SphereArray(-1, ID_INT) == nullptr);

    SphereArray *r0 = SphereArray(0, ID_INT);
    CHECK(r0 != nullptr);
    CHECK(SphereArray_radius(r0) == 0);
    CHECK(SphereArray_diameter(r0) == 1);
    CHECK(SphereArray_validVoxelCount(r0) == 1);
    SphereArray_free(r0);

    const int expected[] = { 1, 7, 33 };
    for (i32 r = 0; r <= 2; r++) {
        SphereArray *a = SphereArray(r, ID_INT);
        CHECK(a != nullptr);
        CHECK(SphereArray_radius(a) == r);
        CHECK(SphereArray_diameter(a) == 2 * r + 1);
        CHECK(SphereArray_validVoxelCount(a) == (bytes) expected[r]);
        SphereArray_free(a);
    }

    SphereArray *s = SphereArray(1, ID_INT, 8);
    CHECK(s != nullptr);
    CHECK(SphereArray_validVoxelCount(s) == 7);
    SphereArray_free(s);

    SphereArray_free(nullptr);
}

// Verifies sphere dimensions and computed voxel-storage geometry.
static void test_geometry(void) {
    SphereArray *a = SphereArray(2, ID_INT);
    CHECK(a != nullptr);

    CHECK(SphereArray_distanceSquaredOffset(0, 0, 0) == 0);
    CHECK(SphereArray_distanceSquaredOffset(1, 2, 2) == 9);
    CHECK(SphereArray_distanceSquaredOffset(-1, -2, -2) == 9);
    CHECK(SphereArray_distanceSquaredGrid(a, 2, 2, 2) == 0);    // center
    CHECK(SphereArray_distanceSquaredGrid(a, 4, 2, 2) == 4);    // dx=2

    CHECK(SphereArray_containsOffset(a, 0, 0, 0));
    CHECK(SphereArray_containsOffset(a, 2, 0, 0));
    CHECK(SphereArray_containsOffset(a, 1, 1, 1));              // 3 <= 4
    CHECK(!SphereArray_containsOffset(a, 2, 1, 0));             // 5 > 4
    CHECK(!SphereArray_containsOffset(a, 2, 2, 0));             // 8 > 4
    CHECK(!SphereArray_containsOffset(a, 3, 0, 0));             // beyond radius
    CHECK(!SphereArray_containsOffset(a, -3, 0, 0));

    CHECK(SphereArray_containsGrid(a, 2, 2, 2));
    CHECK(SphereArray_containsGrid(a, 4, 2, 2));                // (2,0,0)
    CHECK(!SphereArray_containsGrid(a, 0, 0, 0));               // (-2,-2,-2)
    CHECK(!SphereArray_containsGrid(a, 5, 2, 2));               // oob
    CHECK(!SphereArray_containsGrid(a, 2, -1, 2));

    SphereArray_free(a);
}

// Checks voxel writes and reads at valid coordinates and boundary locations.
static void test_access_round_trip(void) {
    SphereArray *a = SphereArray(2, ID_INT);
    CHECK(a != nullptr);

    int v = 4321;
    CHECK(SphereArray_setOffset(a, 0, 0, 0, &v));
    int got = 0;
    CHECK(SphereArray_getGrid(&got, a, 2, 2, 2));
    CHECK(got == 4321);

    CHECK(SphereArray_setOffset(a, 2, 0, 0, &(int){ 11 }));
    CHECK(SphereArray_getOffset(&got, a, 2, 0, 0));
    CHECK(got == 11);
    CHECK(SphereArray_setGrid(a, 2, 1, 2, &(int){ 22 }));
    CHECK(SphereArray_getOffset(&got, a, 0, -1, 0));
    CHECK(got == 22);

    CHECK(SphereArray_getOffset(&got, a, 0, 0, 0));
    CHECK(got == 4321);                                  // untouched

    // Refusals.
    CHECK(!SphereArray_setOffset(a, 2, 2, 0, &(int){ 99 }));
    CHECK(!SphereArray_getOffset(&got, a, 2, 2, 0));
    CHECK(!SphereArray_setGrid(a, 9, 9, 9, &(int){ 99 }));
    CHECK(!SphereArray_getGrid(&got, a, 9, 9, 9));
    CHECK(!SphereArray_setOffset(a, 0, 0, 0, nullptr));
    CHECK(!SphereArray_getOffset(nullptr, a, 0, 0, 0));
    CHECK(SphereArray_getOffset(&got, a, 0, 0, 0));
    CHECK(got == 4321);

    CHECK(SphereArray_slotOffset(a, 0, 0, 0) != nullptr);
    CHECK(SphereArray_slotGrid(a, 2, 2, 2) != nullptr);
    CHECK(SphereArray_slotOffset(a, 2, 2, 0) == nullptr);
    CHECK(SphereArray_slotGrid(a, 9, 9, 9) == nullptr);
    CHECK(SphereArray_slotOffset(nullptr, 0, 0, 0) == nullptr);
    CHECK(SphereArray_slotGrid(nullptr, 0, 0, 0) == nullptr);

    SphereArray_free(a);
}

typedef struct WalkStats {
    bytes count;
    bytes badDistance;
    bytes nullVoxel;
    bytes mismatchedOffset;
    int64_t checksum;
} WalkStats;

// Accumulates traversal count, coordinate checks, and the visited payload checksum.
static void walkVoxel(i32 gx, i32 gy, i32 gz,
                      i32 dx, i32 dy, i32 dz,
                      const void *voxel, void *userData) {
    WalkStats *s = (WalkStats*) userData;
    (*s).count++;
    if (voxel == nullptr)
        (*s).nullVoxel++;
    int64_t dist = (int64_t) dx * dx + (int64_t) dy * dy + (int64_t) dz * dz;
    if (dist > 4)                                        // radius 2
        (*s).badDistance++;
    if (gx != dx + 2 || gy != dy + 2 || gz != dz + 2)
        (*s).mismatchedOffset++;
    if (voxel != nullptr) {
        int v = 0;
        for (bytes i = 0; i < sizeof(int); i++)
            ((u8*) &v)[i] = ((const u8*) voxel)[i];
        (*s).checksum += v;
    }
}

// Confirms iteration visits the expected sphere-array voxels.
static void test_for_each(void) {
    SphereArray *a = SphereArray(2, ID_INT);
    CHECK(a != nullptr);

    int fill = 1;
    for (i32 gz = 0; gz < SphereArray_diameter(a); gz++)
        for (i32 gy = 0; gy < SphereArray_diameter(a); gy++)
            for (i32 gx = 0; gx < SphereArray_diameter(a); gx++)
                if (SphereArray_containsGrid(a, gx, gy, gz)) {
                    int v = fill;
                    CHECK(SphereArray_setGrid(a, gx, gy, gz, &v));
                    fill++;
                }

    WalkStats s = { 0 };
    SphereArray_forEach(a, walkVoxel, &s);
    CHECK(s.count == 33);
    CHECK(s.badDistance == 0);
    CHECK(s.nullVoxel == 0);
    CHECK(s.mismatchedOffset == 0);
    CHECK(s.checksum == (int64_t) 33 * 34 / 2);          // sum 1..33

    SphereArray_forEach(a, nullptr, &s);
    SphereArray_forEach(nullptr, walkVoxel, &s);
    SphereArray_free(a);
}

// Runs sphere-array construction, geometry, access, iteration, and teardown checks.
int main(void) {
    test_construction();
    test_geometry();
    test_access_round_trip();
    test_for_each();

    if (g_failures == 0) {
        printf("sphere_array_test: all assertions held\n");
        return 0;
    }
    printf("sphere_array_test: %d FAILURES\n", g_failures);
    return 1;
}
