#ifndef STRUCT_OCTREE_H
#define STRUCT_OCTREE_H

#include "vex.h"
#include "struct/exhaustion.h"

// struct/octree.h — 3D Spatial Partitioning Octree.
//
// Recursively partitions 3D space into 8 sub-octants for O(log N) point insertion,
// bounding-box (AABB) intersection range queries, and spherical range queries.

#define OCTREE_DEFAULT_MAX_DEPTH 8
#define OCTREE_DEFAULT_NODE_CAPACITY 8

typedef struct OctreePoint {
    f32 x;
    f32 y;
    f32 z;
} OctreePoint;

static inline OctreePoint OctreePoint_3(f32 x, f32 y, f32 z) {
    return (OctreePoint) { .x = x, .y = y, .z = z };
}
#define OctreePoint(...) CONSTRUCTOR_DISPATCH(OctreePoint, __VA_ARGS__)
static inline fn OctreePoint_set(OctreePoint *point, f32 x, f32 y, f32 z) {
    if (point) { (*point).x = x; (*point).y = y; (*point).z = z; }
}

typedef struct OctreeAABB {
    f32 minX;
    f32 minY;
    f32 minZ;
    f32 maxX;
    f32 maxY;
    f32 maxZ;
} OctreeAABB;

static inline OctreeAABB OctreeAABB_6(f32 minX, f32 minY, f32 minZ,
                                      f32 maxX, f32 maxY, f32 maxZ) {
    return (OctreeAABB) { .minX = minX, .minY = minY, .minZ = minZ,
        .maxX = maxX, .maxY = maxY, .maxZ = maxZ };
}
#define OctreeAABB(...) CONSTRUCTOR_DISPATCH(OctreeAABB, __VA_ARGS__)
static inline fn OctreeAABB_setMin(OctreeAABB *box, f32 x, f32 y, f32 z) {
    if (box) { (*box).minX = x; (*box).minY = y; (*box).minZ = z; }
}
static inline fn OctreeAABB_setMax(OctreeAABB *box, f32 x, f32 y, f32 z) {
    if (box) { (*box).maxX = x; (*box).maxY = y; (*box).maxZ = z; }
}

typedef struct OctreeItem {
    OctreePoint point;
    u64    payload;
} OctreeItem;

typedef struct OctreeNode {
    OctreeAABB         bounds;
    OctreeItem        *items;
    u32           itemCount;
    u32           itemCapacity;
    bool               isLeaf;
    struct OctreeNode *children[8];
} OctreeNode;

typedef struct Octree {
    OctreeNode *root;
    u32    maxDepth;
    u32    maxItemsPerNode;
    bytes      totalItems;
    // Exhaustion observability (the Exhaustion Loudness Law): a refused leaf
    // growth is counted, and the epoch's first refusal reports.
    u64    exhaustionCount;
    bool        exhaustionReported;
} Octree;

Octree *Octree_3(OctreeAABB bounds, u32 maxDepth, u32 maxItemsPerNode);
#define Octree(...) CONSTRUCTOR_DISPATCH(Octree, __VA_ARGS__)
fn Octree_free(Octree *self);

bool Octree_insert(Octree *self, OctreePoint point, u64 payload);
bytes Octree_queryRange(const Octree *self, OctreeAABB range, u64 *outPayloads, bytes maxCount);
bytes Octree_querySphere(const Octree *self, OctreePoint center, f32 radius, u64 *outPayloads, bytes maxCount);

bytes Octree_count(const Octree *self);
fn Octree_clear(Octree *self);
// Refused-insertion count since creation (the Exhaustion Loudness Law).
;;TEST
u64 Octree_exhaustionCount(const Octree *self);

// AABB helper functions
bool OctreeAABB_containsPoint(OctreeAABB box, OctreePoint p);
bool OctreeAABB_intersects(OctreeAABB a, OctreeAABB b);
bool OctreeAABB_intersectsSphere(OctreeAABB box, OctreePoint center, f32 radius);

#endif
