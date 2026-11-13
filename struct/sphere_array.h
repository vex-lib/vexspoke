#ifndef STRUCT_SPHERE_ARRAY_H
#define STRUCT_SPHERE_ARRAY_H

#include "vex.h"

#include "struct/collection.h"

// struct/sphere_array.h — 3D Spherical Voxel Matrix evaluated via 3D Pythagorean Theorem.
//
// Represents a 3D spherical volume bounded by a cubic matrix of size (2*radius + 1)^3.
// Valid voxels satisfy: (gx - r)^2 + (gy - r)^2 + (gz - r)^2 <= r^2.
// Alternatively accessible via center-relative offsets (dx, dy, dz) where dx^2 + dy^2 + dz^2 <= r^2.

typedef struct SphereArray {
    Collection collection;   // Base collection header (typeId, activeCount, etc.)
    i32    radius;       // Radius in voxels
    i32    diameter;     // 2 * radius + 1
} SphereArray;

// Allocation
SphereArray *SphereArray_2(i32 radius, u32 elementClass);
SphereArray *SphereArray_3(i32 radius, u32 elementClass, bytes stride);
#define SphereArray(...) CONSTRUCTOR_DISPATCH(SphereArray, __VA_ARGS__)
void SphereArray_free(SphereArray *self);

i32 SphereArray_radius(const SphereArray *self);
i32 SphereArray_diameter(const SphereArray *self);
bytes  SphereArray_validVoxelCount(const SphereArray *self);

// 3D Pythagorean containment tests
bool SphereArray_containsGrid(const SphereArray *self, i32 gx, i32 gy, i32 gz);
bool SphereArray_containsOffset(const SphereArray *self, i32 dx, i32 dy, i32 dz);
int64_t SphereArray_distanceSquaredGrid(const SphereArray *self, i32 gx, i32 gy, i32 gz);
int64_t SphereArray_distanceSquaredOffset(i32 dx, i32 dy, i32 dz);

// Voxel access by grid coordinates [0, diameter - 1] (get output first, set input last)
bool SphereArray_getGrid(any dest, const SphereArray *self, i32 gx, i32 gy, i32 gz);
bool SphereArray_setGrid(SphereArray *self, i32 gx, i32 gy, i32 gz, const_any src);

// Voxel access by center-relative offsets [-radius, radius]
bool SphereArray_getOffset(any dest, const SphereArray *self, i32 dx, i32 dy, i32 dz);
bool SphereArray_setOffset(SphereArray *self, i32 dx, i32 dy, i32 dz, const_any src);

u8 *SphereArray_slotGrid(SphereArray *self, i32 gx, i32 gy, i32 gz);
u8 *SphereArray_slotOffset(SphereArray *self, i32 dx, i32 dy, i32 dz);

// Iteration callback across all valid spherical voxels
typedef void (*SphereArrayVoxelFn)(i32 gx, i32 gy, i32 gz, i32 dx, i32 dy, i32 dz, const_any voxel, any userData);
void SphereArray_forEach(const SphereArray *self, SphereArrayVoxelFn callback, any userData);

#endif
