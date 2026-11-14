#include "vex.h"
#include "algo/kd_tree.h"

#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: KdTree
 * ============================================================================
 * Spatial acceleration structure for 3D point clouds: recursively partitions
 * points by alternating X/Y/Z split planes so nearest-neighbor and radius
 * queries run in O(log N) expected time instead of a linear scan. The tree is
 * built once from a caller-owned point array (arena-allocated nodes, balanced
 * via quickselect on the median), then queried read-only; KdTree_free walks
 * the arena nodes and releases the tree struct. Lives at R2 as a pure leaf
 * math/geometry primitive with zero dependencies above it.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: KdTree (algo/kd_tree.c)
 * ============================================================================
 * Recursively partitions 3D space by alternating split axes (X, Y, Z)
 * enabling O(log N) nearest-neighbor and spherical radius queries.
 *
 * STRUCT FIELDS (Mirroring algo/kd_tree.h):
 * ----------------------------------------------------------------------------
 *   KdPoint {
 *     f32 coord[3];    // [0] = x/horizontal, [1] = y/vertical, [2] = z/depth
 *     u64 payload;  // caller payload carried by the point
 *   }
 *   KdNode {
 *     KdPoint point;         // split point stored at this node
 *     u8 axis;          // 0 = X, 1 = Y, 2 = Z split axis
 *     struct KdNode *left;   // subtree on the near side of the split plane
 *     struct KdNode *right;  // subtree on the far side of the split plane
 *   }
 *   KdTree {
 *     KdNode *root;  // root of the balanced tree (nullptr when empty)
 *     bytes count;  // number of points indexed
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Core Functions: (.h)
 *   - KdTree_build(points, count)
 *   - KdTree_free(tree)
 *   - KdTree_nearest(tree, queryCoord, outNearest, outDistSq)
 *   - KdTree_queryRadius(tree, queryCoord, radius, outPayloads, maxCount)
 * Private Core Functions: (.c static)
 *   - swapPoints(a, b.sh)
 *   - partitionAxis(points, left, right, axis)
 *   - quickSelectAxis(points, left, right, k, axis)
 *   - KdTree_buildRecursive(points, left, right, depth)
 *   - KdNode_freeRecursive(node)
 *   - distSq(a, b.sh)
 *   - nearestRecursive(node, query, bestPoint, bestDistSq)
 *   - radiusRecursive(node, query, radiusSq, outPayloads, maxCount, count)
 * ============================================================================
 */

static fn swapPoints(KdPoint *a, KdPoint *b) {
    KdPoint tmp = *a;
    *a = *b;
    *b = tmp;
}

static bytes partitionAxis(KdPoint *points, bytes left, bytes right, u8 axis) {
    f32 pivot = points[right].coord[axis];
    bytes i = left;
    for (bytes j = left; j < right; j++) {
        if (points[j].coord[axis] < pivot) {
            swapPoints(&points[i], &points[j]);
            i++;
        }
    }
    swapPoints(&points[i], &points[right]);
    return i;
}

static fn quickSelectAxis(KdPoint *points, bytes left, bytes right, bytes k, u8 axis) {
    while (left < right) {
        bytes pivotIdx = partitionAxis(points, left, right, axis);
        if (pivotIdx == k) {
            return;
        } else if (k < pivotIdx) {
            if (pivotIdx == 0) break;
            right = pivotIdx - 1;
        } else {
            left = pivotIdx + 1;
        }
    }
}

static KdNode *KdTree_buildRecursive(KdPoint *points, bytes left, bytes right, u8 depth) {
    if (left > right) return nullptr;

    u8 axis = depth % 3;
    bytes mid = left + (right - left) / 2;

    quickSelectAxis(points, left, right, mid, axis);

    KdNode *node = (KdNode*) Memory_alloc(TYPE_BYTE_ARRAY, sizeof(KdNode));
    if (node == nullptr) return nullptr;

    (*node).point = points[mid];
    (*node).axis = axis;
    (*node).left = (mid > left) ? KdTree_buildRecursive(points, left, mid - 1, depth + 1) : nullptr;
    (*node).right = (mid < right) ? KdTree_buildRecursive(points, mid + 1, right, depth + 1) : nullptr;

    return node;
}

KdTree *KdTree_build(KdPoint *points, bytes count) {
    if (points == nullptr || count == 0) return nullptr;

    KdTree *tree = (KdTree*) Memory_alloc(TYPE_BYTE_ARRAY, sizeof(KdTree));
    if (tree == nullptr) return nullptr;

    (*tree).count = count;
    (*tree).root = KdTree_buildRecursive(points, 0, count - 1, 0);
    return tree;
}

static fn KdNode_freeRecursive(KdNode *node) {
    if (node == nullptr) return;
    if ((*node).left != nullptr) KdNode_freeRecursive((*node).left);
    if ((*node).right != nullptr) KdNode_freeRecursive((*node).right);
    Memory_free(node);
}

fn KdTree_free(KdTree *tree) {
    if (tree == nullptr) return;
    if ((*tree).root != nullptr) {
        KdNode_freeRecursive((*tree).root);
        (*tree).root = nullptr;
    }
    Memory_free(tree);
}

static inline f32 distSq(const f32 a[3], const f32 b[3]) {
    f32 dx = a[0] - b[0];
    f32 dy = a[1] - b[1];
    f32 dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
}

static fn nearestRecursive(const KdNode *node,
                             const f32   query[3],
                             KdPoint      *bestPoint,
                             f32        *bestDistSq) {
    if (node == nullptr) return;

    KdPoint point = (*node).point;
    f32 d = distSq(query, point.coord);
    if (d < *bestDistSq) {
        *bestDistSq = d;
        *bestPoint = point;
    }

    u8 axis = (*node).axis;
    f32 planeDelta = query[axis] - point.coord[axis];

    const KdNode *nearChild = (planeDelta <= 0.0f) ? (*node).left : (*node).right;
    const KdNode *farChild  = (planeDelta <= 0.0f) ? (*node).right : (*node).left;

    // Search near side first
    nearestRecursive(nearChild, query, bestPoint, bestDistSq);

    // Check if far side could possibly contain a closer point
    if (planeDelta * planeDelta < *bestDistSq) {
        nearestRecursive(farChild, query, bestPoint, bestDistSq);
    }
}

bool KdTree_nearest(const KdTree *tree, const f32 queryCoord[3], KdPoint *outNearest, f32 *outDistSq) {
    if (tree == nullptr || (*tree).root == nullptr || outNearest == nullptr) {
        return false;
    }
    f32 bestD = 1e30f;
    const KdNode *root = (*tree).root;
    KdPoint bestP = (*root).point;
    nearestRecursive(root, queryCoord, &bestP, &bestD);

    *outNearest = bestP;
    if (outDistSq != nullptr) {
        *outDistSq = bestD;
    }
    return true;
}

static bytes radiusRecursive(const KdNode *node,
                              const f32   query[3],
                              f32         radiusSq,
                              u64     *outPayloads,
                              bytes        maxCount,
                              bytes        count) {
    if (node == nullptr || count >= maxCount) return count;

    KdPoint point = (*node).point;
    f32 d = distSq(query, point.coord);
    if (d <= radiusSq) {
        outPayloads[count++] = point.payload;
        if (count >= maxCount) return count;
    }

    u8 axis = (*node).axis;
    f32 planeDelta = query[axis] - point.coord[axis];

    const KdNode *nearChild = (planeDelta <= 0.0f) ? (*node).left : (*node).right;
    const KdNode *farChild  = (planeDelta <= 0.0f) ? (*node).right : (*node).left;

    count = radiusRecursive(nearChild, query, radiusSq, outPayloads, maxCount, count);

    if (count < maxCount && (planeDelta * planeDelta <= radiusSq)) {
        count = radiusRecursive(farChild, query, radiusSq, outPayloads, maxCount, count);
    }

    return count;
}

bytes KdTree_queryRadius(const KdTree *tree, const f32 queryCoord[3], f32 radius, u64 *outPayloads, bytes maxCount) {
    if (tree == nullptr || (*tree).root == nullptr || outPayloads == nullptr || maxCount == 0 || radius < 0.0f) {
        return 0;
    }
    return radiusRecursive((*tree).root, queryCoord, radius * radius, outPayloads, maxCount, 0);
}
