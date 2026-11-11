#ifndef VEXSPOKE_ALGO_BVH_H
#define VEXSPOKE_ALGO_BVH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lang/constructor.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BvhAabb {
    f32 min_point[3];
    f32 max_point[3];
} BvhAabb;

static inline BvhAabb BvhAabb_0(void) { return (BvhAabb) {0}; }
#define BvhAabb(...) CONSTRUCTOR_DISPATCH(BvhAabb, __VA_ARGS__)
static inline void BvhAabb_setMinimum(BvhAabb *box, f32 x, f32 y, f32 z) {
    if (box) { (*box).min_point[0] = x; (*box).min_point[1] = y; (*box).min_point[2] = z; }
}
static inline void BvhAabb_setMaximum(BvhAabb *box, f32 x, f32 y, f32 z) {
    if (box) { (*box).max_point[0] = x; (*box).max_point[1] = y; (*box).max_point[2] = z; }
}

typedef struct BvhRay {
    f32 origin[3];
    f32 direction[3];
    f32 t_min;
    f32 t_max;
} BvhRay;

static inline BvhRay BvhRay_8(f32 originX, f32 originY, f32 originZ,
                              f32 directionX, f32 directionY, f32 directionZ,
                              f32 minimumDistance, f32 maximumDistance) {
    return (BvhRay) {
        .origin = { originX, originY, originZ },
        .direction = { directionX, directionY, directionZ },
        .t_min = minimumDistance,
        .t_max = maximumDistance
    };
}
#define BvhRay(...) CONSTRUCTOR_DISPATCH(BvhRay, __VA_ARGS__)
static inline void BvhRay_setOrigin(BvhRay *ray, f32 x, f32 y, f32 z) {
    if (ray) { (*ray).origin[0] = x; (*ray).origin[1] = y; (*ray).origin[2] = z; }
}
static inline void BvhRay_setDirection(BvhRay *ray, f32 x, f32 y, f32 z) {
    if (ray) { (*ray).direction[0] = x; (*ray).direction[1] = y; (*ray).direction[2] = z; }
}
static inline void BvhRay_setRange(BvhRay *ray, f32 minimumDistance, f32 maximumDistance) {
    if (ray) { (*ray).t_min = minimumDistance; (*ray).t_max = maximumDistance; }
}

typedef struct BvhRayHit {
    bool hit;
    f32 t;
    u32 primitive_id;
    f32 normal[3];
} BvhRayHit;

static inline BvhRayHit BvhRayHit_0(void) { return (BvhRayHit) {0}; }
#define BvhRayHit(...) CONSTRUCTOR_DISPATCH(BvhRayHit, __VA_ARGS__)

typedef struct BvhPrimitive {
    u32 id;
    BvhAabb bounds;
    f32 centroid[3];
} BvhPrimitive;

static inline BvhPrimitive BvhPrimitive_5(u32 id, BvhAabb bounds,
                                          f32 centroidX, f32 centroidY, f32 centroidZ) {
    return (BvhPrimitive) { .id = id, .bounds = bounds,
        .centroid = { centroidX, centroidY, centroidZ } };
}
#define BvhPrimitive(...) CONSTRUCTOR_DISPATCH(BvhPrimitive, __VA_ARGS__)

typedef struct BvhNode {
    BvhAabb bounds;
    i32 left_child;
    i32 right_child;
    u32 primitive_offset;
    u32 primitive_count;
    bool is_leaf;
} BvhNode;

typedef struct BvhTree {
    BvhNode *nodes;
    u32 *primitive_indices;
    const BvhPrimitive *primitives;
    u32 node_count;
    u32 node_capacity;
    u32 primitive_count;
} BvhTree;

BvhAabb BvhAabb_empty(void);
BvhAabb BvhAabb_from_points(const f32 min_pt[3], const f32 max_pt[3]);
void BvhAabb_expand_point(const f32 point[3], BvhAabb *box);
void BvhAabb_merge(const BvhAabb *other, BvhAabb *dest);
bool BvhAabb_intersect_ray(const BvhRay *ray, const BvhAabb *box, f32 *t_near_out);

BvhTree BvhTree_0(void);
BvhTree BvhTree_1(u32 primitive_count);
#define BvhTree(...) CONSTRUCTOR_DISPATCH(BvhTree, __VA_ARGS__)
void BvhTree_destroy(BvhTree *tree);
bool BvhTree_build(const BvhPrimitive *prims, u32 count, BvhTree *tree);
bool BvhTree_intersect_ray(const BvhTree *tree, const BvhRay *ray, BvhRayHit *hit_out);

#ifdef __cplusplus
}
#endif

#endif // VEXSPOKE_ALGO_BVH_H
