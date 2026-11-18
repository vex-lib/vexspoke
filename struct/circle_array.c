#include "vex.h"
#include "nio/mem.h"

#include "struct/circle_array.h"
#include "struct/stride.h"
#include "oop/vexspoke_type.h"
#include "struct/exhaustion.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: CircleArray
 * ============================================================================
 * 2D circular matrix evaluated via the Pythagorean Theorem. Encapsulates a
 * square bounding grid of size (2*radius + 1)^2 where only cells whose
 * Euclidean distance from the center satisfies dx^2 + dy^2 <= radius^2 are
 * valid and accessible. Arena-allocated: the CircleArray embeds a Collection
 * header and owns a flat byte data buffer with per-element stride, so cell
 * access is index math into one contiguous block with zero pointer chasing.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: CircleArray (struct/circle_array.c)
 * ============================================================================
 * 2D Circular Matrix evaluated via the Pythagorean Theorem.
 *
 * Encapsulates a square bounding grid of size (2*radius + 1)^2 where only cells
 * whose Euclidean distance from the center satisfies dx^2 + dy^2 <= radius^2
 * are considered valid and accessible.
 *
 * STRUCT FIELDS (Mirroring struct/circle_array.h + struct/collection.h):
 * ----------------------------------------------------------------------------
 *   CircleArray {
 *     Collection collection; // base collection header (typeId, activeCount, etc.)
 *     i32    radius;     // radius in cells
 *     i32    diameter;   // 2 * radius + 1
 *   }
 *   Collection {
 *     u64 typeId;       // mirror of the block-header type (for debug)
 *     u32 activeCount;  // number of live elements
 *     u32 elementClass; // class of elements (Map: key class)
 *     u32 stride;       // Bytes per element (Map: val class)
 *     u32 capacity;     // element capacity (or slot capacity)
 *     u32 head;         // circular head index (Deque/Queue); else 0
 *     u8 *data;         // element / slot buffer
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h)
 *   - CircleArray(radius, elementClass)
 *   - CircleArray(radius, elementClass, stride)
 *
 * Public Core Functions: (.h)
 *   - CircleArray_free(self)
 *   - CircleArray_radius(self)
 *   - CircleArray_diameter(self)
 *   - CircleArray_validCellCount(self)
 *   - CircleArray_containsGrid(self, gridX, gridY)
 *   - CircleArray_containsOffset(self, dx, dy)
 *   - CircleArray_distanceSquaredGrid(self, gridX, gridY)
 *   - CircleArray_distanceSquaredOffset(dx, dy)
 *   - CircleArray_getGrid(dest, self, gridX, gridY)
 *   - CircleArray_setGrid(self, gridX, gridY, src)
 *   - CircleArray_getOffset(dest, self, dx, dy)
 *   - CircleArray_setOffset(self, dx, dy, src)
 *   - CircleArray_slotGrid(self, gridX, gridY)
 *   - CircleArray_slotOffset(self, dx, dy)
 *   - CircleArray_forEach(self, callback, userData)
 * ============================================================================
 */


;;GETTER
static inline u32 Collection_activeCountRaw(const Collection *s) { return (*s).activeCount; }
;;SETTER
static inline fn Collection_setActiveCountRaw(Collection *s, u32 v) { (*s).activeCount = v; }
;;SETTER
static inline fn Collection_setCapacityRaw(Collection *s, u32 v) { (*s).capacity = v; }
;;GETTER
static inline u8 * Collection_dataRaw(const Collection *s) { return (*s).data; }
;;SETTER
static inline fn Collection_setDataRaw(Collection *s, u8 * v) { (*s).data = v; }
;;SETTER
static inline fn Collection_setElementClassRaw(Collection *s, u32 v) { (*s).elementClass = v; }
;;SETTER
static inline fn Collection_setExhaustionCountRaw(Collection *s, u64 v) { (*s).exhaustionCount = v; }
;;SETTER
static inline fn Collection_setExhaustionReportedRaw(Collection *s, bool v) { (*s).exhaustionReported = v; }
;;SETTER
static inline fn Collection_setHeadRaw(Collection *s, u32 v) { (*s).head = v; }
;;GETTER
static inline u32 Collection_strideRaw(const Collection *s) { return (*s).stride; }
;;SETTER
static inline fn Collection_setStrideRaw(Collection *s, u32 v) { (*s).stride = v; }
;;SETTER
static inline fn Collection_setTypeIdRaw(Collection *s, u64 v) { (*s).typeId = v; }

