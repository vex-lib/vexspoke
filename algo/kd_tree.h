#ifndef ALGO_KD_TREE_H
#define ALGO_KD_TREE_H

#include "vex.h"

// algo/kd_tree.h — 3-Dimensional K-D Tree for Spatial Search.
//
// Organizes 3D points by cycling coordinate split planes (X -> Y -> Z)
// to accelerate nearest-neighbor queries and spherical proximity searches.

typedef struct KdPoint {
    f32    coord[3]; // [0] = x/horizontal, [1] = y/vertical, [2] = z/depth
    u64 payload;
} KdPoint;

static inline KdPoint KdPoint_0() { return (KdPoint) {0}; }
static inline KdPoint KdPoint_4(f32 horizontal, f32 vertical, f32 depth,
                               u64 payload) {
    return (KdPoint) { .coord = { horizontal, vertical, depth }, .payload = payload };
}
#define KdPoint(...) CONSTRUCTOR_DISPATCH(KdPoint, __VA_ARGS__)

typedef struct KdNode {
    KdPoint        point;
    u8        axis; // 0 = X, 1 = Y, 2 = Z
    struct KdNode *left;
    struct KdNode *right;
} KdNode;

typedef struct KdTree {
    KdNode *root;
    bytes  count;
} KdTree;

// Build a balanced KD-tree from an array of 3D points
KdTree *KdTree_build(KdPoint *points, bytes count);
fn    KdTree_free(KdTree *tree);

// Find single nearest neighbor to query point.
// Dest-last order: writes to outNearest and outDistanceSquared.
bool   KdTree_nearest(const KdTree *tree, const f32 queryCoord[3], KdPoint *outNearest, f32 *outDistSq);

// Find all points within radius of query point.
bytes KdTree_queryRadius(const KdTree *tree, const f32 queryCoord[3], f32 radius, u64 *outPayloads, bytes maxCount);

#endif
