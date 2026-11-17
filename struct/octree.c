#include "vex.h"
#include "nio/mem.h"

#include "struct/octree.h"
#include "oop/vexspoke_type.h"

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


;;GETTER
static inline OctreeAABB OctreeNode_bounds(const OctreeNode *s) { return (*s).bounds; }
;;GETTER
static inline OctreeAABB *OctreeNode_boundsMut(OctreeNode *s) { return &(*s).bounds; }
;;SETTER
static inline fn OctreeNode_setBounds(OctreeNode *s, OctreeAABB v) { (*s).bounds = v; }
;;GETTER
static inline struct OctreeNode * *OctreeNode_childrenMut(OctreeNode *s) { return (*s).children; }
;;GETTER
static inline const struct OctreeNode * *OctreeNode_children(const OctreeNode *s) { return (*s).children; }
;;GETTER
static inline bool OctreeNode_isLeaf(const OctreeNode *s) { return (*s).isLeaf; }
;;GETTER
static inline bool *OctreeNode_isLeafMut(OctreeNode *s) { return &(*s).isLeaf; }
;;SETTER
static inline fn OctreeNode_setIsLeaf(OctreeNode *s, bool v) { (*s).isLeaf = v; }
;;GETTER
static inline u32 OctreeNode_itemCapacity(const OctreeNode *s) { return (*s).itemCapacity; }
;;GETTER
static inline u32 *OctreeNode_itemCapacityMut(OctreeNode *s) { return &(*s).itemCapacity; }
;;SETTER
static inline fn OctreeNode_setItemCapacity(OctreeNode *s, u32 v) { (*s).itemCapacity = v; }
;;GETTER
static inline u32 OctreeNode_itemCount(const OctreeNode *s) { return (*s).itemCount; }
;;GETTER
static inline u32 *OctreeNode_itemCountMut(OctreeNode *s) { return &(*s).itemCount; }
;;SETTER
static inline fn OctreeNode_setItemCount(OctreeNode *s, u32 v) { (*s).itemCount = v; }
;;GETTER
static inline OctreeItem * OctreeNode_items(const OctreeNode *s) { return (*s).items; }
;;GETTER
static inline OctreeItem * *OctreeNode_itemsMut(OctreeNode *s) { return &(*s).items; }
;;SETTER
static inline fn OctreeNode_setItems(OctreeNode *s, OctreeItem * v) { (*s).items = v; }

;;GETTER
static inline u64 Octree_exhaustionCount(const Octree *s) { return (*s).exhaustionCount; }
;;GETTER
static inline u64 *Octree_exhaustionCountMut(Octree *s) { return &(*s).exhaustionCount; }
;;SETTER
static inline fn Octree_setExhaustionCount(Octree *s, u64 v) { (*s).exhaustionCount = v; }
;;GETTER
static inline bool Octree_exhaustionReported(const Octree *s) { return (*s).exhaustionReported; }
;;GETTER
static inline bool *Octree_exhaustionReportedMut(Octree *s) { return &(*s).exhaustionReported; }
;;SETTER
static inline fn Octree_setExhaustionReported(Octree *s, bool v) { (*s).exhaustionReported = v; }
;;GETTER
static inline u32 Octree_maxDepth(const Octree *s) { return (*s).maxDepth; }
;;GETTER
static inline u32 *Octree_maxDepthMut(Octree *s) { return &(*s).maxDepth; }
;;SETTER
static inline fn Octree_setMaxDepth(Octree *s, u32 v) { (*s).maxDepth = v; }
;;GETTER
static inline u32 Octree_maxItemsPerNode(const Octree *s) { return (*s).maxItemsPerNode; }
;;GETTER
static inline u32 *Octree_maxItemsPerNodeMut(Octree *s) { return &(*s).maxItemsPerNode; }
;;SETTER
static inline fn Octree_setMaxItemsPerNode(Octree *s, u32 v) { (*s).maxItemsPerNode = v; }
;;GETTER
static inline OctreeNode * Octree_root(const Octree *s) { return (*s).root; }
;;GETTER
static inline OctreeNode * *Octree_rootMut(Octree *s) { return &(*s).root; }
;;SETTER
static inline fn Octree_setRoot(Octree *s, OctreeNode * v) { (*s).root = v; }
;;GETTER
static inline bytes Octree_totalItems(const Octree *s) { return (*s).totalItems; }
;;GETTER
static inline bytes *Octree_totalItemsMut(Octree *s) { return &(*s).totalItems; }
;;SETTER
static inline fn Octree_setTotalItems(Octree *s, bytes v) { (*s).totalItems = v; }

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
    OctreeNode_setBounds(node, bounds);
    OctreeNode_setItemCount(node, 0);
    OctreeNode_setItemCapacity(node, capacity);
    OctreeNode_setIsLeaf(node, true);
    for (i32 i = 0; i < 8; i++) {
        OctreeNode_childrenMut(node)[i] = nullptr;
    }
    OctreeNode_setItems(node, (OctreeItem*) Memory_alloc(TYPE_BYTE_ARRAY, (bytes) capacity * sizeof(OctreeItem)));
    if (OctreeNode_items(node) == nullptr) {
        Memory_free(node);
        return nullptr;
    }
    return node;
}

