#include "vex.h"
#include "nio/mem.h"

#include "struct/sphere_array.h"
#include "oop/stride.h"
#include "oop/vexspoke_type.h"
#include "struct/exhaustion.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: SphereArray
 * ============================================================================
 * 3D spherical voxel matrix evaluated via the 3D Pythagorean theorem: a cubic
 * bounding volume of (2*radius + 1)^3 voxels where only cells whose Euclidean
 * distance from center satisfies dx^2 + dy^2 + dz^2 <= radius^2 are valid.
 * A Collection header plus a flat data buffer hold the voxels; the active
 * count is precomputed at construction so iteration never re-tests the whole
 * cube. Grid accessors address voxels by [0, diameter) coordinates and offset
 * accessors by center-relative [-radius, radius] deltas; both bounds-check
 * before touching memory. forEach walks only the valid spherical shell.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: SphereArray (struct/sphere_array.c)
 * ============================================================================
 * 3D Spherical Voxel Matrix evaluated via the 3D Pythagorean Theorem.
 *
 * Encapsulates a cubic bounding voxel volume of size (2*radius + 1)^3 where
 * only voxels whose Euclidean distance from center satisfies:
 * dx^2 + dy^2 + dz^2 <= radius^2
 * are considered valid and accessible.
 *
 * STRUCT FIELDS (Mirroring struct/sphere_array.h):
 * ----------------------------------------------------------------------------
 *   SphereArray {
 *     Collection collection; // base collection header (typeId, activeCount, stride, data)
 *     i32 radius;        // radius in voxels
 *     i32 diameter;      // 2 * radius + 1
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - SphereArray(radius, elementClass)
 *   - SphereArray(radius, elementClass, stride)
 *
 * Core Functions:
 *   - SphereArray_free(self)
 *   - SphereArray_containsGrid(self, gx, gy, gz)
 *   - SphereArray_containsOffset(self, dx, dy, dz)
 *   - SphereArray_distanceSquaredGrid(self, gx, gy, gz)
 *   - SphereArray_distanceSquaredOffset(dx, dy, dz)
 *   - SphereArray_slotGrid(self, gx, gy, gz)
 *   - SphereArray_slotOffset(self, dx, dy, dz)
 *   - SphereArray_getGrid(dest, self, gx, gy, gz)
 *   - SphereArray_setGrid(self, gx, gy, gz, src)
 *   - SphereArray_getOffset(dest, self, dx, dy, dz)
 *   - SphereArray_setOffset(self, dx, dy, dz, src)
 *   - SphereArray_forEach(self, callback, userData)
 *
 * Private Core Functions: (.c static)
 *   - SphereArray_index(gx, gy, gz, diameter)
 *
 * Getters:
 *   - SphereArray_radius(self)
 *   - SphereArray_diameter(self)
 *   - SphereArray_validVoxelCount(self)
 * ============================================================================
 */

SphereArray *SphereArray_3(i32 radius, u32 elementClass, bytes stride) {
    if (radius < 0) {
        return nullptr;
    }
    i32 diameter = 2 * radius + 1;
    bytes totalVoxels = (bytes) diameter * (bytes) diameter * (bytes) diameter;
    bytes dataBytes = totalVoxels * stride;

    SphereArray *self = (SphereArray*) Memory_alloc(TYPE_SPHERE_ARRAY, sizeof(SphereArray));
    if (self == nullptr) {
        (void) Struct_reportConstructionExhaustion("sphere_array", sizeof(SphereArray));
        return nullptr;
    }

    u8 *data = (u8*) Memory_alloc(TYPE_BYTE_ARRAY, dataBytes);
    if (data == nullptr) {
        (void) Struct_reportConstructionExhaustion("sphere_array", dataBytes);
        Memory_free(self);
        return nullptr;
    }
    memset(data, 0, dataBytes);

    Collection *coll = &(*self).collection;
    (*coll).typeId = TYPE_SPHERE_ARRAY;
    (*coll).elementClass = elementClass;
    (*coll).stride = (u32) stride;
    (*coll).capacity = (u32) totalVoxels;
    (*coll).head = 0;
    (*coll).exhaustionCount = 0;
    (*coll).exhaustionReported = false;
    (*coll).data = data;
    (*self).radius = radius;
    (*self).diameter = diameter;

    // Precalculate active count of voxels satisfying dx^2 + dy^2 + dz^2 <= r^2
    int64_t rSq = (int64_t) radius * (int64_t) radius;
    u32 active = 0;
    for (i32 gz = 0; gz < diameter; gz++) {
        int64_t dz = (int64_t) gz - (int64_t) radius;
        int64_t dzSq = dz * dz;
        for (i32 gy = 0; gy < diameter; gy++) {
            int64_t dy = (int64_t) gy - (int64_t) radius;
            int64_t dyzSq = dzSq + dy * dy;
            if (dyzSq > rSq) continue;
            for (i32 gx = 0; gx < diameter; gx++) {
                int64_t dx = (int64_t) gx - (int64_t) radius;
                if (dyzSq + dx * dx <= rSq) {
                    active++;
                }
            }
        }
    }
    (*coll).activeCount = active;

    return self;
}

SphereArray *SphereArray_2(i32 radius, u32 elementClass) {
    bytes stride = Stride_get(elementClass);
    if (stride == 0) {
        stride = 8;
    }
    return SphereArray_3(radius, elementClass, stride);
}

void SphereArray_free(SphereArray *self) {
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

i32 SphereArray_radius(const SphereArray *self) {
    if (self == nullptr) return 0;
    return (*self).radius;
}

i32 SphereArray_diameter(const SphereArray *self) {
    if (self == nullptr) return 0;
    return (*self).diameter;
}

bytes SphereArray_validVoxelCount(const SphereArray *self) {
    if (self == nullptr) return 0;
    const Collection *coll = &(*self).collection;
    return (bytes) (*coll).activeCount;
}

int64_t SphereArray_distanceSquaredOffset(i32 dx, i32 dy, i32 dz) {
    int64_t x = (int64_t) dx;
    int64_t y = (int64_t) dy;
    int64_t z = (int64_t) dz;
    return x * x + y * y + z * z;
}

int64_t SphereArray_distanceSquaredGrid(const SphereArray *self, i32 gx, i32 gy, i32 gz) {
    if (self == nullptr) return -1;
    int64_t dx = (int64_t) gx - (int64_t) (*self).radius;
    int64_t dy = (int64_t) gy - (int64_t) (*self).radius;
    int64_t dz = (int64_t) gz - (int64_t) (*self).radius;
    return dx * dx + dy * dy + dz * dz;
}

bool SphereArray_containsOffset(const SphereArray *self, i32 dx, i32 dy, i32 dz) {
    if (self == nullptr) return false;
    i32 r = (*self).radius;
    if (dx < -r || dx > r || dy < -r || dy > r || dz < -r || dz > r) {
        return false;
    }
    int64_t distSq = SphereArray_distanceSquaredOffset(dx, dy, dz);
    int64_t rSq = (int64_t) r * (int64_t) r;
    return distSq <= rSq;
}

bool SphereArray_containsGrid(const SphereArray *self, i32 gx, i32 gy, i32 gz) {
    if (self == nullptr) return false;
    i32 d = (*self).diameter;
    if (gx < 0 || gx >= d || gy < 0 || gy >= d || gz < 0 || gz >= d) {
        return false;
    }
    int64_t distSq = SphereArray_distanceSquaredGrid(self, gx, gy, gz);
    int64_t rSq = (int64_t) (*self).radius * (int64_t) (*self).radius;
    return distSq <= rSq;
}

static inline bytes SphereArray_index(i32 gx, i32 gy, i32 gz, i32 diameter) {
    bytes d = (bytes) diameter;
    return ((bytes) gz * d * d) + ((bytes) gy * d) + (bytes) gx;
}

u8 *SphereArray_slotGrid(SphereArray *self, i32 gx, i32 gy, i32 gz) {
    if (!SphereArray_containsGrid(self, gx, gy, gz)) {
        return nullptr;
    }
    bytes idx = SphereArray_index(gx, gy, gz, (*self).diameter);
    const Collection *coll = &(*self).collection;
    return (*coll).data + (idx * (bytes) (*coll).stride);
}

u8 *SphereArray_slotOffset(SphereArray *self, i32 dx, i32 dy, i32 dz) {
    if (self == nullptr) return nullptr;
    i32 gx = dx + (*self).radius;
    i32 gy = dy + (*self).radius;
    i32 gz = dz + (*self).radius;
    return SphereArray_slotGrid(self, gx, gy, gz);
}

bool SphereArray_getGrid(void *dest, const SphereArray *self, i32 gx, i32 gy, i32 gz) {
    if (self == nullptr || dest == nullptr) return false;
    if (!SphereArray_containsGrid(self, gx, gy, gz)) {
        return false;
    }
    bytes idx = SphereArray_index(gx, gy, gz, (*self).diameter);
    const Collection *coll = &(*self).collection;
    bytes stride = (bytes) (*coll).stride;
    const u8 *src = (*coll).data + (idx * stride);
    memcpy(dest, src, stride);
    return true;
}

bool SphereArray_setGrid(SphereArray *self, i32 gx, i32 gy, i32 gz, const void *src) {
    if (self == nullptr || src == nullptr) return false;
    if (!SphereArray_containsGrid(self, gx, gy, gz)) {
        return false;
    }
    bytes idx = SphereArray_index(gx, gy, gz, (*self).diameter);
    Collection *coll = &(*self).collection;
    bytes stride = (bytes) (*coll).stride;
    u8 *dest = (*coll).data + (idx * stride);
    memcpy(dest, src, stride);
    return true;
}

bool SphereArray_getOffset(void *dest, const SphereArray *self, i32 dx, i32 dy, i32 dz) {
    if (self == nullptr) return false;
    i32 gx = dx + (*self).radius;
    i32 gy = dy + (*self).radius;
    i32 gz = dz + (*self).radius;
    return SphereArray_getGrid(dest, self, gx, gy, gz);
}

bool SphereArray_setOffset(SphereArray *self, i32 dx, i32 dy, i32 dz, const void *src) {
    if (self == nullptr) return false;
    i32 gx = dx + (*self).radius;
    i32 gy = dy + (*self).radius;
    i32 gz = dz + (*self).radius;
    return SphereArray_setGrid(self, gx, gy, gz, src);
}

void SphereArray_forEach(const SphereArray *self, SphereArrayVoxelFn callback, void *userData) {
    if (self == nullptr || callback == nullptr) return;
    i32 r = (*self).radius;
    i32 d = (*self).diameter;
    int64_t rSq = (int64_t) r * (int64_t) r;
    const Collection *coll = &(*self).collection;
    bytes stride = (bytes) (*coll).stride;
    for (i32 gz = 0; gz < d; gz++) {
        i32 dz = gz - r;
        int64_t dzSq = (int64_t) dz * (int64_t) dz;
        for (i32 gy = 0; gy < d; gy++) {
            i32 dy = gy - r;
            int64_t dyzSq = dzSq + ((int64_t) dy * (int64_t) dy);
            if (dyzSq > rSq) continue;
            for (i32 gx = 0; gx < d; gx++) {
                i32 dx = gx - r;
                if (dyzSq + ((int64_t) dx * (int64_t) dx) <= rSq) {
                    bytes idx = SphereArray_index(gx, gy, gz, d);
                    const u8 *voxel = (*coll).data + (idx * stride);
                    callback(gx, gy, gz, dx, dy, dz, voxel, userData);
                }
            }
        }
    }
}
