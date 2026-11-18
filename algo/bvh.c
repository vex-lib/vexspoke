#include "vex.h"
#include "algo/bvh.h"

#include <float.h>
#include <stdlib.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Bvh
 * ============================================================================
 * Bounding Volume Hierarchy (BVH) for accelerated 3D ray tracing and spatial
 * queries. The tree stores AABB nodes over primitive index ranges; rays are
 * tested against node boxes in front-to-back order so early misses prune
 * whole subtrees. BvhTree(...) allocates node and primitive-index arrays;
 * it borrows the primitive array while built. There is no global state, so
 * independent trees can be built and queried on separate threads. Cold build
 * paths validate primitive counts; hot intersect paths guard only null
 * pointers per the Cold-Strict, Hot-Minimal Validation Law.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Bvh (algo/bvh.c)
 * ============================================================================
 * Bounding Volume Hierarchy (BVH) for accelerated 3D ray tracing and spatial queries.
 *
 * STRUCT FIELDS:
 *   - BvhAabb: min_point[3], max_point[3]
 *   - BvhRay: origin[3], direction[3], t_min, t_max
 *   - BvhTree: nodes, primitive_indices, primitives, counts
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * AABB Utilities:
 *   - BvhAabb_empty()
 *   - BvhAabb_from_points(min_pt, max_pt)
 *   - BvhAabb_expand_point(point, box)
 *   - BvhAabb_merge(other, dest)
 *   - BvhAabb_intersect_ray(ray, box, t_near_out)
 * Tree Operations:
 *   - BvhTree_0()
 *   - BvhTree_1(primitive_count)
 *   - BvhTree_destroy(tree)
 *   - BvhTree_build(prims, count, tree)
 *   - BvhTree_intersect_ray(tree, ray, hit_out)
 * ============================================================================
 */


;;GETTER
static inline f32 *BvhAabb_max_pointMut(BvhAabb *s) { return (*s).max_point; }
;;GETTER
static inline const f32 *BvhAabb_max_point(const BvhAabb *s) { return (*s).max_point; }
;;GETTER
static inline f32 *BvhAabb_min_pointMut(BvhAabb *s) { return (*s).min_point; }
;;GETTER
static inline const f32 *BvhAabb_min_point(const BvhAabb *s) { return (*s).min_point; }

;;GETTER
static inline bool BvhRayHit_hit(const BvhRayHit *s) { return (*s).hit; }
;;SETTER
static inline fn BvhRayHit_setHit(BvhRayHit *s, bool v) { (*s).hit = v; }
;;GETTER
static inline f32 *BvhRayHit_normalMut(BvhRayHit *s) { return (*s).normal; }
;;SETTER
static inline fn BvhRayHit_setPrimitive_id(BvhRayHit *s, u32 v) { (*s).primitive_id = v; }
;;GETTER
static inline f32 BvhRayHit_t(const BvhRayHit *s) { return (*s).t; }
;;SETTER
static inline fn BvhRayHit_setT(BvhRayHit *s, f32 v) { (*s).t = v; }

;;GETTER
static inline BvhAabb BvhNode_bounds(const BvhNode *s) { return (*s).bounds; }
;;GETTER
static inline BvhAabb *BvhNode_boundsMut(BvhNode *s) { return &(*s).bounds; }
;;SETTER
static inline fn BvhNode_setBounds(BvhNode *s, BvhAabb v) { (*s).bounds = v; }
;;GETTER
static inline bool BvhNode_is_leaf(const BvhNode *s) { return (*s).is_leaf; }
;;SETTER
static inline fn BvhNode_setIs_leaf(BvhNode *s, bool v) { (*s).is_leaf = v; }
;;GETTER
static inline i32 BvhNode_left_child(const BvhNode *s) { return (*s).left_child; }
;;SETTER
static inline fn BvhNode_setLeft_child(BvhNode *s, i32 v) { (*s).left_child = v; }
;;GETTER
static inline u32 BvhNode_primitive_count(const BvhNode *s) { return (*s).primitive_count; }
;;SETTER
static inline fn BvhNode_setPrimitive_count(BvhNode *s, u32 v) { (*s).primitive_count = v; }
;;GETTER
static inline u32 BvhNode_primitive_offset(const BvhNode *s) { return (*s).primitive_offset; }
;;SETTER
static inline fn BvhNode_setPrimitive_offset(BvhNode *s, u32 v) { (*s).primitive_offset = v; }
;;GETTER
static inline i32 BvhNode_right_child(const BvhNode *s) { return (*s).right_child; }
;;SETTER
static inline fn BvhNode_setRight_child(BvhNode *s, i32 v) { (*s).right_child = v; }