;;GETTER
static inline const Collection *CircleArray_collectionConst(const CircleArray *s) { return &(*s).collection; }
;;GETTER
static inline Collection *CircleArray_collectionMut(CircleArray *s) { return &(*s).collection; }
;;GETTER
static inline i32 CircleArray_diameterRaw(const CircleArray *s) { return (*s).diameter; }
;;SETTER
static inline fn CircleArray_setDiameter(CircleArray *s, i32 v) { (*s).diameter = v; }
;;GETTER
static inline i32 CircleArray_radiusRaw(const CircleArray *s) { return (*s).radius; }
;;SETTER
static inline fn CircleArray_setRadius(CircleArray *s, i32 v) { (*s).radius = v; }

CircleArray *CircleArray_3(i32 radius, u32 elementClass, bytes stride) {
    if (radius < 0) {
        return nullptr;
    }
    i32 diameter = 2 * radius + 1;
    bytes totalCells = (bytes) diameter * (bytes) diameter;
    bytes dataBytes = totalCells * stride;

    CircleArray *self = (CircleArray*) Memory_alloc(TYPE_CIRCLE_ARRAY, sizeof(CircleArray));
    if (self == nullptr) {
        (discard) Struct_reportConstructionExhaustion("circle_array", sizeof(CircleArray));
        return nullptr;
    }

    u8 *data = (u8*) Memory_alloc(TYPE_BYTE_ARRAY, dataBytes);
    if (data == nullptr) {
        (discard) Struct_reportConstructionExhaustion("circle_array", dataBytes);
        Memory_free(self);
        return nullptr;
    }
    memset(data, 0, dataBytes);

    Collection *coll = CircleArray_collectionMut(self);
    Collection_setTypeIdRaw(coll, TYPE_CIRCLE_ARRAY);
    Collection_setElementClassRaw(coll, elementClass);
    Collection_setStrideRaw(coll, (u32) stride);
    Collection_setCapacityRaw(coll, (u32) totalCells);
    Collection_setHeadRaw(coll, 0);
    Collection_setExhaustionCountRaw(coll, 0);
    Collection_setExhaustionReportedRaw(coll, false);
    Collection_setDataRaw(coll, data);
    CircleArray_setRadius(self, radius);
    CircleArray_setDiameter(self, diameter);

    // Calculate active count: number of valid cells where Pythagorean inequality holds
    i64 rSq = (i64) radius * (i64) radius;
    u32 active = 0;
    for (i32 gy = 0; gy < diameter; gy++) {
        i64 dy = (i64) gy - (i64) radius;
        for (i32 gx = 0; gx < diameter; gx++) {
            i64 dx = (i64) gx - (i64) radius;
            if (dx * dx + dy * dy <= rSq) {
                active++;
            }
        }
    }
    Collection_setActiveCountRaw(coll, active);

    return self;
}

CircleArray *CircleArray_2(i32 radius, u32 elementClass) {
    bytes stride = Stride_get(elementClass);
    if (stride == 0) {
        stride = 8;
    }
    return CircleArray_3(radius, elementClass, stride);
}

fn CircleArray_free(CircleArray *self) {
    if (self == nullptr) {
        return;
    }
    Collection *coll = CircleArray_collectionMut(self);
    if (Collection_dataRaw(coll) != nullptr) {
        Memory_free(Collection_dataRaw(coll));
        Collection_setDataRaw(coll, nullptr);
    }
    Memory_free(self);
}

i32 CircleArray_radius(const CircleArray *self) {
    if (self == nullptr) return 0;
    return CircleArray_radiusRaw(self);
}

i32 CircleArray_diameter(const CircleArray *self) {
    if (self == nullptr) return 0;
    return CircleArray_diameterRaw(self);
}

bytes CircleArray_validCellCount(const CircleArray *self) {
    if (self == nullptr) return 0;
    const Collection *coll = CircleArray_collectionConst(self);
    return (bytes) Collection_activeCountRaw(coll);
}

i64 CircleArray_distanceSquaredOffset(i32 dx, i32 dy) {
    i64 x = (i64) dx;
    i64 y = (i64) dy;
    return x * x + y * y;
}

i64 CircleArray_distanceSquaredGrid(const CircleArray *self, i32 gridX, i32 gridY) {
    if (self == nullptr) return -1;
    i64 dx = (i64) gridX - (i64) CircleArray_radius(self);
    i64 dy = (i64) gridY - (i64) CircleArray_radius(self);
    return dx * dx + dy * dy;
}

