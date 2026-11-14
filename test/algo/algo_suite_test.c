#include "vex.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "algo/sort.h"
#include "algo/dijkstra.h"
#include "algo/kd_tree.h"
#include "algo/bvh.h"
#include "algo/tree_sit.h"
#include "lang/point/point.h"
#include "lang/rect/rectangle.h"
#include "nio/mem.h"

static fn test_radix_sort() {
    u32 a32[256];
    for (int i = 0; i < 256; i++) {
        a32[i] = (u32) ((255 - i) * 179424673u + 37u);
    }
    assert(Sort_radixSortUint32(a32, 256u));
    for (int i = 0; i < 255; i++) {
        assert(a32[i] <= a32[i + 1]);
    }

    u64 a64[256];
    for (int i = 0; i < 256; i++) {
        a64[i] = ((u64) (255 - i) << 32) | (u64) ((i * 31337) ^ 0xDEADBEEFULL);
    }
    assert(Sort_radixSortUint64(a64, 256u));
    for (int i = 0; i < 255; i++) {
        assert(a64[i] <= a64[i + 1]);
    }

    u64 keys[64];
    any values[64];
    for (int i = 0; i < 64; i++) {
        keys[i] = (u64) (63 - i);
        values[i] = (any) (pointer) (i * 10);
    }
    assert(Sort_radixSortPairsUint64(keys, values, 64u));
    for (int i = 0; i < 63; i++) {
        assert(keys[i] <= keys[i + 1]);
        assert((pointer) values[i] == (pointer) ((63 - keys[i]) * 10));
    }
}

static fn test_dijkstra() {
    DijkstraGraph *g = Dijkstra(6);
    assert(g != nullptr);

    Dijkstra_addEdge(g, 0, 1, 7.0f);
    Dijkstra_addEdge(g, 0, 2, 9.0f);
    Dijkstra_addEdge(g, 0, 5, 14.0f);
    Dijkstra_addEdge(g, 1, 2, 10.0f);
    Dijkstra_addEdge(g, 1, 3, 15.0f);
    Dijkstra_addEdge(g, 2, 3, 11.0f);
    Dijkstra_addEdge(g, 2, 5, 2.0f);
    Dijkstra_addEdge(g, 3, 4, 6.0f);
    Dijkstra_addEdge(g, 4, 5, 9.0f);

    u32 path[8];
    f32 total_dist = 0.0f;
    bytes path_len = Dijkstra_shortestPath(g, 0, 5, path, 8, &total_dist);

    // Shortest path from 0 to 5: 0 -> 2 -> 5 with cost 9 + 2 = 11.0
    assert(fabsf(total_dist - 11.0f) < 0.001f);
    assert(path_len == 3);
    assert(path[0] == 0);
    assert(path[1] == 2);
    assert(path[2] == 5);

    Dijkstra_free(g);
}

static fn test_geometry_constructors() {
    Point *origin = Point();
    assert(origin != nullptr && (*origin).x == 0 && (*origin).y == 0);
    Point_free(origin);
    Point *pixel = Point(320, 240);
    assert(pixel != nullptr && (*pixel).x == 320 && (*pixel).y == 240);
    Point_free(pixel);

    Rectangle *empty = Rectangle();
    assert(empty != nullptr && (*empty).width == 0.0f && (*empty).height == 0.0f);
    Rectangle_free(empty);
    Rectangle *bounds = Rectangle(10.0f, 20.0f, 640.0f, 480.0f);
    assert(bounds != nullptr && (*bounds).x == 10.0f && (*bounds).width == 640.0f);
    Rectangle_free(bounds);
}

static fn test_kd_tree() {
    KdPoint pts[10];
    for (int i = 0; i < 10; i++) {
        pts[i].coord[0] = (f32) i;
        pts[i].coord[1] = (f32) (i * 2);
        pts[i].coord[2] = (f32) (i * 3);
        pts[i].payload = (u64) i;
    }

    KdTree *tree = KdTree_build(pts, 10);
    assert(tree != nullptr);

    f32 query[3] = { 3.1f, 5.9f, 9.1f };
    KdPoint nearest;
    f32 dist_sq = 0.0f;

    bool found = KdTree_nearest(tree, query, &nearest, &dist_sq);
    assert(found == true);
    assert(nearest.payload == 3); // (3, 6, 9) is closest

    u64 payloads[10];
    bytes count = KdTree_queryRadius(tree, query, 5.0f, payloads, 10);
    assert(count > 0);

    KdTree_free(tree);
}

