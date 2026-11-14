// reactive/reactive_object.c — the standard object reactives in one file.
// See reactive/reactive_object.h (the ;;INTENTION for the Single Class Per File
// waiver).

#include "vex.h"
#include "reactive/reactive_object.h"

#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: ReactiveObject (the standard object family)
 * ============================================================================
 * The standard object typed reactives (Vec2, Vec3, Vec4, Rectangle, Ld128), each stamped by TYPEDEF_REACTIVE / IMPLEMENT_REACTIVE.
 * The word holds the object pointer; a rebind is a change. They share this file
 * by intent (see the header).
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: ReactiveObject (reactive/reactive_object.c)
 * ============================================================================
 * the standard object-reactive family (five TYPEDEF_REACTIVE stamps).
 *
 * STRUCT FIELDS: none — the classes live in the header; this file holds the bodies.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Per class (emitted by the macro): _0/_1, _free, _set, _get, and the four typed
 * channels (addOn/removeOn Set/Changed/Get/Nullptr).
 * ============================================================================
 */

// The standard objects' valueOf(): render the FIELDS, not the pointer. A null
// value (the word is the object pointer) prints "null".
static fn vec2_str(const Vec2 *v, char *out, bytes cap) {
    if (v == nullptr) { snprintf(out, cap, "null"); return; }
    snprintf(out, cap, "(%.3g, %.3g)", (d64) (*v).x, (d64) (*v).y);
}

static fn vec3_str(const Vec3 *v, char *out, bytes cap) {
    if (v == nullptr) { snprintf(out, cap, "null"); return; }
    snprintf(out, cap, "(%.3g, %.3g, %.3g)", (d64) (*v).x, (d64) (*v).y, (d64) (*v).z);
}

static fn vec4_str(const Vec4 *v, char *out, bytes cap) {
    if (v == nullptr) { snprintf(out, cap, "null"); return; }
    snprintf(out, cap, "(%.3g, %.3g, %.3g, %.3g)",
             (d64) (*v).x, (d64) (*v).y, (d64) (*v).z, (d64) (*v).w);
}

static fn rectangle_str(const Rectangle *r, char *out, bytes cap) {
    if (r == nullptr) { snprintf(out, cap, "null"); return; }
    snprintf(out, cap, "[%.3g, %.3g, %.3g x %.3g]",
             (d64) (*r).x, (d64) (*r).y, (d64) (*r).width, (d64) (*r).height);
}

IMPLEMENT_REACTIVE_STR(Vec2, vec2_str);
IMPLEMENT_REACTIVE_STR(Vec3, vec3_str);
IMPLEMENT_REACTIVE_STR(Vec4, vec4_str);
IMPLEMENT_REACTIVE_STR(Rectangle, rectangle_str);
static fn ld128_str(const Ld128 *v, char *out, bytes cap) {
    if (v == nullptr) { snprintf(out, cap, "null"); return; }
    snprintf(out, cap, "(%.3g)", ld128_toD64(*v));
}

IMPLEMENT_REACTIVE_STR(Ld128, ld128_str);