;;GETTER
static inline BvhAabb BvhPrimitive_bounds(const BvhPrimitive *s) { return (*s).bounds; }
;;GETTER
static inline const f32 *BvhPrimitive_centroid(const BvhPrimitive *s) { return (*s).centroid; }
;;GETTER
static inline u32 BvhPrimitive_id(const BvhPrimitive *s) { return (*s).id; }

;;GETTER
static inline const f32 *BvhRay_direction(const BvhRay *s) { return (*s).direction; }
;;GETTER
static inline const f32 *BvhRay_origin(const BvhRay *s) { return (*s).origin; }
;;GETTER
static inline f32 BvhRay_t_max(const BvhRay *s) { return (*s).t_max; }
;;GETTER
static inline f32 BvhRay_t_min(const BvhRay *s) { return (*s).t_min; }

;;SETTER
static inline fn BvhTree_setNode_capacity(BvhTree *s, u32 v) { (*s).node_capacity = v; }
;;GETTER
static inline u32 BvhTree_node_count(const BvhTree *s) { return (*s).node_count; }
;;GETTER
static inline u32 *BvhTree_node_countMut(BvhTree *s) { return &(*s).node_count; }
;;SETTER
static inline fn BvhTree_setNode_count(BvhTree *s, u32 v) { (*s).node_count = v; }
;;GETTER
static inline BvhNode * BvhTree_nodes(const BvhTree *s) { return (*s).nodes; }
;;SETTER
static inline fn BvhTree_setNodes(BvhTree *s, BvhNode * v) { (*s).nodes = v; }
;;GETTER
static inline u32 BvhTree_primitive_count(const BvhTree *s) { return (*s).primitive_count; }
;;SETTER
static inline fn BvhTree_setPrimitive_count(BvhTree *s, u32 v) { (*s).primitive_count = v; }
;;GETTER
static inline u32 * BvhTree_primitive_indices(const BvhTree *s) { return (*s).primitive_indices; }
;;SETTER
static inline fn BvhTree_setPrimitive_indices(BvhTree *s, u32 * v) { (*s).primitive_indices = v; }
;;GETTER
static inline const BvhPrimitive * BvhTree_primitives(const BvhTree *s) { return (*s).primitives; }
;;SETTER
static inline fn BvhTree_setPrimitives(BvhTree *s, const BvhPrimitive * v) { (*s).primitives = v; }

;;INTENTION("Accelerated spatial ray tracing and AABB bounding volume queries")

static inline f32 fminf_local(f32 a, f32 b) {
    return (a < b) ? a : b;
}

static inline f32 fmaxf_local(f32 a, f32 b) {
    return (a > b) ? a : b;
}

BvhAabb BvhAabb_empty() {
    BvhAabb box;
    box.min_point[0] = FLT_MAX;
    box.min_point[1] = FLT_MAX;
    box.min_point[2] = FLT_MAX;
    box.max_point[0] = -FLT_MAX;
    box.max_point[1] = -FLT_MAX;
    box.max_point[2] = -FLT_MAX;
    return box;
}

BvhAabb BvhAabb_from_points(const f32 min_pt[3], const f32 max_pt[3]) {
    BvhAabb box;
    box.min_point[0] = min_pt[0];
    box.min_point[1] = min_pt[1];
    box.min_point[2] = min_pt[2];
    box.max_point[0] = max_pt[0];
    box.max_point[1] = max_pt[1];
    box.max_point[2] = max_pt[2];
    return box;
}

