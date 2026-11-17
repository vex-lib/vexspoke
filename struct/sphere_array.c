#include "vex.h"
#include "nio/mem.h"

#include "struct/sphere_array.h"
#include "struct/stride.h"
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


;;GETTER
static inline u32 Collection_activeCount(const Collection *s) { return (*s).activeCount; }
;;GETTER
static inline u32 *Collection_activeCountMut(Collection *s) { return &(*s).activeCount; }
;;SETTER
static inline fn Collection_setActiveCount(Collection *s, u32 v) { (*s).activeCount = v; }
;;GETTER
static inline u32 Collection_capacity(const Collection *s) { return (*s).capacity; }
;;GETTER
static inline u32 *Collection_capacityMut(Collection *s) { return &(*s).capacity; }
;;SETTER
static inline fn Collection_setCapacity(Collection *s, u32 v) { (*s).capacity = v; }
;;GETTER
static inline u8 * Collection_data(const Collection *s) { return (*s).data; }
;;GETTER
static inline u8 * *Collection_dataMut(Collection *s) { return &(*s).data; }
;;SETTER
static inline fn Collection_setData(Collection *s, u8 * v) { (*s).data = v; }
;;GETTER
static inline u32 Collection_elementClass(const Collection *s) { return (*s).elementClass; }
;;GETTER
static inline u32 *Collection_elementClassMut(Collection *s) { return &(*s).elementClass; }
;;SETTER
static inline fn Collection_setElementClass(Collection *s, u32 v) { (*s).elementClass = v; }
;;GETTER
static inline u64 Collection_exhaustionCount(const Collection *s) { return (*s).exhaustionCount; }
;;GETTER
static inline u64 *Collection_exhaustionCountMut(Collection *s) { return &(*s).exhaustionCount; }
;;SETTER
static inline fn Collection_setExhaustionCount(Collection *s, u64 v) { (*s).exhaustionCount = v; }
;;GETTER
static inline bool Collection_exhaustionReported(const Collection *s) { return (*s).exhaustionReported; }
;;GETTER
static inline bool *Collection_exhaustionReportedMut(Collection *s) { return &(*s).exhaustionReported; }
;;SETTER
static inline fn Collection_setExhaustionReported(Collection *s, bool v) { (*s).exhaustionReported = v; }
;;GETTER
static inline u32 Collection_head(const Collection *s) { return (*s).head; }
;;GETTER
static inline u32 *Collection_headMut(Collection *s) { return &(*s).head; }
;;SETTER
static inline fn Collection_setHead(Collection *s, u32 v) { (*s).head = v; }
;;GETTER
static inline u32 Collection_stride(const Collection *s) { return (*s).stride; }
;;GETTER
static inline u32 *Collection_strideMut(Collection *s) { return &(*s).stride; }
;;SETTER
static inline fn Collection_setStride(Collection *s, u32 v) { (*s).stride = v; }
;;GETTER
static inline u64 Collection_typeId(const Collection *s) { return (*s).typeId; }
;;GETTER
static inline u64 *Collection_typeIdMut(Collection *s) { return &(*s).typeId; }
;;SETTER
static inline fn Collection_setTypeId(Collection *s, u64 v) { (*s).typeId = v; }

;;GETTER
static inline Collection SphereArray_collection(const SphereArray *s) { return (*s).collection; }
;;GETTER
static inline Collection *SphereArray_collectionMut(SphereArray *s) { return &(*s).collection; }
;;SETTER
static inline fn SphereArray_setCollection(SphereArray *s, Collection v) { (*s).collection = v; }
;;GETTER
static inline i32 SphereArray_diameter(const SphereArray *s) { return (*s).diameter; }
;;GETTER
static inline i32 *SphereArray_diameterMut(SphereArray *s) { return &(*s).diameter; }
;;SETTER
static inline fn SphereArray_setDiameter(SphereArray *s, i32 v) { (*s).diameter = v; }
;;GETTER
static inline i32 SphereArray_radius(const SphereArray *s) { return (*s).radius; }
;;GETTER
static inline i32 *SphereArray_radiusMut(SphereArray *s) { return &(*s).radius; }
;;SETTER
static inline fn SphereArray_setRadius(SphereArray *s, i32 v) { (*s).radius = v; }

