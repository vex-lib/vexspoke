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

#include "vex.h"
#include <stdio.h>

#include "struct/sphere_array.h"
#include "oop/vexspoke_type.h"

static i32 g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Checks sphere-array constructors, initial state, and rejected dimensions.
static fn test_construction() {
    CHECK(SphereArray(-1, ID_INT) == nullptr);

    SphereArray *r0 = SphereArray(0, ID_INT);
    CHECK(r0 != nullptr);
    CHECK(SphereArray_radius(r0) == 0);
    CHECK(SphereArray_diameter(r0) == 1);
    CHECK(SphereArray_validVoxelCount(r0) == 1);
    SphereArray_free(r0);

    const i32 expected[] = { 1, 7, 33 };
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
static fn test_geometry() {
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
static fn test_access_round_trip() {
    SphereArray *a = SphereArray(2, ID_INT);
    CHECK(a != nullptr);

    i32 v = 4321;
    CHECK(SphereArray_setOffset(a, 0, 0, 0, &v));
    i32 got = 0;
    CHECK(SphereArray_getGrid(&got, a, 2, 2, 2));
    CHECK(got == 4321);

    CHECK(SphereArray_setOffset(a, 2, 0, 0, &(i32){ 11 }));
    CHECK(SphereArray_getOffset(&got, a, 2, 0, 0));
    CHECK(got == 11);
    CHECK(SphereArray_setGrid(a, 2, 1, 2, &(i32){ 22 }));
    CHECK(SphereArray_getOffset(&got, a, 0, -1, 0));
    CHECK(got == 22);

    CHECK(SphereArray_getOffset(&got, a, 0, 0, 0));
    CHECK(got == 4321);                                  // untouched

    // Refusals.
    CHECK(!SphereArray_setOffset(a, 2, 2, 0, &(i32){ 99 }));
    CHECK(!SphereArray_getOffset(&got, a, 2, 2, 0));
    CHECK(!SphereArray_setGrid(a, 9, 9, 9, &(i32){ 99 }));
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
    i64 checksum;
} WalkStats;

;;GETTER
static inline bytes WalkStats_badDistance(const WalkStats *s) { return (*s).badDistance; }
;;GETTER
static inline bytes *WalkStats_badDistanceMut(WalkStats *s) { return &(*s).badDistance; }
;;SETTER
static inline fn WalkStats_setBadDistance(WalkStats *s, bytes v) { (*s).badDistance = v; }
;;GETTER
static inline i64 WalkStats_checksum(const WalkStats *s) { return (*s).checksum; }
;;GETTER
static inline i64 *WalkStats_checksumMut(WalkStats *s) { return &(*s).checksum; }
;;SETTER
static inline fn WalkStats_setChecksum(WalkStats *s, i64 v) { (*s).checksum = v; }
;;GETTER
static inline bytes WalkStats_count(const WalkStats *s) { return (*s).count; }
;;GETTER
static inline bytes *WalkStats_countMut(WalkStats *s) { return &(*s).count; }
;;SETTER
static inline fn WalkStats_setCount(WalkStats *s, bytes v) { (*s).count = v; }
;;GETTER
static inline bytes WalkStats_mismatchedOffset(const WalkStats *s) { return (*s).mismatchedOffset; }
;;GETTER
static inline bytes *WalkStats_mismatchedOffsetMut(WalkStats *s) { return &(*s).mismatchedOffset; }
;;SETTER
static inline fn WalkStats_setMismatchedOffset(WalkStats *s, bytes v) { (*s).mismatchedOffset = v; }
;;GETTER
static inline bytes WalkStats_nullVoxel(const WalkStats *s) { return (*s).nullVoxel; }
;;GETTER
static inline bytes *WalkStats_nullVoxelMut(WalkStats *s) { return &(*s).nullVoxel; }
;;SETTER
static inline fn WalkStats_setNullVoxel(WalkStats *s, bytes v) { (*s).nullVoxel = v; }


// Accumulates traversal count, coordinate checks, and the visited payload checksum.
static fn walkVoxel(i32 gx, i32 gy, i32 gz,
                      i32 dx, i32 dy, i32 dz,
                      const_any voxel, any userData) {
    WalkStats *s = (WalkStats*) userData;
    WalkStats_setCount(s, WalkStats_count(s) + 1u);
    if (voxel == nullptr)
        WalkStats_setNullVoxel(s, WalkStats_nullVoxel(s) + 1u);
    i64 dist = (i64) dx * dx + (i64) dy * dy + (i64) dz * dz;
    if (dist > 4)                                        // radius 2
        WalkStats_setBadDistance(s, WalkStats_badDistance(s) + 1u);
    if (gx != dx + 2 || gy != dy + 2 || gz != dz + 2)
        WalkStats_setMismatchedOffset(s, WalkStats_mismatchedOffset(s) + 1u);
    if (voxel != nullptr) {
        i32 v = 0;
        for (bytes i = 0; i < sizeof(i32); i++)
            ((u8*) &v)[i] = ((const u8*) voxel)[i];
        WalkStats_setChecksum(s, WalkStats_checksum(s) + (v));
    }
}

// Confirms iteration visits the expected sphere-array voxels.
static fn test_for_each() {
    SphereArray *a = SphereArray(2, ID_INT);
    CHECK(a != nullptr);

    i32 fill = 1;
    for (i32 gz = 0; gz < SphereArray_diameter(a); gz++)
        for (i32 gy = 0; gy < SphereArray_diameter(a); gy++)
            for (i32 gx = 0; gx < SphereArray_diameter(a); gx++)
                if (SphereArray_containsGrid(a, gx, gy, gz)) {
                    i32 v = fill;
                    CHECK(SphereArray_setGrid(a, gx, gy, gz, &v));
                    fill++;
                }

    WalkStats s = { 0 };
    SphereArray_forEach(a, walkVoxel, &s);
    CHECK(s.count == 33);
    CHECK(s.badDistance == 0);
    CHECK(s.nullVoxel == 0);
    CHECK(s.mismatchedOffset == 0);
    CHECK(s.checksum == (i64) 33 * 34 / 2);          // sum 1..33

    SphereArray_forEach(a, nullptr, &s);
    SphereArray_forEach(nullptr, walkVoxel, &s);
    SphereArray_free(a);
}

// Runs sphere-array construction, geometry, access, iteration, and teardown checks.
i32 main() {
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