fn BvhAabb_expand_point(const f32 point[3], BvhAabb *box) {
    if (!point || !box) {
        return;
    }
    BvhAabb_min_pointMut(box)[0] = fminf_local(BvhAabb_min_pointMut(box)[0], point[0]);
    BvhAabb_min_pointMut(box)[1] = fminf_local(BvhAabb_min_pointMut(box)[1], point[1]);
    BvhAabb_min_pointMut(box)[2] = fminf_local(BvhAabb_min_pointMut(box)[2], point[2]);
    BvhAabb_max_pointMut(box)[0] = fmaxf_local(BvhAabb_max_pointMut(box)[0], point[0]);
    BvhAabb_max_pointMut(box)[1] = fmaxf_local(BvhAabb_max_pointMut(box)[1], point[1]);
    BvhAabb_max_pointMut(box)[2] = fmaxf_local(BvhAabb_max_pointMut(box)[2], point[2]);
}

fn BvhAabb_merge(const BvhAabb *other, BvhAabb *dest) {
    if (!other || !dest) {
        return;
    }
    BvhAabb_min_pointMut(dest)[0] = fminf_local(BvhAabb_min_point(dest)[0], BvhAabb_min_point(other)[0]);
    BvhAabb_min_pointMut(dest)[1] = fminf_local(BvhAabb_min_point(dest)[1], BvhAabb_min_point(other)[1]);
    BvhAabb_min_pointMut(dest)[2] = fminf_local(BvhAabb_min_point(dest)[2], BvhAabb_min_point(other)[2]);
    BvhAabb_max_pointMut(dest)[0] = fmaxf_local(BvhAabb_max_point(dest)[0], BvhAabb_max_point(other)[0]);
    BvhAabb_max_pointMut(dest)[1] = fmaxf_local(BvhAabb_max_point(dest)[1], BvhAabb_max_point(other)[1]);
    BvhAabb_max_pointMut(dest)[2] = fmaxf_local(BvhAabb_max_point(dest)[2], BvhAabb_max_point(other)[2]);
}

bool BvhAabb_intersect_ray(const BvhRay *ray, const BvhAabb *box, f32 *t_near_out) {
    if (!ray || !box) {
        return false;
    }

    f32 t_min = BvhRay_t_min(ray);
    f32 t_max = BvhRay_t_max(ray);

    for (i32 i = 0; i < 3; i++) {
        f32 inv_d = (BvhRay_direction(ray)[i] != 0.0f) ? (1.0f / BvhRay_direction(ray)[i]) : 1e30f;
        f32 t0 = (BvhAabb_min_point(box)[i] - BvhRay_origin(ray)[i]) * inv_d;
        f32 t1 = (BvhAabb_max_point(box)[i] - BvhRay_origin(ray)[i]) * inv_d;
        if (inv_d < 0.0f) {
            f32 temp = t0;
            t0 = t1;
            t1 = temp;
        }
        t_min = (t0 > t_min) ? t0 : t_min;
        t_max = (t1 < t_max) ? t1 : t_max;
        if (t_max < t_min) {
            return false;
        }
    }

    if (t_near_out) {
        *t_near_out = t_min;
    }
    return true;
}

BvhTree BvhTree_0() {
    BvhTree tree;
    tree.nodes = nullptr;
    tree.primitive_indices = nullptr;
    tree.primitives = nullptr;
    tree.node_count = 0u;
    tree.node_capacity = 0u;
    tree.primitive_count = 0u;
    return tree;
}

