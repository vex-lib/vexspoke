#ifndef LANG_VEC3_VEC3_LONG_f64_H
#define LANG_VEC3_VEC3_LONG_f64_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "lang/constructor.h"

#include "math/coord_frame.h"

// lang/vec3/vec3_long_f64.h — Cosmic-Scale 3D Vector (int64 sector + d64 local).
//
// Single Class Per File Law: Vec3Longf64.
//
// Unbounded spatial coordinate representation capable of addressing solar
// systems down to sub-millimeter precision without loss of significance.

typedef struct CoordLongf64 {
    int64_t sector;
    d64 local;
} CoordLongf64;

typedef struct Vec3Longf64 {
    CoordLongf64 horizontal;
    CoordLongf64 vertical;
    CoordLongf64 depth;
    u64 frame;
} Vec3Longf64;

Vec3Longf64 *Vec3Longf64_0(void);
Vec3Longf64 *Vec3Longf64_7(int64_t sectorH, d64 localH, int64_t sectorV, d64 localV,
                                 int64_t sectorD, d64 localD, CoordFrame frame);
#define Vec3Longf64(...) CONSTRUCTOR_DISPATCH(Vec3Longf64, __VA_ARGS__)
void Vec3Longf64_free(Vec3Longf64 *v);

void Vec3Longf64_rebalance(Vec3Longf64 *v, d64 sectorSize);

d64 Vec3Longf64_getRightLocal(const Vec3Longf64 *v);
d64 Vec3Longf64_getLeftLocal(const Vec3Longf64 *v);
d64 Vec3Longf64_getUpLocal(const Vec3Longf64 *v);
d64 Vec3Longf64_getDownLocal(const Vec3Longf64 *v);
d64 Vec3Longf64_getFrontLocal(const Vec3Longf64 *v);
d64 Vec3Longf64_getBackLocal(const Vec3Longf64 *v);

#endif
