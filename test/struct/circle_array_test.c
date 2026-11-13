// vexspoke/test/struct/circle_array_test.c — owner test for struct/circle_array.
//
// Proves the 2D Pythagorean disk matrix:
//   - construction: negative radius refused; radius 0 is the one-cell disk;
//     diameter is 2*radius+1; validCellCount is the exact lattice count;
//   - the analytic valid-cell counts for r = 0..4 (1, 5, 13, 29, 49) are the
//     oracle for both the constructor's precompute and forEach's walk;
//   - grid vs. offset coordinate systems agree (grid = offset + radius);
//   - containment/distance math at the center, the axis edge, the diagonal
//     corner, and just-outside boundaries;
//   - get/set round trips over a stride-4 payload, and out-of-disk /
//     out-of-bounds / nullptr destinations are refused without corruption;
//   - slot accessors return nullptr off the disk;
//   - forEach visits exactly the valid cells, once each, with correct dx/dy;
//   - every entry point tolerates nullptr.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "struct/circle_array.h"
#include "oop/vexspoke_type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Checks circle-array constructors, initial state, and invalid construction inputs.
static void test_construction(void) {
    CHECK(CircleArray(-1, ID_INT) == nullptr);   // negative radius refused

    CircleArray *r0 = CircleArray(0, ID_INT);
    CHECK(r0 != nullptr);
    CHECK(CircleArray_radius(r0) == 0);
    CHECK(CircleArray_diameter(r0) == 1);
    CHECK(CircleArray_validCellCount(r0) == 1);         // the center cell
    CircleArray_free(r0);

    const int expected[] = { 1, 5, 13, 29, 49 };
    for (i32 r = 0; r <= 4; r++) {
        CircleArray *a = CircleArray(r, ID_INT);
        CHECK(a != nullptr);
        CHECK(CircleArray_radius(a) == r);
        CHECK(CircleArray_diameter(a) == 2 * r + 1);
        CHECK(CircleArray_validCellCount(a) == (bytes) expected[r]);
        CircleArray_free(a);
    }

    // Explicit stride construction is honored.
    CircleArray *s = CircleArray(2, ID_INT, 8);
    CHECK(s != nullptr);
    CHECK(CircleArray_validCellCount(s) == 13);
    CircleArray_free(s);

    CircleArray_free(nullptr);                          // safe
}

// Verifies circle dimensions and computed storage geometry.
static void test_geometry(void) {
    CircleArray *a = CircleArray(3, ID_INT);
    CHECK(a != nullptr);

    // Distance squared around the center.
    CHECK(CircleArray_distanceSquaredOffset(0, 0) == 0);
    CHECK(CircleArray_distanceSquaredOffset(3, 4) == 25);
    CHECK(CircleArray_distanceSquaredOffset(-3, -4) == 25);
    CHECK(CircleArray_distanceSquaredGrid(a, 3, 3) == 0);       // center
    CHECK(CircleArray_distanceSquaredGrid(a, 0, 3) == 9);       // axis edge

    // Containment: center, axis edge in, diagonal corner out.
    CHECK(CircleArray_containsOffset(a, 0, 0));
    CHECK(CircleArray_containsOffset(a, 3, 0));
    CHECK(CircleArray_containsOffset(a, 0, 3));
    CHECK(!CircleArray_containsOffset(a, 3, 3));        // 18 > 9
    CHECK(!CircleArray_containsOffset(a, 4, 0));        // beyond radius
    CHECK(!CircleArray_containsOffset(a, -4, 0));
    CHECK(CircleArray_containsGrid(a, 3, 3));           // center cell
    CHECK(CircleArray_containsGrid(a, 6, 3));           // (dx=3,dy=0)
    CHECK(!CircleArray_containsGrid(a, 0, 0));          // (dx=-3,dy=-3)
    CHECK(!CircleArray_containsGrid(a, 7, 3));          // out of bounds
    CHECK(!CircleArray_containsGrid(a, -1, 3));

    CircleArray_free(a);
}