BvhTree BvhTree_1(u32 primitive_count) {
    BvhTree tree = BvhTree_0();
    if (primitive_count == 0u)
        return tree;
    if (primitive_count > (UINT32_MAX - 1u) / 2u)
        return tree;

    u32 max_nodes = (2u * primitive_count) + 1u;
    tree.nodes = (BvhNode*) malloc((bytes) max_nodes * sizeof(BvhNode));
    tree.primitive_indices = (u32*) malloc((bytes) primitive_count * sizeof(u32));
    if (!tree.nodes || !tree.primitive_indices) {
        free(tree.nodes);
        free(tree.primitive_indices);
        return BvhTree_0();
    }
    tree.node_capacity = max_nodes;
    tree.primitive_count = primitive_count;
    for (u32 i = 0; i < primitive_count; i++)
        tree.primitive_indices[i] = i;
    return tree;
}

fn BvhTree_destroy(BvhTree *tree) {
    if (!tree) {
        return;
    }
    if (BvhTree_nodes(tree)) {
        free(BvhTree_nodes(tree));
        BvhTree_setNodes(tree, nullptr);
    }
    if (BvhTree_primitive_indices(tree)) {
        free(BvhTree_primitive_indices(tree));
        BvhTree_setPrimitive_indices(tree, nullptr);
    }
    BvhTree_setNode_count(tree, 0);
    BvhTree_setNode_capacity(tree, 0);
    BvhTree_setPrimitive_count(tree, 0);
    BvhTree_setPrimitives(tree, nullptr);
}

static i32 bvh_build_recursive(
    BvhTree *tree,
    u32 offset,
    u32 count,
    u32 max_leaf_prims
) {
    if (count == 0) {
        return -1;
    }

    u32 node_idx = (*BvhTree_node_countMut(tree))++;
    BvhNode *node = &BvhTree_nodes(tree)[node_idx];
    BvhNode_setBounds(node, BvhAabb_empty());
    BvhNode_setLeft_child(node, -1);
    BvhNode_setRight_child(node, -1);
    BvhNode_setPrimitive_offset(node, offset);
    BvhNode_setPrimitive_count(node, count);
    BvhNode_setIs_leaf(node, false);

    BvhAabb centroid_bounds = BvhAabb_empty();
    for (u32 i = 0; i < count; i++) {
        u32 prim_id = BvhTree_primitive_indices(tree)[offset + i];
        const BvhPrimitive *prim = &BvhTree_primitives(tree)[prim_id];
        BvhAabb prim_bounds = BvhPrimitive_bounds(prim);
        BvhAabb_merge(&prim_bounds, BvhNode_boundsMut(node));
        BvhAabb_expand_point(BvhPrimitive_centroid(prim), &centroid_bounds);
    }

    if (count <= max_leaf_prims) {
        BvhNode_setIs_leaf(node, true);
        return (i32) node_idx;
    }

    f32 extent[3];
    extent[0] = centroid_bounds.max_point[0] - centroid_bounds.min_point[0];
    extent[1] = centroid_bounds.max_point[1] - centroid_bounds.min_point[1];
    extent[2] = centroid_bounds.max_point[2] - centroid_bounds.min_point[2];

    i32 axis = 0;
    if (extent[1] > extent[0]) {
        axis = 1;
    }
    if (extent[2] > extent[axis]) {
        axis = 2;
    }

    if (extent[axis] <= 1e-6f) {
        BvhNode_setIs_leaf(node, true);
        return (i32) node_idx;
    }

    f32 split_pos = 0.5f * (centroid_bounds.min_point[axis] + centroid_bounds.max_point[axis]);

    // Partition primitives in-place
    i32 left = (i32) offset;
    i32 right = (i32) (offset + count - 1);

    while (left <= right) {
        u32 prim_id = BvhTree_primitive_indices(tree)[left];
        if (BvhTree_primitives(tree)[prim_id].centroid[axis] < split_pos) {
            left++;
        } else {
            u32 temp = BvhTree_primitive_indices(tree)[left];
            BvhTree_primitive_indices(tree)[left] = BvhTree_primitive_indices(tree)[right];
            BvhTree_primitive_indices(tree)[right] = temp;
            right--;
        }
    }

    u32 left_count = (u32) (left - (i32) offset);
    if (left_count == 0 || left_count == count) {
        left_count = count / 2;
    }

    i32 left_child = bvh_build_recursive(tree, offset, left_count, max_leaf_prims);
    i32 right_child = bvh_build_recursive(tree, offset + left_count, count - left_count, max_leaf_prims);

    // Re-acquire node pointer in case memory relocated (though capacity was pre-allocated)
    node = &BvhTree_nodes(tree)[node_idx];
    BvhNode_setLeft_child(node, left_child);
    BvhNode_setRight_child(node, right_child);
    BvhNode_setIs_leaf(node, false);

    return (i32) node_idx;
}

