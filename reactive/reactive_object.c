// reactive/reactive_object.c — the standard object reactives in one file.
// See reactive/reactive_object.h (the ;;INTENTION for the Single Class Per File
// waiver).

#include "reactive/reactive_object.h"

#include <stdio.h>

#include "util/annotation.h"
#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: ReactiveObject (the standard object family)
 * ============================================================================
 * The standard object typed reactives (Coord2, Coord3, Coord4, Rectangle, Intf64,
 * LongFloat, Longf64), each stamped by TYPEDEF_REACTIVE / IMPLEMENT_REACTIVE.
 * The word holds the object pointer; a rebind is a change. They share this file
 * by intent (see the header).
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: ReactiveObject (reactive/reactive_object.c)
 * ============================================================================
 * the standard object-reactive family (seven TYPEDEF_REACTIVE stamps).
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
static void vec2_str(const Coord2 *v, char *out, bytes cap) {
    if (v == nullptr) { snprintf(out, cap, "null"); return; }
    snprintf(out, cap, "(%.3g, %.3g)", (d64) (*v).x, (d64) (*v).y);
}

static void vec3_str(const Coord3 *v, char *out, bytes cap) {
    if (v == nullptr) { snprintf(out, cap, "null"); return; }
    snprintf(out, cap, "(%.3g, %.3g, %.3g)", (d64) (*v).x, (d64) (*v).y, (d64) (*v).z);
}

static void vec4_str(const Coord4 *v, char *out, bytes cap) {
    if (v == nullptr) { snprintf(out, cap, "null"); return; }
    snprintf(out, cap, "(%.3g, %.3g, %.3g, %.3g)",
             (d64) (*v).x, (d64) (*v).y, (d64) (*v).z, (d64) (*v).w);
}

static void rectangle_str(const Rectangle *r, char *out, bytes cap) {
    if (r == nullptr) { snprintf(out, cap, "null"); return; }
    snprintf(out, cap, "[%.3g, %.3g, %.3g x %.3g]",
             (d64) (*r).x, (d64) (*r).y, (d64) (*r).width, (d64) (*r).height);
}

IMPLEMENT_REACTIVE_STR(Coord2, vec2_str);
IMPLEMENT_REACTIVE_STR(Coord3, vec3_str);
IMPLEMENT_REACTIVE_STR(Coord4, vec4_str);
IMPLEMENT_REACTIVE_STR(Rectangle, rectangle_str);
IMPLEMENT_REACTIVE(Intf64);
IMPLEMENT_REACTIVE(LongFloat);
IMPLEMENT_REACTIVE(Longf64);