// Checks indexed writes and reads across the circular array's logical bounds.
static void test_access_round_trip(void) {
    CircleArray *a = CircleArray(2, ID_INT);
    CHECK(a != nullptr);

    // Offset write / grid read must agree.
    int v = 1234;
    CHECK(CircleArray_setOffset(a, 0, 0, &v));
    int got = 0;
    CHECK(CircleArray_getGrid(&got, a, 2, 2));          // center grid == radius
    CHECK(got == 1234);

    CHECK(CircleArray_setOffset(a, 2, 0, &(int){ 7 }));
    CHECK(CircleArray_getOffset(&got, a, 2, 0));
    CHECK(got == 7);
    CHECK(CircleArray_setGrid(a, 2, 1, &(int){ 9 }));
    CHECK(CircleArray_getOffset(&got, a, 0, -1));
    CHECK(got == 9);

    // Initial cells are zeroed.
    CHECK(CircleArray_getOffset(&got, a, 1, 1));        // 2 > 4? no: 1+1=2<=4 valid
    CHECK(got == 0);

    // Out-of-disk and out-of-bounds writes are refused and leave neighbors.
    CHECK(!CircleArray_setOffset(a, 2, 2, &(int){ 99 }));
    CHECK(!CircleArray_setGrid(a, 5, 5, &(int){ 99 }));
    CHECK(!CircleArray_getOffset(&got, a, 2, 2));
    CHECK(!CircleArray_getGrid(&got, a, 5, 5));
    CHECK(CircleArray_getOffset(&got, a, 0, 0));
    CHECK(got == 1234);                                 // untouched

    // nullptr src / dest are refused.
    CHECK(!CircleArray_setOffset(a, 0, 0, nullptr));
    CHECK(!CircleArray_getOffset(nullptr, a, 0, 0));

    // Slot accessors.
    CHECK(CircleArray_slotOffset(a, 0, 0) != nullptr);
    CHECK(CircleArray_slotGrid(a, 2, 2) != nullptr);
    CHECK(CircleArray_slotOffset(a, 2, 2) == nullptr);  // corner, outside disk
    CHECK(CircleArray_slotGrid(a, 9, 9) == nullptr);
    CHECK(CircleArray_slotOffset(nullptr, 0, 0) == nullptr);
    CHECK(CircleArray_slotGrid(nullptr, 0, 0) == nullptr);

    CircleArray_free(a);
}

typedef struct WalkStats {
    bytes count;
    bytes badDistance;
    bytes nullElement;
    bytes mismatchedOffset;
    int64_t checksum;
} WalkStats;

// Accumulates callback count, coordinate consistency, and payload checksum.
static void walkCell(i32 gridX, i32 gridY, i32 dx, i32 dy,
                     const_any element, any userData) {
    WalkStats *s = (WalkStats*) userData;
    (*s).count++;
    if (element == nullptr)
        (*s).nullElement++;
    int64_t dist = (int64_t) dx * dx + (int64_t) dy * dy;
    if (dist > 9)                                       // radius 3
        (*s).badDistance++;
    if (gridX != dx + 3 || gridY != dy + 3)
        (*s).mismatchedOffset++;
    if (element != nullptr) {
        int v = 0;
        for (bytes i = 0; i < sizeof(int); i++)
            ((u8*) &v)[i] = ((const u8*) element)[i];
        (*s).checksum += v;
    }
}

// Confirms iteration visits each occupied circle-array element exactly once.
static void test_for_each(void) {
    CircleArray *a = CircleArray(3, ID_INT);
    CHECK(a != nullptr);

    // Fill every valid cell with its linear (1 + dx + dy) to make the walk sum
    // deterministic and to prove the callback sees live data.
    int fill = 1;
    for (i32 gy = 0; gy < CircleArray_diameter(a); gy++) {
        for (i32 gx = 0; gx < CircleArray_diameter(a); gx++) {
            if (CircleArray_containsGrid(a, gx, gy)) {
                int v = fill;
                CHECK(CircleArray_setGrid(a, gx, gy, &v));
                fill++;
            }
        }
    }

    WalkStats s = { 0 };
    CircleArray_forEach(a, walkCell, &s);
    CHECK(s.count == 29);                               // exact lattice count
    CHECK(s.badDistance == 0);
    CHECK(s.nullElement == 0);
    CHECK(s.mismatchedOffset == 0);
    // Sum of 1..29.
    CHECK(s.checksum == (int64_t) 29 * 30 / 2);

    CircleArray_forEach(a, nullptr, &s);                // safe
    CircleArray_forEach(nullptr, walkCell, &s);         // safe
    CircleArray_free(a);
}

// Runs circle-array construction, geometry, access, iteration, and teardown checks.
int main(void) {
    test_construction();
    test_geometry();
    test_access_round_trip();
    test_for_each();

    if (g_failures == 0) {
        printf("circle_array_test: all assertions held\n");
        return 0;
    }
    printf("circle_array_test: %d FAILURES\n", g_failures);
    return 1;
}