bool BvhTree_build(const BvhPrimitive *prims, u32 count, BvhTree *tree) {
    if (!tree || (count > 0u && !prims)) {
        return false;
    }
    if (BvhTree_primitive_count(tree) != count
        || (count > 0u && (!BvhTree_nodes(tree) || !BvhTree_primitive_indices(tree)))) {
        return false;
    }
    BvhTree_setNode_count(tree, 0u);
    BvhTree_setPrimitives(tree, nullptr);
    if (count == 0) {
        return true;
    }

    BvhTree_setPrimitives(tree, prims);
    for (u32 i = 0; i < count; i++)
        BvhTree_primitive_indices(tree)[i] = i;
    bvh_build_recursive(tree, 0, count, 2);
    return true;
}

bool BvhTree_intersect_ray(const BvhTree *tree, const BvhRay *ray, BvhRayHit *hit_out) {
    if (!tree || !ray || !hit_out || BvhTree_node_count(tree) == 0) {
        if (hit_out) {
            BvhRayHit_setHit(hit_out, false);
        }
        return false;
    }

    BvhRayHit_setHit(hit_out, false);
    BvhRayHit_setT(hit_out, BvhRay_t_max(ray));
    BvhRayHit_setPrimitive_id(hit_out, 0);
    BvhRayHit_normalMut(hit_out)[0] = 0.0f;
    BvhRayHit_normalMut(hit_out)[1] = 0.0f;
    BvhRayHit_normalMut(hit_out)[2] = 1.0f;

    i32 stack[64];
    i32 stack_ptr = 0;
    stack[stack_ptr++] = 0;

    while (stack_ptr > 0) {
        i32 node_idx = stack[--stack_ptr];
        const BvhNode *node = &BvhTree_nodes(tree)[node_idx];

        f32 t_box = 0.0f;
        BvhRay curr_ray = *ray;
        curr_ray.t_max = BvhRayHit_t(hit_out);

        BvhAabb node_bounds = BvhNode_bounds(node);
        if (!BvhAabb_intersect_ray(&curr_ray, &node_bounds, &t_box)) {
            continue;
        }

        if (BvhNode_is_leaf(node)) {
            for (u32 i = 0; i < BvhNode_primitive_count(node); i++) {
                u32 prim_idx = BvhTree_primitive_indices(tree)[BvhNode_primitive_offset(node) + i];
                const BvhPrimitive *prim = &BvhTree_primitives(tree)[prim_idx];

                f32 t_prim = 0.0f;
                BvhAabb prim_bounds = BvhPrimitive_bounds(prim);
                if (BvhAabb_intersect_ray(&curr_ray, &prim_bounds, &t_prim)) {
                    if (t_prim < BvhRayHit_t(hit_out)) {
                        BvhRayHit_setHit(hit_out, true);
                        BvhRayHit_setT(hit_out, t_prim);
                        BvhRayHit_setPrimitive_id(hit_out, BvhPrimitive_id(prim));
                    }
                }
            }
        } else {
            if (BvhNode_left_child(node) >= 0 && stack_ptr < 64) {
                stack[stack_ptr++] = BvhNode_left_child(node);
            }
            if (BvhNode_right_child(node) >= 0 && stack_ptr < 64) {
                stack[stack_ptr++] = BvhNode_right_child(node);
            }
        }
    }

    return BvhRayHit_hit(hit_out);
}
