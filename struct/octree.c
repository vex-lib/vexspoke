#include "struct/octree.h"

#include <string.h>
#include <math.h>
#include "nio/mem.h"
#include "oop/vexspoke_type.h"
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Octree
 * ============================================================================
 * 3D spatial partitioning octree. Partitions 3D volumes into 8 octants to
 * accelerate range queries, frustum/AABB intersections, and radius queries.
 * Arena-allocated: the Octree owns a root OctreeNode; leaves grow their item
 * arrays by doubling and subdivide into eight children once full. Insert and
 * query recurse through the octant tree with bounded depth, keeping point
 * lookups O(log N) on average.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Octree (struct/octree.c)
 * ============================================================================
 * 3D Spatial Partitioning Octree.
 *
 * Partitions 3D volumes into 8 octants to accelerate range queries,
 * frustum/AABB intersections, and radius queries.
 *
 * STRUCT FIELDS (Mirroring struct/octree.h):
 * ----------------------------------------------------------------------------
 *   OctreePoint {
 *     f32 x;
 *     f32 y;
 *     f32 z;
 *   }
 *   OctreeAABB {
 *     f32 minX; f32 minY; f32 minZ;
 *     f32 maxX; f32 maxY; f32 maxZ;
 *   }
 *   OctreeItem {
 *     OctreePoint point;
 *     u64    payload;
 *   }
 *   OctreeNode {
 *     OctreeAABB         bounds;
 *     OctreeItem        *items;
 *     u32           itemCount;
 *     u32           itemCapacity;
 *     bool               isLeaf;
 *     struct OctreeNode *children[8];
 *   }
 *   Octree {
 *     OctreeNode *root;
 *     u32    maxDepth;
 *     u32    maxItemsPerNode;
 *     bytes      totalItems;
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h)
 *   - Octree(bounds, maxDepth, maxItemsPerNode)
 *
 * Public Core Functions: (.h)
 *   - Octree_free(self)
 *   - Octree_insert(self, point, payload)
 *   - Octree_queryRange(self, range, outPayloads, maxCount)
 *   - Octree_querySphere(self, center, radius, outPayloads, maxCount)
 *   - Octree_count(self)
 *   - Octree_clear(self)
 *   - OctreeAABB_containsPoint(box, p)
 *   - OctreeAABB_intersects(a, b.sh)
 *   - OctreeAABB_intersectsSphere(box, center, radius)
 *
 * Private Core Functions: (.c static)
 *   - OctreeNode_2(bounds, capacity)
 *   - OctreeNode_free(node)
 *   - OctreeNode_subdivide(node, childCapacity)
 *   - OctreeNode_insert(node, item, depth, maxDepth, maxItems)
 *   - OctreeNode_queryRange(node, range, outPayloads, maxCount, count)
 *   - OctreeNode_querySphere(node, center, radius, outPayloads, maxCount, count)
 * ============================================================================
 */

bool OctreeAABB_containsPoint(OctreeAABB box, OctreePoint p) {
    return (p.x >= box.minX && p.x <= box.maxX &&
            p.y >= box.minY && p.y <= box.maxY &&
            p.z >= box.minZ && p.z <= box.maxZ);
}

bool OctreeAABB_intersects(OctreeAABB a, OctreeAABB b) {
    return (a.minX <= b.maxX && a.maxX >= b.minX &&
            a.minY <= b.maxY && a.maxY >= b.minY &&
            a.minZ <= b.maxZ && a.maxZ >= b.minZ);
}

bool OctreeAABB_intersectsSphere(OctreeAABB box, OctreePoint center, f32 radius) {
    f32 closestX = center.x;
    if (closestX < box.minX) closestX = box.minX;
    else if (closestX > box.maxX) closestX = box.maxX;

    f32 closestY = center.y;
    if (closestY < box.minY) closestY = box.minY;
    else if (closestY > box.maxY) closestY = box.maxY;

    f32 closestZ = center.z;
    if (closestZ < box.minZ) closestZ = box.minZ;
    else if (closestZ > box.maxZ) closestZ = box.maxZ;

    f32 dx = center.x - closestX;
    f32 dy = center.y - closestY;
    f32 dz = center.z - closestZ;
    return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
}

static OctreeNode *OctreeNode_2(OctreeAABB bounds, u32 capacity) {
    OctreeNode *node = (OctreeNode*) Memory_alloc(TYPE_OCTREE, sizeof(OctreeNode));
    if (node == nullptr) {
        return nullptr;
    }
    (*node).bounds = bounds;
    (*node).itemCount = 0;
    (*node).itemCapacity = capacity;
    (*node).isLeaf = true;
    for (int i = 0; i < 8; i++) {
        (*node).children[i] = nullptr;
    }
    (*node).items = (OctreeItem*) Memory_alloc(TYPE_BYTE_ARRAY, (bytes) capacity * sizeof(OctreeItem));
    if ((*node).items == nullptr) {
        Memory_free(node);
        return nullptr;
    }
    return node;
}

