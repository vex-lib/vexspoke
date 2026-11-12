#include "lang/vec3/vec3_long_f64.h"

#include "nio/mem.h"
#include "oop/vexspoke_type.h"
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Vec3Longf64
 * ============================================================================
 * Cosmic-scale 3D vector: each axis is an int64 sector plus a d64 local
 * offset, so solar-system and galaxy-scale coordinates keep sub-millimeter
 * precision without loss of significance. Vec3Longf64_rebalance carries
 * local overflow into the sector using a caller-supplied sector size. The
 * struct is arena-allocated (ID_VEC3) and freed by Vec3Longf64_free.
 * Lives at R2 as a leaf math primitive for the R5 spatial engines.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Vec3Longf64 (lang/vec3/vec3_long_f64.c — defined in lang/vec3/vec3_long_f64.h)
 * ============================================================================
 * Unbounded spatial coordinate representation for solar-system and galaxy scale.
 *
 * STRUCT FIELDS (Mirroring lang/vec3/vec3_long_f64.h):
 * ----------------------------------------------------------------------------
 *   CoordLongf64 {
 *     int64_t sector;  // coarse sector index (int64, unbounded)
 *     d64 local;    // fine offset within the sector
 *   }
 *   Vec3Longf64 {
 *     CoordLongf64 horizontal;  // right (+) / left (-) axis
 *     CoordLongf64 vertical;    // up (+) / down (-) axis
 *     CoordLongf64 depth;       // front (+) / back (-) axis
 *     u64 frame;              // CoordFrame tag (COORD_FRAME_DEFAULT)
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h)
 *   - Vec3Longf64_0(void)
 *   - Vec3Longf64(sectorH, localH, sectorV, localV, sectorD, localD, frame)
 * Public Core Functions: (.h)
 *   - Vec3Longf64_free(v)
 *   - Vec3Longf64_rebalance(v, sectorSize)
 * Public Getters: (.h)
 *   - Vec3Longf64_getRightLocal(v)
 *   - Vec3Longf64_getLeftLocal(v)
 *   - Vec3Longf64_getUpLocal(v)
 *   - Vec3Longf64_getDownLocal(v)
 *   - Vec3Longf64_getFrontLocal(v)
 *   - Vec3Longf64_getBackLocal(v)
 * Private Core Functions: (.c static)
 *   - rebalance_axis_d(axis, sectorSize)
 * ============================================================================
 */

Vec3Longf64 *Vec3Longf64_0(void) {
    Vec3Longf64 *v = (Vec3Longf64*) Memory_alloc(ID_VEC3, sizeof(Vec3Longf64));
    if (!v) return nullptr;
    (*v).horizontal.sector = 0;
    (*v).horizontal.local = 0.0;
    (*v).vertical.sector = 0;
    (*v).vertical.local = 0.0;
    (*v).depth.sector = 0;
    (*v).depth.local = 0.0;
    (*v).frame = (u64) COORD_FRAME_DEFAULT;
    return v;
}

Vec3Longf64 *Vec3Longf64_7(int64_t sectorH, d64 localH, int64_t sectorV, d64 localV,
                                 int64_t sectorD, d64 localD, CoordFrame frame) {
    Vec3Longf64 *v = (Vec3Longf64*) Memory_alloc(ID_VEC3, sizeof(Vec3Longf64));
    if (!v) return nullptr;
    (*v).horizontal.sector = sectorH;
    (*v).horizontal.local = localH;
    (*v).vertical.sector = sectorV;
    (*v).vertical.local = localV;
    (*v).depth.sector = sectorD;
    (*v).depth.local = localD;
    (*v).frame = CoordFrame_isValid(frame) ? (u64) frame : (u64) COORD_FRAME_DEFAULT;
    return v;
}

void Vec3Longf64_free(Vec3Longf64 *v) {
    if (v) Memory_free(v);
}

static void rebalance_axis_d(CoordLongf64 *axis, d64 sectorSize) {
    if (sectorSize <= 0.0) return;
    while ((*axis).local >= sectorSize) {
        (*axis).local -= sectorSize;
        (*axis).sector += 1;
    }
    while ((*axis).local < 0.0) {
        (*axis).local += sectorSize;
        (*axis).sector -= 1;
    }
}

void Vec3Longf64_rebalance(Vec3Longf64 *v, d64 sectorSize) {
    if (!v) return;
    rebalance_axis_d(&(*v).horizontal, sectorSize);
    rebalance_axis_d(&(*v).vertical, sectorSize);
    rebalance_axis_d(&(*v).depth, sectorSize);
}

d64 Vec3Longf64_getRightLocal(const Vec3Longf64 *v) { return v ? (*v).horizontal.local : 0.0; }
d64 Vec3Longf64_getLeftLocal(const Vec3Longf64 *v)  { return v ? -(*v).horizontal.local : 0.0; }
d64 Vec3Longf64_getUpLocal(const Vec3Longf64 *v)    { return v ? (*v).vertical.local : 0.0; }
d64 Vec3Longf64_getDownLocal(const Vec3Longf64 *v)  { return v ? -(*v).vertical.local : 0.0; }
d64 Vec3Longf64_getFrontLocal(const Vec3Longf64 *v) { return v ? (*v).depth.local : 0.0; }
d64 Vec3Longf64_getBackLocal(const Vec3Longf64 *v)  { return v ? -(*v).depth.local : 0.0; }
