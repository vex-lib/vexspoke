#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "nio/mem.h"
#include "oop/type.h"
#include "struct/circle_array.h"
#include "struct/sphere_array.h"
#include "struct/octree.h"

static int g_failures = 0;

#define TEST_ASSERT(expr, msg) do { \
    if (!(expr)) { \
        fprintf(stderr, "[spatial_struct_test] FAIL: %s (line %d)\n", msg, __LINE__); \
        g_failures++; \
    } else { \
        printf("[spatial_struct_test] PASS %s\n", msg); \
    } \
} while (0)

typedef struct CellPayload {
    int32_t val;
    float   weight;
} CellPayload;

// Counts visited circle-array cells and accumulates their grid coordinates.
// Counts CircleArray callback visits for comparison with its valid-cell count.
static void countCellsCb(int32_t gx, int32_t gy, int32_t dx, int32_t dy, const void *element, void *userData) {
    (void) gx; (void) gy; (void) dx; (void) dy; (void) element;
    size_t *count = (size_t*) userData;
    (*count)++;
}

// Counts visited sphere-array voxels and accumulates their grid coordinates.
// Counts SphereArray callback visits for comparison with its valid-voxel count.
static void countVoxelsCb(int32_t gx, int32_t gy, int32_t gz, int32_t dx, int32_t dy, int32_t dz, const void *voxel, void *userData) {
    (void) gx; (void) gy; (void) gz; (void) dx; (void) dy; (void) dz; (void) voxel;
    size_t *count = (size_t*) userData;
    (*count)++;
}

// Exercises circle-array placement, lookup, bounds, and callback traversal.
// Tests CircleArray geometry, callback counts, payload round trips, and rejected cells.
static void test_circle_array(void) {
    printf("\n--- Testing CircleArray (Pythagorean 2D Matrix) ---\n");
    int32_t r = 3;
    CircleArray *ca = CircleArray(r, ID_BYTE, sizeof(CellPayload));
    TEST_ASSERT(ca != nullptr, "CircleArray stride constructor succeeds");
    TEST_ASSERT(CircleArray_radius(ca) == 3, "radius is 3");
    TEST_ASSERT(CircleArray_diameter(ca) == 7, "diameter is 7 (2*3 + 1)");

    // Pythagorean containment tests
    TEST_ASSERT(CircleArray_containsOffset(ca, 0, 0), "center (0, 0) is inside");
    TEST_ASSERT(CircleArray_containsOffset(ca, 3, 0), "edge (3, 0) is inside (3^2 <= 9)");
    TEST_ASSERT(CircleArray_containsOffset(ca, 0, 3), "edge (0, 3) is inside (3^2 <= 9)");
    TEST_ASSERT(CircleArray_containsOffset(ca, -3, 0), "edge (-3, 0) is inside");
    TEST_ASSERT(CircleArray_containsOffset(ca, 0, -3), "edge (0, -3) is inside");
    TEST_ASSERT(CircleArray_containsOffset(ca, 2, 2), "diagonal (2, 2) is inside (4 + 4 = 8 <= 9)");
    TEST_ASSERT(!CircleArray_containsOffset(ca, 3, 3), "corner (3, 3) is outside (9 + 9 = 18 > 9)");
    TEST_ASSERT(!CircleArray_containsOffset(ca, 2, 3), "point (2, 3) is outside (4 + 9 = 13 > 9)");

    // Grid coordinates
    TEST_ASSERT(CircleArray_containsGrid(ca, 3, 3), "grid center (3, 3) is inside");
    TEST_ASSERT(CircleArray_containsGrid(ca, 6, 3), "grid edge (6, 3) is inside");
    TEST_ASSERT(!CircleArray_containsGrid(ca, 6, 6), "grid corner (6, 6) is outside");
    TEST_ASSERT(!CircleArray_containsGrid(ca, -1, 3), "negative gridX rejected");
    TEST_ASSERT(!CircleArray_containsGrid(ca, 3, 7), "out-of-bounds gridY rejected");

    // Valid cell count vs forEach iteration count
    size_t validCells = CircleArray_validCellCount(ca);
    TEST_ASSERT(validCells > 0 && validCells < 49, "valid cell count is between 0 and 49");
    size_t iteratedCells = 0;
    CircleArray_forEach(ca, countCellsCb, &iteratedCells);
    TEST_ASSERT(iteratedCells == validCells, "forEach visited exactly validCellCount cells");

    // Write and read payload
    CellPayload writeVal = { 42, 3.1415f };
    TEST_ASSERT(CircleArray_setOffset(ca, 2, 2, &writeVal), "write to valid offset (2, 2) succeeds");

    CellPayload readVal = { 0, 0.0f };
    TEST_ASSERT(CircleArray_getOffset(&readVal, ca, 2, 2), "read from valid offset (2, 2) succeeds");
    TEST_ASSERT(readVal.val == 42 && readVal.weight == 3.1415f, "read payload matches written payload");

    // Write to outside cell fails
    CellPayload outsideVal = { 99, 1.0f };
    TEST_ASSERT(!CircleArray_setOffset(ca, 3, 3, &outsideVal), "write to outside cell (3, 3) rejected");
    TEST_ASSERT(!CircleArray_getOffset(&readVal, ca, 3, 3), "read from outside cell (3, 3) rejected");

    CircleArray_free(ca);
}