bool CircleArray_containsOffset(const CircleArray *self, i32 dx, i32 dy) {
    if (self == nullptr) return false;
    i32 r = CircleArray_radius(self);
    if (dx < -r || dx > r || dy < -r || dy > r) {
        return false;
    }
    i64 distSq = CircleArray_distanceSquaredOffset(dx, dy);
    i64 rSq = (i64) r * (i64) r;
    return distSq <= rSq;
}

bool CircleArray_containsGrid(const CircleArray *self, i32 gridX, i32 gridY) {
    if (self == nullptr) return false;
    i32 d = CircleArray_diameter(self);
    if (gridX < 0 || gridX >= d || gridY < 0 || gridY >= d) {
        return false;
    }
    i64 distSq = CircleArray_distanceSquaredGrid(self, gridX, gridY);
    i64 rSq = (i64) CircleArray_radius(self) * (i64) CircleArray_radius(self);
    return distSq <= rSq;
}

u8 *CircleArray_slotGrid(CircleArray *self, i32 gridX, i32 gridY) {
    if (!CircleArray_containsGrid(self, gridX, gridY)) {
        return nullptr;
    }
    bytes index = (bytes) gridY * (bytes) CircleArray_diameter(self) + (bytes) gridX;
    const Collection *coll = CircleArray_collectionConst(self);
    return Collection_dataRaw(coll) + (index * (bytes) Collection_strideRaw(coll));
}

u8 *CircleArray_slotOffset(CircleArray *self, i32 dx, i32 dy) {
    if (self == nullptr) return nullptr;
    i32 gridX = dx + CircleArray_radius(self);
    i32 gridY = dy + CircleArray_radius(self);
    return CircleArray_slotGrid(self, gridX, gridY);
}

bool CircleArray_getGrid(any dest, const CircleArray *self, i32 gridX, i32 gridY) {
    if (self == nullptr || dest == nullptr) return false;
    if (!CircleArray_containsGrid(self, gridX, gridY)) {
        return false;
    }
    bytes index = (bytes) gridY * (bytes) CircleArray_diameter(self) + (bytes) gridX;
    const Collection *coll = CircleArray_collectionConst(self);
    bytes stride = (bytes) Collection_strideRaw(coll);
    const u8 *src = Collection_dataRaw(coll) + (index * stride);
    memcpy(dest, src, stride);
    return true;
}

bool CircleArray_setGrid(CircleArray *self, i32 gridX, i32 gridY, const_any src) {
    if (self == nullptr || src == nullptr) return false;
    if (!CircleArray_containsGrid(self, gridX, gridY)) {
        return false;
    }
    bytes index = (bytes) gridY * (bytes) CircleArray_diameter(self) + (bytes) gridX;
    Collection *coll = CircleArray_collectionMut(self);
    bytes stride = (bytes) Collection_strideRaw(coll);
    u8 *dest = Collection_dataRaw(coll) + (index * stride);
    memcpy(dest, src, stride);
    return true;
}

bool CircleArray_getOffset(any dest, const CircleArray *self, i32 dx, i32 dy) {
    if (self == nullptr) return false;
    i32 gridX = dx + CircleArray_radius(self);
    i32 gridY = dy + CircleArray_radius(self);
    return CircleArray_getGrid(dest, self, gridX, gridY);
}

bool CircleArray_setOffset(CircleArray *self, i32 dx, i32 dy, const_any src) {
    if (self == nullptr) return false;
    i32 gridX = dx + CircleArray_radius(self);
    i32 gridY = dy + CircleArray_radius(self);
    return CircleArray_setGrid(self, gridX, gridY, src);
}

fn CircleArray_forEach(const CircleArray *self, CircleArrayCellFn callback, any userData) {
    if (self == nullptr || callback == nullptr) return;
    i32 r = CircleArray_radius(self);
    i32 d = CircleArray_diameter(self);
    i64 rSq = (i64) r * (i64) r;
    const Collection *coll = CircleArray_collectionConst(self);
    bytes stride = (bytes) Collection_strideRaw(coll);
    for (i32 gy = 0; gy < d; gy++) {
        i32 dy = gy - r;
        i64 dySq = (i64) dy * (i64) dy;
        for (i32 gx = 0; gx < d; gx++) {
            i32 dx = gx - r;
            if ((i64) dx * (i64) dx + dySq <= rSq) {
                bytes index = (bytes) gy * (bytes) d + (bytes) gx;
                const u8 *element = Collection_dataRaw(coll) + (index * stride);
                callback(gx, gy, dx, dy, element, userData);
            }
        }
    }
}