SphereArray *SphereArray_3(i32 radius, u32 elementClass, bytes stride) {
    if (radius < 0) {
        return nullptr;
    }
    i32 diameter = 2 * radius + 1;
    bytes totalVoxels = (bytes) diameter * (bytes) diameter * (bytes) diameter;
    bytes dataBytes = totalVoxels * stride;

    SphereArray *self = (SphereArray*) Memory_alloc(TYPE_SPHERE_ARRAY, sizeof(SphereArray));
    if (self == nullptr) {
        (discard) Struct_reportConstructionExhaustion("sphere_array", sizeof(SphereArray));
        return nullptr;
    }

    u8 *data = (u8*) Memory_alloc(TYPE_BYTE_ARRAY, dataBytes);
    if (data == nullptr) {
        (discard) Struct_reportConstructionExhaustion("sphere_array", dataBytes);
        Memory_free(self);
        return nullptr;
    }
    memset(data, 0, dataBytes);

    Collection *coll = SphereArray_collectionMut(self);
    Collection_setTypeId(coll, TYPE_SPHERE_ARRAY);
    Collection_setElementClass(coll, elementClass);
    Collection_setStride(coll, (u32) stride);
    Collection_setCapacity(coll, (u32) totalVoxels);
    Collection_setHead(coll, 0);
    Collection_setExhaustionCount(coll, 0);
    Collection_setExhaustionReported(coll, false);
    Collection_setData(coll, data);
    SphereArray_setRadius(self, radius);
    SphereArray_setDiameter(self, diameter);

    // Precalculate active count of voxels satisfying dx^2 + dy^2 + dz^2 <= r^2
    i64 rSq = (i64) radius * (i64) radius;
    u32 active = 0;
    for (i32 gz = 0; gz < diameter; gz++) {
        i64 dz = (i64) gz - (i64) radius;
        i64 dzSq = dz * dz;
        for (i32 gy = 0; gy < diameter; gy++) {
            i64 dy = (i64) gy - (i64) radius;
            i64 dyzSq = dzSq + dy * dy;
            if (dyzSq > rSq) continue;
            for (i32 gx = 0; gx < diameter; gx++) {
                i64 dx = (i64) gx - (i64) radius;
                if (dyzSq + dx * dx <= rSq) {
                    active++;
                }
            }
        }
    }
    Collection_setActiveCount(coll, active);

    return self;
}

SphereArray *SphereArray_2(i32 radius, u32 elementClass) {
    bytes stride = Stride_get(elementClass);
    if (stride == 0) {
        stride = 8;
    }
    return SphereArray_3(radius, elementClass, stride);
}

fn SphereArray_free(SphereArray *self) {
    if (self == nullptr) {
        return;
    }
    Collection *coll = SphereArray_collectionMut(self);
    if (Collection_data(coll) != nullptr) {
        Memory_free(Collection_data(coll));
        Collection_setData(coll, nullptr);
    }
    Memory_free(self);
}

i32 SphereArray_radius(const SphereArray *self) {
    if (self == nullptr) return 0;
    return SphereArray_radius(self);
}

i32 SphereArray_diameter(const SphereArray *self) {
    if (self == nullptr) return 0;
    return SphereArray_diameter(self);
}

bytes SphereArray_validVoxelCount(const SphereArray *self) {
    if (self == nullptr) return 0;
    const Collection *coll = SphereArray_collectionMut(self);
    return (bytes) Collection_activeCount(coll);
}

i64 SphereArray_distanceSquaredOffset(i32 dx, i32 dy, i32 dz) {
    i64 x = (i64) dx;
    i64 y = (i64) dy;
    i64 z = (i64) dz;
    return x * x + y * y + z * z;
}

i64 SphereArray_distanceSquaredGrid(const SphereArray *self, i32 gx, i32 gy, i32 gz) {
    if (self == nullptr) return -1;
    i64 dx = (i64) gx - (i64) SphereArray_radius(self);
    i64 dy = (i64) gy - (i64) SphereArray_radius(self);
    i64 dz = (i64) gz - (i64) SphereArray_radius(self);
    return dx * dx + dy * dy + dz * dz;
}

bool SphereArray_containsOffset(const SphereArray *self, i32 dx, i32 dy, i32 dz) {
    if (self == nullptr) return false;
    i32 r = SphereArray_radius(self);
    if (dx < -r || dx > r || dy < -r || dy > r || dz < -r || dz > r) {
        return false;
    }
    i64 distSq = SphereArray_distanceSquaredOffset(dx, dy, dz);
    i64 rSq = (i64) r * (i64) r;
    return distSq <= rSq;
}