// Checks sphere-array voxel access, geometry, bounds, and traversal callbacks.
// Tests SphereArray containment, traversal counts, payload access, and outside rejection.
static void test_sphere_array(void) {
    printf("\n--- Testing SphereArray (Pythagorean 3D Voxel Matrix) ---\n");
    int32_t r = 2;
    SphereArray *sa = SphereArray(r, ID_BYTE, sizeof(CellPayload));
    TEST_ASSERT(sa != nullptr, "SphereArray stride constructor succeeds");
    TEST_ASSERT(SphereArray_radius(sa) == 2, "radius is 2");
    TEST_ASSERT(SphereArray_diameter(sa) == 5, "diameter is 5 (2*2 + 1)");

    // 3D Pythagorean containment
    TEST_ASSERT(SphereArray_containsOffset(sa, 0, 0, 0), "center (0, 0, 0) inside");
    TEST_ASSERT(SphereArray_containsOffset(sa, 2, 0, 0), "axial edge (2, 0, 0) inside (4 <= 4)");
    TEST_ASSERT(SphereArray_containsOffset(sa, 0, 2, 0), "axial edge (0, 2, 0) inside (4 <= 4)");
    TEST_ASSERT(SphereArray_containsOffset(sa, 0, 0, 2), "axial edge (0, 0, 2) inside (4 <= 4)");
    TEST_ASSERT(SphereArray_containsOffset(sa, 1, 1, 1), "diagonal (1, 1, 1) inside (1+1+1=3 <= 4)");
    TEST_ASSERT(!SphereArray_containsOffset(sa, 2, 2, 0), "planar corner (2, 2, 0) outside (4+4=8 > 4)");
    TEST_ASSERT(!SphereArray_containsOffset(sa, 2, 2, 2), "cubic corner (2, 2, 2) outside (4+4+4=12 > 4)");

    size_t validVoxels = SphereArray_validVoxelCount(sa);
    TEST_ASSERT(validVoxels > 0 && validVoxels < 125, "valid voxels count is between 0 and 125");
    size_t iteratedVoxels = 0;
    SphereArray_forEach(sa, countVoxelsCb, &iteratedVoxels);
    TEST_ASSERT(iteratedVoxels == validVoxels, "forEach visited exactly validVoxelCount voxels");

    CellPayload vWrite = { 101, 2.718f };
    TEST_ASSERT(SphereArray_setOffset(sa, 1, 1, 1, &vWrite), "write to valid voxel (1, 1, 1) succeeds");
    CellPayload vRead = { 0, 0.0f };
    TEST_ASSERT(SphereArray_getOffset(&vRead, sa, 1, 1, 1), "read from valid voxel (1, 1, 1) succeeds");
    TEST_ASSERT(vRead.val == 101 && vRead.weight == 2.718f, "read voxel payload matches");

    TEST_ASSERT(!SphereArray_setOffset(sa, 2, 2, 2, &vWrite), "write to outside voxel (2, 2, 2) rejected");

    SphereArray_free(sa);
}