static void OctreeNode_free(OctreeNode *node) {
    if (node == nullptr) {
        return;
    }
    if (!(*node).isLeaf) {
        for (int i = 0; i < 8; i++) {
            if ((*node).children[i] != nullptr) {
                OctreeNode_free((*node).children[i]);
                (*node).children[i] = nullptr;
            }
        }
    }
    if ((*node).items != nullptr) {
        Memory_free((*node).items);
        (*node).items = nullptr;
    }
    Memory_free(node);
}

static void OctreeNode_subdivide(OctreeNode *node, u32 childCapacity) {
    OctreeAABB bounds = (*node).bounds;
    f32 midX = (bounds.minX + bounds.maxX) * 0.5f;
    f32 midY = (bounds.minY + bounds.maxY) * 0.5f;
    f32 midZ = (bounds.minZ + bounds.maxZ) * 0.5f;

    OctreeAABB subBounds[8];
    // 0: ---
    subBounds[0] = (OctreeAABB) { bounds.minX, bounds.minY, bounds.minZ, midX, midY, midZ };
    // 1: +--
    subBounds[1] = (OctreeAABB) { midX, bounds.minY, bounds.minZ, bounds.maxX, midY, midZ };
    // 2: -+-
    subBounds[2] = (OctreeAABB) { bounds.minX, midY, bounds.minZ, midX, bounds.maxY, midZ };
    // 3: ++-
    subBounds[3] = (OctreeAABB) { midX, midY, bounds.minZ, bounds.maxX, bounds.maxY, midZ };
    // 4: --+
    subBounds[4] = (OctreeAABB) { bounds.minX, bounds.minY, midZ, midX, midY, bounds.maxZ };
    // 5: +-+
    subBounds[5] = (OctreeAABB) { midX, bounds.minY, midZ, bounds.maxX, midY, bounds.maxZ };
    // 6: -++
    subBounds[6] = (OctreeAABB) { bounds.minX, midY, midZ, midX, bounds.maxY, bounds.maxZ };
    // 7: +++
    subBounds[7] = (OctreeAABB) { midX, midY, midZ, bounds.maxX, bounds.maxY, bounds.maxZ };

    for (int i = 0; i < 8; i++) {
        (*node).children[i] = OctreeNode_2(subBounds[i], childCapacity);
    }
    (*node).isLeaf = false;
}

static bool OctreeNode_insert(Octree *owner, OctreeNode *node, OctreeItem item, u32 depth, u32 maxDepth, u32 maxItems) {
    if (!OctreeAABB_containsPoint((*node).bounds, item.point)) {
        return false;
    }

    if ((*node).isLeaf) {
        if ((*node).itemCount < maxItems || depth >= maxDepth) {
            // Can add directly to this leaf
            if ((*node).itemCount >= (*node).itemCapacity) {
                // Grow leaf capacity
                u32 newCap = (*node).itemCapacity * 2;
                if (newCap == 0) newCap = 8;
                OctreeItem *newItems = (OctreeItem*) Memory_alloc(TYPE_BYTE_ARRAY, (bytes) newCap * sizeof(OctreeItem));
                if (newItems == nullptr) {
                    // A refused leaf growth is reported at the owner level.
                    (void) Struct_reportExhaustion(&(*owner).exhaustionCount, &(*owner).exhaustionReported,
                                                   "octree", (bytes) newCap * sizeof(OctreeItem),
                                                   (bytes) (*node).itemCapacity * sizeof(OctreeItem));
                    return false;
                }
                if ((*node).items != nullptr) {
                    memcpy(newItems, (*node).items, (bytes) (*node).itemCount * sizeof(OctreeItem));
                    Memory_free((*node).items);
                }
                (*node).items = newItems;
                (*node).itemCapacity = newCap;
            }
            (*node).items[(*node).itemCount] = item;
            (*node).itemCount++;
            return true;
        }

        // Subdivide leaf into 8 octants
        OctreeNode_subdivide(node, maxItems);

        // Re-insert existing items into children
        for (u32 i = 0; i < (*node).itemCount; i++) {
            OctreeItem existing = (*node).items[i];
            for (int c = 0; c < 8; c++) {
                if (OctreeNode_insert(owner, (*node).children[c], existing, depth + 1, maxDepth, maxItems)) {
                    break;
                }
            }
        }
        (*node).itemCount = 0;
    }

    // Insert new item into matching child
    for (int c = 0; c < 8; c++) {
        if (OctreeNode_insert(owner, (*node).children[c], item, depth + 1, maxDepth, maxItems)) {
            return true;
        }
    }

    return false;
}