static fn OctreeNode_free(OctreeNode *node) {
    if (node == nullptr) {
        return;
    }
    if (!OctreeNode_isLeaf(node)) {
        for (i32 i = 0; i < 8; i++) {
            if (OctreeNode_childrenMut(node)[i] != nullptr) {
                OctreeNode_free(OctreeNode_childrenMut(node)[i]);
                OctreeNode_childrenMut(node)[i] = nullptr;
            }
        }
    }
    if (OctreeNode_items(node) != nullptr) {
        Memory_free(OctreeNode_items(node));
        OctreeNode_setItems(node, nullptr);
    }
    Memory_free(node);
}

static fn OctreeNode_subdivide(OctreeNode *node, u32 childCapacity) {
    OctreeAABB bounds = OctreeNode_bounds(node);
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

    for (i32 i = 0; i < 8; i++) {
        OctreeNode_childrenMut(node)[i] = OctreeNode_2(subBounds[i], childCapacity);
    }
    OctreeNode_setIsLeaf(node, false);
}

static bool OctreeNode_insert(Octree *owner, OctreeNode *node, OctreeItem item, u32 depth, u32 maxDepth, u32 maxItems) {
    if (!OctreeAABB_containsPoint(OctreeNode_bounds(node), item.point)) {
        return false;
    }

    if (OctreeNode_isLeaf(node)) {
        if (OctreeNode_itemCount(node) < maxItems || depth >= maxDepth) {
            // Can add directly to this leaf
            if (OctreeNode_itemCount(node) >= OctreeNode_itemCapacity(node)) {
                // Grow leaf capacity
                u32 newCap = OctreeNode_itemCapacity(node) * 2;
                if (newCap == 0) newCap = 8;
                OctreeItem *newItems = (OctreeItem*) Memory_alloc(TYPE_BYTE_ARRAY, (bytes) newCap * sizeof(OctreeItem));
                if (newItems == nullptr) {
                    // A refused leaf growth is reported at the owner level.
                    (discard) Struct_reportExhaustion(Octree_exhaustionCountMut(owner), Octree_exhaustionReportedMut(owner),
                                                   "octree", (bytes) newCap * sizeof(OctreeItem),
                                                   (bytes) OctreeNode_itemCapacity(node) * sizeof(OctreeItem));
                    return false;
                }
                if (OctreeNode_items(node) != nullptr) {
                    memcpy(newItems, OctreeNode_items(node), (bytes) OctreeNode_itemCount(node) * sizeof(OctreeItem));
                    Memory_free(OctreeNode_items(node));
                }
                OctreeNode_setItems(node, newItems);
                OctreeNode_setItemCapacity(node, newCap);
            }
            OctreeNode_items(node)[OctreeNode_itemCount(node)] = item;
            OctreeNode_setItemCount(node, OctreeNode_itemCount(node) + 1u);
            return true;
        }

        // Subdivide leaf into 8 octants
        OctreeNode_subdivide(node, maxItems);

        // Re-insert existing items into children
        for (u32 i = 0; i < OctreeNode_itemCount(node); i++) {
            OctreeItem existing = OctreeNode_items(node)[i];
            for (i32 c = 0; c < 8; c++) {
                if (OctreeNode_insert(owner, OctreeNode_childrenMut(node)[c], existing, depth + 1, maxDepth, maxItems)) {
                    break;
                }
            }
        }
        OctreeNode_setItemCount(node, 0);
    }

    // Insert new item into matching child
    for (i32 c = 0; c < 8; c++) {
        if (OctreeNode_insert(owner, OctreeNode_childrenMut(node)[c], item, depth + 1, maxDepth, maxItems)) {
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
        (discard) Struct_reportConstructionExhaustion("octree", sizeof(Octree));
        return nullptr;
    }

    Octree_setMaxDepth(self, maxDepth);
    Octree_setMaxItemsPerNode(self, maxItemsPerNode);
    Octree_setTotalItems(self, 0);
    Octree_setExhaustionCount(self, 0);
    Octree_setExhaustionReported(self, false);
    Octree_setRoot(self, OctreeNode_2(bounds, maxItemsPerNode));
    if (Octree_root(self) == nullptr) {
        (discard) Struct_reportConstructionExhaustion("octree", sizeof(OctreeNode));
        Memory_free(self);
        return nullptr;
    }

    return self;
}

fn Octree_free(Octree *self) {
    if (self == nullptr) {
        return;
    }
    if (Octree_root(self) != nullptr) {
        OctreeNode_free(Octree_root(self));
        Octree_setRoot(self, nullptr);
    }
    Memory_free(self);
}

bool Octree_insert(Octree *self, OctreePoint point, u64 payload) {
    if (self == nullptr || Octree_root(self) == nullptr) {
        return false;
    }
    OctreeItem item;
    item.point = point;
    item.payload = payload;

    bool ok = OctreeNode_insert(self, Octree_root(self), item, 0, Octree_maxDepth(self), Octree_maxItemsPerNode(self));
    if (ok) {
        Octree_setTotalItems(self, Octree_totalItems(self) + 1u);
    }
    return ok;
}

static bytes OctreeNode_queryRange(const OctreeNode *node, OctreeAABB range, u64 *outPayloads, bytes maxCount, bytes count) {
    if (node == nullptr || count >= maxCount) {
        return count;
    }
    if (!OctreeAABB_intersects(OctreeNode_bounds(node), range)) {
        return count;
    }

    if (OctreeNode_isLeaf(node)) {
        for (u32 i = 0; i < OctreeNode_itemCount(node) && count < maxCount; i++) {
            if (OctreeAABB_containsPoint(range, OctreeNode_items(node)[i].point)) {
                outPayloads[count++] = OctreeNode_items(node)[i].payload;
            }
        }
        return count;
    }

    for (i32 i = 0; i < 8 && count < maxCount; i++) {
        count = OctreeNode_queryRange(OctreeNode_childrenMut(node)[i], range, outPayloads, maxCount, count);
    }
    return count;
}

bytes Octree_queryRange(const Octree *self, OctreeAABB range, u64 *outPayloads, bytes maxCount) {
    if (self == nullptr || Octree_root(self) == nullptr || outPayloads == nullptr || maxCount == 0) {
        return 0;
    }
    return OctreeNode_queryRange(Octree_root(self), range, outPayloads, maxCount, 0);
}

static bytes OctreeNode_querySphere(const OctreeNode *node, OctreePoint center, f32 radius, u64 *outPayloads, bytes maxCount, bytes count) {
    if (node == nullptr || count >= maxCount) {
        return count;
    }
    if (!OctreeAABB_intersectsSphere(OctreeNode_bounds(node), center, radius)) {
        return count;
    }

    f32 rSq = radius * radius;
    if (OctreeNode_isLeaf(node)) {
        for (u32 i = 0; i < OctreeNode_itemCount(node) && count < maxCount; i++) {
            f32 dx = OctreeNode_items(node)[i].point.x - center.x;
            f32 dy = OctreeNode_items(node)[i].point.y - center.y;
            f32 dz = OctreeNode_items(node)[i].point.z - center.z;
            if (dx * dx + dy * dy + dz * dz <= rSq) {
                outPayloads[count++] = OctreeNode_items(node)[i].payload;
            }
        }
        return count;
    }

    for (i32 i = 0; i < 8 && count < maxCount; i++) {
        count = OctreeNode_querySphere(OctreeNode_childrenMut(node)[i], center, radius, outPayloads, maxCount, count);
    }
    return count;
}

bytes Octree_querySphere(const Octree *self, OctreePoint center, f32 radius, u64 *outPayloads, bytes maxCount) {
    if (self == nullptr || Octree_root(self) == nullptr || outPayloads == nullptr || maxCount == 0) {
        return 0;
    }
    return OctreeNode_querySphere(Octree_root(self), center, radius, outPayloads, maxCount, 0);
}

bytes Octree_count(const Octree *self) {
    if (self == nullptr) return 0;
    return Octree_totalItems(self);
}

;;TEST
u64 Octree_exhaustionCount(const Octree *self) {
    return self ? Struct_exhaustionCount(Octree_exhaustionCountMut(self)) : 0;
}

fn Octree_clear(Octree *self) {
    if (self == nullptr || Octree_root(self) == nullptr) return;
    const OctreeNode *root = Octree_root(self);
    OctreeAABB bounds = OctreeNode_bounds(root);
    OctreeNode_free(Octree_root(self));
    Octree_setRoot(self, OctreeNode_2(bounds, Octree_maxItemsPerNode(self)));
    Octree_setTotalItems(self, 0);
}
