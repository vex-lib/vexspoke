#include "lang/vec3/vec3_int_f32.h"

#include <math.h>

#include "nio/mem.h"
#include "oop/type.h"
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Vec3IntFloat
 * ============================================================================
 * Large-world 3D vector that eliminates 32-bit f32ing-point jitter at
 * great distances by pairing an integer grid sector with a normalized local
 * f32 offset on each of the three axes (horizontal/vertical/depth). A
 * CoordFrame id records which spatial convention the components are stated
 * in. Vec3IntFloat_rebalance normalizes sector overflow: when |local| passes
 * sectorSize the sector bumps and the local offset wraps back into range, so
 * coordinates stay bounded and exact. Directional getters (right/left/up/
 * down/front/back) expose signed local components for input and camera math.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Vec3IntFloat (lang/vec3/vec3_int_f32.c — defined in lang/vec3/vec3_int_f32.h)
 * ============================================================================
 * Represents coordinates across massive open worlds without precision loss.
 *
 * STRUCT FIELDS (Mirroring lang/vec3/vec3_int_f32.h):
 * ----------------------------------------------------------------------------
 *   CoordIntFloat {
 *     i32 sector; // integer grid tile index
 *     f32 local;    // normalized offset within the sector
 *   }
 *   Vec3IntFloat {
 *     CoordIntFloat horizontal; // right/left axis (sector + local)
 *     CoordIntFloat vertical;   // up/down axis (sector + local)
 *     CoordIntFloat depth;      // front/back axis (sector + local)
 *     u32 frame;           // CoordFrame id (COORD_FRAME_DEFAULT when unset)
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Vec3IntFloat_0()
 *   - Vec3IntFloat(sectorH, localH, sectorV, localV, sectorD, localD, frame)
 *
 * Core Functions:
 *   - Vec3IntFloat_free(v)
 *   - Vec3IntFloat_rebalance(v, sectorSize)
 *
 * Private Core Functions: (.c static)
 *   - rebalance_axis(axis, sectorSize)
 *
 * Getters:
 *   - Vec3IntFloat_getRightLocal(v)
 *   - Vec3IntFloat_getLeftLocal(v)
 *   - Vec3IntFloat_getUpLocal(v)
 *   - Vec3IntFloat_getDownLocal(v)
 *   - Vec3IntFloat_getFrontLocal(v)
 *   - Vec3IntFloat_getBackLocal(v)
 * ============================================================================
 */

Vec3IntFloat *Vec3IntFloat_0(void) {
    Vec3IntFloat *v = (Vec3IntFloat*) Memory_alloc(ID_VEC3, sizeof(Vec3IntFloat));
    if (!v) return nullptr;
    (*v).horizontal.sector = 0;
    (*v).horizontal.local = 0.0f;
    (*v).vertical.sector = 0;
    (*v).vertical.local = 0.0f;
    (*v).depth.sector = 0;
    (*v).depth.local = 0.0f;
    (*v).frame = (u32) COORD_FRAME_DEFAULT;
    return v;
}

Vec3IntFloat *Vec3IntFloat_7(i32 sectorH, f32 localH, i32 sectorV, f32 localV,
                             i32 sectorD, f32 localD, CoordFrame frame) {
    Vec3IntFloat *v = (Vec3IntFloat*) Memory_alloc(ID_VEC3, sizeof(Vec3IntFloat));
    if (!v) return nullptr;
    (*v).horizontal.sector = sectorH;
    (*v).horizontal.local = localH;
    (*v).vertical.sector = sectorV;
    (*v).vertical.local = localV;
    (*v).depth.sector = sectorD;
    (*v).depth.local = localD;
    (*v).frame = CoordFrame_isValid(frame) ? (u32) frame : (u32) COORD_FRAME_DEFAULT;
    return v;
}

void Vec3IntFloat_free(Vec3IntFloat *v) {
    if (v) Memory_free(v);
}

static void rebalance_axis(CoordIntFloat *axis, f32 sectorSize) {
    if (sectorSize <= 0.0f) return;
    while ((*axis).local >= sectorSize) {
        (*axis).local -= sectorSize;
        (*axis).sector += 1;
    }
    while ((*axis).local < 0.0f) {
        (*axis).local += sectorSize;
        (*axis).sector -= 1;
    }
}

void Vec3IntFloat_rebalance(Vec3IntFloat *v, f32 sectorSize) {
    if (!v) return;
    rebalance_axis(&(*v).horizontal, sectorSize);
    rebalance_axis(&(*v).vertical, sectorSize);
    rebalance_axis(&(*v).depth, sectorSize);
}

f32 Vec3IntFloat_getRightLocal(const Vec3IntFloat *v) { return v ? (*v).horizontal.local : 0.0f; }
f32 Vec3IntFloat_getLeftLocal(const Vec3IntFloat *v)  { return v ? -(*v).horizontal.local : 0.0f; }
f32 Vec3IntFloat_getUpLocal(const Vec3IntFloat *v)    { return v ? (*v).vertical.local : 0.0f; }
f32 Vec3IntFloat_getDownLocal(const Vec3IntFloat *v)  { return v ? -(*v).vertical.local : 0.0f; }
f32 Vec3IntFloat_getFrontLocal(const Vec3IntFloat *v) { return v ? (*v).depth.local : 0.0f; }
f32 Vec3IntFloat_getBackLocal(const Vec3IntFloat *v)  { return v ? -(*v).depth.local : 0.0f; }