bool SphereArray_containsGrid(const SphereArray *self, i32 gx, i32 gy, i32 gz) {
    if (self == nullptr) return false;
    i32 d = SphereArray_diameter(self);
    if (gx < 0 || gx >= d || gy < 0 || gy >= d || gz < 0 || gz >= d) {
        return false;
    }
    i64 distSq = SphereArray_distanceSquaredGrid(self, gx, gy, gz);
    i64 rSq = (i64) SphereArray_radius(self) * (i64) SphereArray_radius(self);
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
    bytes idx = SphereArray_index(gx, gy, gz, SphereArray_diameter(self));
    const Collection *coll = SphereArray_collectionMut(self);
    return Collection_data(coll) + (idx * (bytes) Collection_stride(coll));
}

u8 *SphereArray_slotOffset(SphereArray *self, i32 dx, i32 dy, i32 dz) {
    if (self == nullptr) return nullptr;
    i32 gx = dx + SphereArray_radius(self);
    i32 gy = dy + SphereArray_radius(self);
    i32 gz = dz + SphereArray_radius(self);
    return SphereArray_slotGrid(self, gx, gy, gz);
}

bool SphereArray_getGrid(any dest, const SphereArray *self, i32 gx, i32 gy, i32 gz) {
    if (self == nullptr || dest == nullptr) return false;
    if (!SphereArray_containsGrid(self, gx, gy, gz)) {
        return false;
    }
    bytes idx = SphereArray_index(gx, gy, gz, SphereArray_diameter(self));
    const Collection *coll = SphereArray_collectionMut(self);
    bytes stride = (bytes) Collection_stride(coll);
    const u8 *src = Collection_data(coll) + (idx * stride);
    memcpy(dest, src, stride);
    return true;
}

bool SphereArray_setGrid(SphereArray *self, i32 gx, i32 gy, i32 gz, const_any src) {
    if (self == nullptr || src == nullptr) return false;
    if (!SphereArray_containsGrid(self, gx, gy, gz)) {
        return false;
    }
    bytes idx = SphereArray_index(gx, gy, gz, SphereArray_diameter(self));
    Collection *coll = SphereArray_collectionMut(self);
    bytes stride = (bytes) Collection_stride(coll);
    u8 *dest = Collection_data(coll) + (idx * stride);
    memcpy(dest, src, stride);
    return true;
}

bool SphereArray_getOffset(any dest, const SphereArray *self, i32 dx, i32 dy, i32 dz) {
    if (self == nullptr) return false;
    i32 gx = dx + SphereArray_radius(self);
    i32 gy = dy + SphereArray_radius(self);
    i32 gz = dz + SphereArray_radius(self);
    return SphereArray_getGrid(dest, self, gx, gy, gz);
}

bool SphereArray_setOffset(SphereArray *self, i32 dx, i32 dy, i32 dz, const_any src) {
    if (self == nullptr) return false;
    i32 gx = dx + SphereArray_radius(self);
    i32 gy = dy + SphereArray_radius(self);
    i32 gz = dz + SphereArray_radius(self);
    return SphereArray_setGrid(self, gx, gy, gz, src);
}

fn SphereArray_forEach(const SphereArray *self, SphereArrayVoxelFn callback, any userData) {
    if (self == nullptr || callback == nullptr) return;
    i32 r = SphereArray_radius(self);
    i32 d = SphereArray_diameter(self);
    i64 rSq = (i64) r * (i64) r;
    const Collection *coll = SphereArray_collectionMut(self);
    bytes stride = (bytes) Collection_stride(coll);
    for (i32 gz = 0; gz < d; gz++) {
        i32 dz = gz - r;
        i64 dzSq = (i64) dz * (i64) dz;
        for (i32 gy = 0; gy < d; gy++) {
            i32 dy = gy - r;
            i64 dyzSq = dzSq + ((i64) dy * (i64) dy);
            if (dyzSq > rSq) continue;
            for (i32 gx = 0; gx < d; gx++) {
                i32 dx = gx - r;
                if (dyzSq + ((i64) dx * (i64) dx) <= rSq) {
                    bytes idx = SphereArray_index(gx, gy, gz, d);
                    const u8 *voxel = Collection_data(coll) + (idx * stride);
                    callback(gx, gy, gz, dx, dy, dz, voxel, userData);
                }
            }
        }
    }
}