Octree *Octree_3(OctreeAABB bounds, u32 maxDepth, u32 maxItemsPerNode) {
    if (maxDepth == 0) maxDepth = OCTREE_DEFAULT_MAX_DEPTH;
    if (maxItemsPerNode == 0) maxItemsPerNode = OCTREE_DEFAULT_NODE_CAPACITY;

    Octree *self = (Octree*) Memory_alloc(TYPE_OCTREE, sizeof(Octree));
    if (self == nullptr) {
        (void) Struct_reportConstructionExhaustion("octree", sizeof(Octree));
        return nullptr;
    }

    (*self).maxDepth = maxDepth;
    (*self).maxItemsPerNode = maxItemsPerNode;
    (*self).totalItems = 0;
    (*self).exhaustionCount = 0;
    (*self).exhaustionReported = false;
    (*self).root = OctreeNode_2(bounds, maxItemsPerNode);
    if ((*self).root == nullptr) {
        (void) Struct_reportConstructionExhaustion("octree", sizeof(OctreeNode));
        Memory_free(self);
        return nullptr;
    }

    return self;
}

void Octree_free(Octree *self) {
    if (self == nullptr) {
        return;
    }
    if ((*self).root != nullptr) {
        OctreeNode_free((*self).root);
        (*self).root = nullptr;
    }
    Memory_free(self);
}

bool Octree_insert(Octree *self, OctreePoint point, u64 payload) {
    if (self == nullptr || (*self).root == nullptr) {
        return false;
    }
    OctreeItem item;
    item.point = point;
    item.payload = payload;

    bool ok = OctreeNode_insert(self, (*self).root, item, 0, (*self).maxDepth, (*self).maxItemsPerNode);
    if (ok) {
        (*self).totalItems++;
    }
    return ok;
}

static bytes OctreeNode_queryRange(const OctreeNode *node, OctreeAABB range, u64 *outPayloads, bytes maxCount, bytes count) {
    if (node == nullptr || count >= maxCount) {
        return count;
    }
    if (!OctreeAABB_intersects((*node).bounds, range)) {
        return count;
    }

    if ((*node).isLeaf) {
        for (u32 i = 0; i < (*node).itemCount && count < maxCount; i++) {
            if (OctreeAABB_containsPoint(range, (*node).items[i].point)) {
                outPayloads[count++] = (*node).items[i].payload;
            }
        }
        return count;
    }

    for (int i = 0; i < 8 && count < maxCount; i++) {
        count = OctreeNode_queryRange((*node).children[i], range, outPayloads, maxCount, count);
    }
    return count;
}

bytes Octree_queryRange(const Octree *self, OctreeAABB range, u64 *outPayloads, bytes maxCount) {
    if (self == nullptr || (*self).root == nullptr || outPayloads == nullptr || maxCount == 0) {
        return 0;
    }
    return OctreeNode_queryRange((*self).root, range, outPayloads, maxCount, 0);
}

static bytes OctreeNode_querySphere(const OctreeNode *node, OctreePoint center, f32 radius, u64 *outPayloads, bytes maxCount, bytes count) {
    if (node == nullptr || count >= maxCount) {
        return count;
    }
    if (!OctreeAABB_intersectsSphere((*node).bounds, center, radius)) {
        return count;
    }

    f32 rSq = radius * radius;
    if ((*node).isLeaf) {
        for (u32 i = 0; i < (*node).itemCount && count < maxCount; i++) {
            f32 dx = (*node).items[i].point.x - center.x;
            f32 dy = (*node).items[i].point.y - center.y;
            f32 dz = (*node).items[i].point.z - center.z;
            if (dx * dx + dy * dy + dz * dz <= rSq) {
                outPayloads[count++] = (*node).items[i].payload;
            }
        }
        return count;
    }

    for (int i = 0; i < 8 && count < maxCount; i++) {
        count = OctreeNode_querySphere((*node).children[i], center, radius, outPayloads, maxCount, count);
    }
    return count;
}

bytes Octree_querySphere(const Octree *self, OctreePoint center, f32 radius, u64 *outPayloads, bytes maxCount) {
    if (self == nullptr || (*self).root == nullptr || outPayloads == nullptr || maxCount == 0) {
        return 0;
    }
    return OctreeNode_querySphere((*self).root, center, radius, outPayloads, maxCount, 0);
}

bytes Octree_count(const Octree *self) {
    if (self == nullptr) return 0;
    return (*self).totalItems;
}

;;TEST
u64 Octree_exhaustionCount(const Octree *self) {
    return self ? Struct_exhaustionCount(&(*self).exhaustionCount) : 0;
}

void Octree_clear(Octree *self) {
    if (self == nullptr || (*self).root == nullptr) return;
    const OctreeNode *root = (*self).root;
    OctreeAABB bounds = (*root).bounds;
    OctreeNode_free((*self).root);
    (*self).root = OctreeNode_2(bounds, (*self).maxItemsPerNode);
    (*self).totalItems = 0;
}
