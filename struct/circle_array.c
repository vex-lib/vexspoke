#include "vex.h"
#include "nio/mem.h"

#include "struct/circle_array.h"
#include "oop/stride.h"
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

CircleArray *CircleArray_3(i32 radius, u32 elementClass, bytes stride) {
    if (radius < 0) {
        return nullptr;
    }
    i32 diameter = 2 * radius + 1;
    bytes totalCells = (bytes) diameter * (bytes) diameter;
    bytes dataBytes = totalCells * stride;

    CircleArray *self = (CircleArray*) Memory_alloc(TYPE_CIRCLE_ARRAY, sizeof(CircleArray));
    if (self == nullptr) {
        (fn) Struct_reportConstructionExhaustion("circle_array", sizeof(CircleArray));
        return nullptr;
    }

    u8 *data = (u8*) Memory_alloc(TYPE_BYTE_ARRAY, dataBytes);
    if (data == nullptr) {
        (fn) Struct_reportConstructionExhaustion("circle_array", dataBytes);
        Memory_free(self);
        return nullptr;
    }
    memset(data, 0, dataBytes);

    Collection *coll = &(*self).collection;
    (*coll).typeId = TYPE_CIRCLE_ARRAY;
    (*coll).elementClass = elementClass;
    (*coll).stride = (u32) stride;
    (*coll).capacity = (u32) totalCells;
    (*coll).head = 0;
    (*coll).exhaustionCount = 0;
    (*coll).exhaustionReported = false;
    (*coll).data = data;
    (*self).radius = radius;
    (*self).diameter = diameter;

    // Calculate active count: number of valid cells where Pythagorean inequality holds
    int64_t rSq = (int64_t) radius * (int64_t) radius;
    u32 active = 0;
    for (i32 gy = 0; gy < diameter; gy++) {
        int64_t dy = (int64_t) gy - (int64_t) radius;
        for (i32 gx = 0; gx < diameter; gx++) {
            int64_t dx = (int64_t) gx - (int64_t) radius;
            if (dx * dx + dy * dy <= rSq) {
                active++;
            }
        }
    }
    (*coll).activeCount = active;

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
    Collection *coll = &(*self).collection;
    if ((*coll).data != nullptr) {
        Memory_free((*coll).data);
        (*coll).data = nullptr;
    }
    Memory_free(self);
}

i32 CircleArray_radius(const CircleArray *self) {
    if (self == nullptr) return 0;
    return (*self).radius;
}

i32 CircleArray_diameter(const CircleArray *self) {
    if (self == nullptr) return 0;
    return (*self).diameter;
}

bytes CircleArray_validCellCount(const CircleArray *self) {
    if (self == nullptr) return 0;
    const Collection *coll = &(*self).collection;
    return (bytes) (*coll).activeCount;
}

int64_t CircleArray_distanceSquaredOffset(i32 dx, i32 dy) {
    int64_t x = (int64_t) dx;
    int64_t y = (int64_t) dy;
    return x * x + y * y;
}

int64_t CircleArray_distanceSquaredGrid(const CircleArray *self, i32 gridX, i32 gridY) {
    if (self == nullptr) return -1;
    int64_t dx = (int64_t) gridX - (int64_t) (*self).radius;
    int64_t dy = (int64_t) gridY - (int64_t) (*self).radius;
    return dx * dx + dy * dy;
}

bool CircleArray_containsOffset(const CircleArray *self, i32 dx, i32 dy) {
    if (self == nullptr) return false;
    i32 r = (*self).radius;
    if (dx < -r || dx > r || dy < -r || dy > r) {
        return false;
    }
    int64_t distSq = CircleArray_distanceSquaredOffset(dx, dy);
    int64_t rSq = (int64_t) r * (int64_t) r;
    return distSq <= rSq;
}

bool CircleArray_containsGrid(const CircleArray *self, i32 gridX, i32 gridY) {
    if (self == nullptr) return false;
    i32 d = (*self).diameter;
    if (gridX < 0 || gridX >= d || gridY < 0 || gridY >= d) {
        return false;
    }
    int64_t distSq = CircleArray_distanceSquaredGrid(self, gridX, gridY);
    int64_t rSq = (int64_t) (*self).radius * (int64_t) (*self).radius;
    return distSq <= rSq;
}

u8 *CircleArray_slotGrid(CircleArray *self, i32 gridX, i32 gridY) {
    if (!CircleArray_containsGrid(self, gridX, gridY)) {
        return nullptr;
    }
    bytes index = (bytes) gridY * (bytes) (*self).diameter + (bytes) gridX;
    const Collection *coll = &(*self).collection;
    return (*coll).data + (index * (bytes) (*coll).stride);
}

u8 *CircleArray_slotOffset(CircleArray *self, i32 dx, i32 dy) {
    if (self == nullptr) return nullptr;
    i32 gridX = dx + (*self).radius;
    i32 gridY = dy + (*self).radius;
    return CircleArray_slotGrid(self, gridX, gridY);
}

bool CircleArray_getGrid(any dest, const CircleArray *self, i32 gridX, i32 gridY) {
    if (self == nullptr || dest == nullptr) return false;
    if (!CircleArray_containsGrid(self, gridX, gridY)) {
        return false;
    }
    bytes index = (bytes) gridY * (bytes) (*self).diameter + (bytes) gridX;
    const Collection *coll = &(*self).collection;
    bytes stride = (bytes) (*coll).stride;
    const u8 *src = (*coll).data + (index * stride);
    memcpy(dest, src, stride);
    return true;
}

bool CircleArray_setGrid(CircleArray *self, i32 gridX, i32 gridY, const_any src) {
    if (self == nullptr || src == nullptr) return false;
    if (!CircleArray_containsGrid(self, gridX, gridY)) {
        return false;
    }
    bytes index = (bytes) gridY * (bytes) (*self).diameter + (bytes) gridX;
    Collection *coll = &(*self).collection;
    bytes stride = (bytes) (*coll).stride;
    u8 *dest = (*coll).data + (index * stride);
    memcpy(dest, src, stride);
    return true;
}

bool CircleArray_getOffset(any dest, const CircleArray *self, i32 dx, i32 dy) {
    if (self == nullptr) return false;
    i32 gridX = dx + (*self).radius;
    i32 gridY = dy + (*self).radius;
    return CircleArray_getGrid(dest, self, gridX, gridY);
}

bool CircleArray_setOffset(CircleArray *self, i32 dx, i32 dy, const_any src) {
    if (self == nullptr) return false;
    i32 gridX = dx + (*self).radius;
    i32 gridY = dy + (*self).radius;
    return CircleArray_setGrid(self, gridX, gridY, src);
}

fn CircleArray_forEach(const CircleArray *self, CircleArrayCellFn callback, any userData) {
    if (self == nullptr || callback == nullptr) return;
    i32 r = (*self).radius;
    i32 d = (*self).diameter;
    int64_t rSq = (int64_t) r * (int64_t) r;
    const Collection *coll = &(*self).collection;
    bytes stride = (bytes) (*coll).stride;
    for (i32 gy = 0; gy < d; gy++) {
        i32 dy = gy - r;
        int64_t dySq = (int64_t) dy * (int64_t) dy;
        for (i32 gx = 0; gx < d; gx++) {
            i32 dx = gx - r;
            if ((int64_t) dx * (int64_t) dx + dySq <= rSq) {
                bytes index = (bytes) gy * (bytes) d + (bytes) gx;
                const u8 *element = (*coll).data + (index * stride);
                callback(gx, gy, dx, dy, element, userData);
            }
        }
    }
}
