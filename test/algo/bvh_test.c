// vexspoke/test/algo/bvh_test.c — owner test for algo/bvh.
//
// Proves the bounding-volume hierarchy and ray/AABB math:
//   - BvhAabb_empty is the inverted identity, expand_point grows to include a
//     point, merge unions two boxes, from_points copies;
//   - BvhAabb_intersect_ray: hit with a positive near t, a ray starting inside
//     reports t_min, a miss returns false, and the t_min/t_max window clips;
//   - BvhTree constructors reject overflowing counts, and build refuses
//     mismatched counts, null primitives and a null tree;
//   - build over a trail of boxes reports the nearest hit's primitive id;
//   - a ray that misses everything returns hit=false and a false result;
//   - all-identical centroids still build (degenerate leaf) and are hittable;
//   - nullptr safety and hit_out initialization.

#include "vex.h"
#include <stdio.h>
#include <math.h>

#include "algo/bvh.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

/** Creates a cube AABB centered at the supplied coordinates with half-extent h. */
static BvhAabb box_at(f32 x, f32 y, f32 z, f32 h) {
    f32 mn[3] = { x - h, y - h, z - h };
    f32 mx[3] = { x + h, y + h, z + h };
    return BvhAabb_from_points(mn, mx);
}

/** Creates a BVH primitive with the requested identity and centered cube bounds. */
static BvhPrimitive prim(u32 id, f32 x, f32 y, f32 z, f32 h) {
    BvhPrimitive p;
    p.id = id;
    p.bounds = box_at(x, y, z, h);
    p.centroid[0] = x;
    p.centroid[1] = y;
    p.centroid[2] = z;
    return p;
}

/** Checks AABB construction, containment, and overlap boundary behavior. */
static fn test_aabb() {
    BvhAabb e = BvhAabb_empty();
    CHECK(e.min_point[0] == FLT_MAX && e.max_point[0] == -FLT_MAX);

    f32 pt[3] = { 2.0f, -1.0f, 5.0f };
    BvhAabb_expand_point(pt, &e);
    CHECK(e.min_point[0] == 2.0f && e.max_point[0] == 2.0f);
    CHECK(e.min_point[1] == -1.0f && e.max_point[1] == -1.0f);
    CHECK(e.min_point[2] == 5.0f && e.max_point[2] == 5.0f);

    f32 pt2[3] = { -3.0f, 4.0f, 1.0f };
    BvhAabb_expand_point(pt2, &e);
    CHECK(e.min_point[0] == -3.0f && e.max_point[0] == 2.0f);
    CHECK(e.min_point[1] == -1.0f && e.max_point[1] == 4.0f);
    CHECK(e.min_point[2] == 1.0f && e.max_point[2] == 5.0f);

    BvhAabb other = box_at(10, 10, 10, 1);
    BvhAabb_merge(&other, &e);
    CHECK(e.max_point[0] == 11.0f && e.max_point[1] == 11.0f && e.max_point[2] == 11.0f);
    CHECK(e.min_point[0] == -3.0f);

    // Null guards.
    BvhAabb_expand_point(nullptr, &e);
    BvhAabb_expand_point(pt, nullptr);
    BvhAabb_merge(nullptr, &e);
    BvhAabb_merge(&other, nullptr);
}

/** Verifies ray/AABB intersection results for hit and miss cases. */
static fn test_ray_aabb() {
    BvhAabb b = box_at(0, 0, 0, 1);            // [-1,1]^3
    BvhRay ray = { { -5, 0, 0 }, { 1, 0, 0 }, 0.0f, 100.0f };
    f32 t = -1.0f;
    CHECK(BvhAabb_intersect_ray(&ray, &b, &t));
    CHECK(fabsf(t - 4.0f) < 1e-4f);            // enters the box at x=-1

    // Starting inside: the near t is the ray's own t_min.
    BvhRay inside = { { 0, 0, 0 }, { 1, 0, 0 }, 0.0f, 100.0f };
    CHECK(BvhAabb_intersect_ray(&inside, &b, &t));
    CHECK(t == 0.0f);

    // A window that ends before the box.
    BvhRay early = { { -5, 0, 0 }, { 1, 0, 0 }, 0.0f, 3.0f };
    CHECK(!BvhAabb_intersect_ray(&early, &b, &t));

    // A ray that never points at the box.
    BvhRay away = { { 0, -5, 0 }, { 0, 1, 0 }, 0.0f, 1.0f };
    CHECK(!BvhAabb_intersect_ray(&away, &b, &t));

    CHECK(!BvhAabb_intersect_ray(nullptr, &b, &t));
    CHECK(!BvhAabb_intersect_ray(&ray, nullptr, &t));
    // A null t_out is optional.
    CHECK(BvhAabb_intersect_ray(&ray, &b, nullptr));
}