// Verifies octree insertion and spatial query results for the tested tree.
// Checks Octree insertion/splitting, range and sphere queries, clearing, and cleanup.
static void test_octree(void) {
    printf("\n--- Testing Octree (3D Spatial Partitioning) ---\n");
    OctreeAABB bounds = { -100.0f, -100.0f, -100.0f, 100.0f, 100.0f, 100.0f };
    Octree *oct = Octree(bounds, 4, 2); // capacity 2 items before subdividing
    TEST_ASSERT(oct != nullptr, "Octree constructor succeeds");

    // Insert 4 items
    TEST_ASSERT(Octree_insert(oct, (OctreePoint){ 10.0f, 10.0f, 10.0f }, 1001), "insert item 1");
    TEST_ASSERT(Octree_insert(oct, (OctreePoint){ 12.0f, 12.0f, 12.0f }, 1002), "insert item 2");
    TEST_ASSERT(Octree_insert(oct, (OctreePoint){ -50.0f, 20.0f, 30.0f }, 2001), "insert item 3 (causes split)");
    TEST_ASSERT(Octree_insert(oct, (OctreePoint){ 70.0f, -80.0f, 60.0f }, 3001), "insert item 4");
    TEST_ASSERT(Octree_count(oct) == 4, "Octree total count is 4");

    // Out of bounds insertion fails
    TEST_ASSERT(!Octree_insert(oct, (OctreePoint){ 200.0f, 0.0f, 0.0f }, 9999), "insert out of bounds fails");

    // Range Query (AABB)
    uint64_t results[8];
    OctreeAABB queryBox = { 0.0f, 0.0f, 0.0f, 20.0f, 20.0f, 20.0f };
    size_t matched = Octree_queryRange(oct, queryBox, results, 8);
    TEST_ASSERT(matched == 2, "queryRange found 2 items in [0, 20]^3");
    bool found1001 = (results[0] == 1001 || results[1] == 1001);
    bool found1002 = (results[0] == 1002 || results[1] == 1002);
    TEST_ASSERT(found1001 && found1002, "queryRange returned correct payloads (1001 and 1002)");

    // Sphere Query (Pythagorean radius query)
    OctreePoint sphereCenter = { 10.0f, 10.0f, 10.0f };
    float radius = 5.0f; // distance to (12, 12, 12) is sqrt(4+4+4) = 3.464 <= 5.0
    matched = Octree_querySphere(oct, sphereCenter, radius, results, 8);
    TEST_ASSERT(matched == 2, "querySphere found 2 items within radius 5.0");

    // Sphere query with small radius
    matched = Octree_querySphere(oct, sphereCenter, 1.0f, results, 8);
    TEST_ASSERT(matched == 1, "querySphere with radius 1.0 found only 1 item (itself)");
    TEST_ASSERT(results[0] == 1001, "item found is 1001");

    // Clear octree
    Octree_clear(oct);
    TEST_ASSERT(Octree_count(oct) == 0, "count after clear is 0");
    matched = Octree_queryRange(oct, queryBox, results, 8);
    TEST_ASSERT(matched == 0, "queryRange after clear returns 0");

    Octree_free(oct);
}

// Runs the circle-array, sphere-array, and octree spatial-structure cases.
// Initializes memory and runs the three spatial-structure behavior suites.
int main(void) {
    printf("=== Running Spatial Data Structures Test Suite ===\n");
    Memory_init(32 * 1024 * 1024);

    test_circle_array();
    test_sphere_array();
    test_octree();

    printf("\n=== Spatial Data Structures Test Summary: %d failures ===\n", g_failures);
    return (g_failures == 0) ? 0 : 1;
}