static fn test_bvh() {
    BvhPrimitive prims[3];

    // Primitive 0: box around (0, 0, 5)
    prims[0].id = 100;
    prims[0].bounds = BvhAabb_from_points((f32[3]){-1.0f, -1.0f, 4.0f}, (f32[3]){1.0f, 1.0f, 6.0f});
    prims[0].centroid[0] = 0.0f; prims[0].centroid[1] = 0.0f; prims[0].centroid[2] = 5.0f;

    // Primitive 1: box around (10, 10, 5)
    prims[1].id = 101;
    prims[1].bounds = BvhAabb_from_points((f32[3]){9.0f, 9.0f, 4.0f}, (f32[3]){11.0f, 11.0f, 6.0f});
    prims[1].centroid[0] = 10.0f; prims[1].centroid[1] = 10.0f; prims[1].centroid[2] = 5.0f;

    // Primitive 2: box around (-10, -10, 5)
    prims[2].id = 102;
    prims[2].bounds = BvhAabb_from_points((f32[3]){-11.0f, -11.0f, 4.0f}, (f32[3]){-9.0f, -9.0f, 6.0f});
    prims[2].centroid[0] = -10.0f; prims[2].centroid[1] = -10.0f; prims[2].centroid[2] = 5.0f;

    BvhTree tree = BvhTree(3u);
    assert(tree.nodes != nullptr);
    assert(BvhTree_build(prims, 3u, &tree));

    // Ray pointing along Z axis towards (0, 0, 5)
    BvhRay ray;
    ray.origin[0] = 0.0f; ray.origin[1] = 0.0f; ray.origin[2] = 0.0f;
    ray.direction[0] = 0.0f; ray.direction[1] = 0.0f; ray.direction[2] = 1.0f;
    ray.t_min = 0.0f; ray.t_max = 100.0f;

    BvhRayHit hit;
    assert(BvhTree_intersect_ray(&tree, &ray, &hit));
    assert(hit.hit == true);
    assert(hit.primitive_id == 100);
    assert(fabsf(hit.t - 4.0f) < 0.001f);

    // Ray missing all boxes
    ray.direction[0] = 0.0f; ray.direction[1] = 1.0f; ray.direction[2] = 0.0f;
    assert(!BvhTree_intersect_ray(&tree, &ray, &hit) || hit.hit == false);

    BvhTree_destroy(&tree);
}

static bool tree_sit_counter(const TreeSitTree *tree, i32 node_idx, int depth, any user_data) {
    (fn) tree;
    (fn) node_idx;
    (fn) depth;
    int *c = (int*) user_data;
    (*c)++;
    return true;
}

static fn test_tree_sit() {
    TreeSitTree tree = TreeSitTree(16u);
    assert(tree.nodes != nullptr);

    TreeSitSpan root_span = {0, 100, 1, 0, 10, 0};
    i32 root = TreeSitTree_add_node(-1, 1, "module", root_span, &tree);
    assert(root == 0);

    TreeSitSpan f1_span = {0, 40, 1, 0, 4, 0};
    i32 f1 = TreeSitTree_add_node(root, 2, "func1", f1_span, &tree);

    TreeSitSpan stmt_span = {10, 25, 2, 4, 2, 19};
    i32 st = TreeSitTree_add_node(f1, 3, "return", stmt_span, &tree);
    assert(st == 2);

    TreeSitSpan f2_span = {41, 100, 5, 0, 10, 0};
    i32 f2 = TreeSitTree_add_node(root, 2, "func2", f2_span, &tree);
    assert(f2 == 3);

    int count_pre = 0;
    TreeSitTree_walk_preorder(&tree, tree_sit_counter, &count_pre);
    assert(count_pre == 4);

    int count_post = 0;
    TreeSitTree_walk_postorder(&tree, tree_sit_counter, &count_post);
    assert(count_post == 4);

    assert(TreeSitTree_count_kind(&tree, 2) == 2); // 2 functions

    i32 found = TreeSitTree_find_by_byte(&tree, 15);
    assert(found == st); // inside return statement

    TreeSitTree_destroy(&tree);
}

int main() {
    printf("[Algo Suite Test] Starting test suite...\n");
    assert(Memory_init(0u));
    test_radix_sort();
    test_dijkstra();
    test_geometry_constructors();
    test_kd_tree();
    test_bvh();
    test_tree_sit();
    Memory_freeAll();
    printf("[Algo Suite Test] ALL TESTS PASSED\n");
    return 0;
}