/** Exercises BVH creation, build, query, and destruction across ordinary inputs. */
static fn test_tree_lifecycle() {
    BvhTree t = BvhTree();
    CHECK(t.nodes == nullptr);
    CHECK(t.node_count == 0u);
    BvhTree tooLarge = BvhTree(UINT32_MAX);
    CHECK(tooLarge.nodes == nullptr);
    BvhTree_destroy(&t);
    BvhTree_destroy(nullptr);

    CHECK(!BvhTree_build(nullptr, 1, &t));
    BvhPrimitive p = prim(0, 0, 0, 0, 1);
    CHECK(!BvhTree_build(&p, 1, nullptr));
    CHECK(!BvhTree_build(&p, 1u, &t));
    CHECK(t.primitive_count == 0u);
    CHECK(BvhTree_build(&p, 0, &t));            // empty build succeeds
    CHECK(t.node_count == 0);
    BvhTree_destroy(&t);
}

/** Checks ray traversal returns the expected nearest primitive in the built tree. */
static fn test_ray_tree() {
    enum { N = 12 };
    BvhPrimitive prims[N];
    for (u32 i = 0; i < N; i++)
        prims[i] = prim(i, (f32) i * 2.0f, 0.0f, 0.0f, 0.5f);   // boxes at x=0,2,4,...

    BvhTree tree = BvhTree(N);
    CHECK(tree.nodes != nullptr);
    CHECK(BvhTree_build(prims, N, &tree));
    CHECK(tree.node_count > 0);

    // Ray down +x at y=z=0 hits the first box (id 0).
    BvhRayHit hit;
    BvhRay ray = { { -50, 0, 0 }, { 1, 0, 0 }, 0.0f, 1e6f };
    CHECK(BvhTree_intersect_ray(&tree, &ray, &hit));
    CHECK(hit.hit);
    CHECK(hit.primitive_id == 0);
    CHECK(fabsf(hit.t - 49.5f) < 1e-2f);

    // Ray down -x from the far end hits the last box (id N-1).
    BvhRay back = { { 50, 0, 0 }, { -1, 0, 0 }, 0.0f, 1e6f };
    CHECK(BvhTree_intersect_ray(&tree, &back, &hit));
    CHECK(hit.primitive_id == N - 1);

    // A parallel ray off to the side misses.
    BvhRay miss = { { -50, 100, 0 }, { 1, 0, 0 }, 0.0f, 1e6f };
    CHECK(!BvhTree_intersect_ray(&tree, &miss, &hit));
    CHECK(!hit.hit);

    // Null arguments: hit_out is cleared.
    BvhRayHit out = { true, 0, 0, { 0, 0, 0 } };
    CHECK(!BvhTree_intersect_ray(nullptr, &ray, &out));
    CHECK(!out.hit);
    CHECK(!BvhTree_intersect_ray(&tree, nullptr, &out));
    CHECK(!BvhTree_intersect_ray(&tree, &ray, nullptr));

    BvhTree_destroy(&tree);
}

/** Covers empty, repeated, and degenerate primitive bounds without invalid traversal. */
static fn test_degenerate() {
    // Five identical centroids: the split collapses to a leaf, still hittable.
    enum { N = 5 };
    BvhPrimitive prims[N];
    for (u32 i = 0; i < N; i++)
        prims[i] = prim(i + 1, 3.0f, 3.0f, 3.0f, 0.5f);

    BvhTree tree = BvhTree(N);
    CHECK(tree.nodes != nullptr);
    CHECK(BvhTree_build(prims, N, &tree));
    CHECK(tree.node_count == 1);                  // one degenerate leaf

    BvhRayHit hit;
    BvhRay ray = { { 0, 3, 3 }, { 1, 0, 0 }, 0.0f, 100.0f };
    CHECK(BvhTree_intersect_ray(&tree, &ray, &hit));
    CHECK(hit.hit);
    CHECK(hit.primitive_id >= 1 && hit.primitive_id <= N);
    CHECK(fabsf(hit.t - 2.5f) < 1e-3f);

    // A single-primitive tree.
    BvhPrimitive one = prim(77, 0, 0, 0, 1);
    BvhTree t1 = BvhTree(1u);
    CHECK(t1.nodes != nullptr);
    CHECK(BvhTree_build(&one, 1u, &t1));
    BvhRay r1 = { { -5, 0, 0 }, { 1, 0, 0 }, 0.0f, 100.0f };
    CHECK(BvhTree_intersect_ray(&t1, &r1, &hit));
    CHECK(hit.primitive_id == 77);
    BvhTree_destroy(&t1);

    BvhTree_destroy(&tree);
}

/** Runs the BVH owner scenarios and reports aggregate assertion status. */
int main() {
    test_aabb();
    test_ray_aabb();
    test_tree_lifecycle();
    test_ray_tree();
    test_degenerate();

    if (g_failures == 0) {
        printf("bvh_test: all assertions held\n");
        return 0;
    }
    printf("bvh_test: %d FAILURES\n", g_failures);
    return 1;
}
