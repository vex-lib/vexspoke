// reactive/reactive_primitive.c — the whole scalar reactive family in one file.
// See reactive/reactive_primitive.h (the ;;INTENTION for the Single Class Per File
// waiver).

#include "reactive/reactive_primitive.h"

#include <stdio.h>
#include <string.h>

#include "util/annotation.h"
#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: ReactivePrimitive (the scalar family)
 * ============================================================================
 * The ten scalar typed reactives (Bool, Byte, Short, Char, Int, Long, Float,
 * d64, String, IntFloat) stamped from reactive/reactive_tmpl.inc. Each embeds
 * the one engine and differs ONLY in how it packs its value into the word: an
 * integer cast, a bit-cast (f32/d64/IntFloat), or a pointer (String). They
 * share this file by intent (see the header), since they are mechanical stamps.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: ReactivePrimitive (reactive/reactive_primitive.c)
 * ============================================================================
 * the scalar typed-reactive family (ten template instantiations) + the bit-cast
 * helpers.
 *
 * STRUCT FIELDS: none — the classes live in the header; this file holds the
 * bodies and the static bit-cast pairs (f32Bits/f32Of, bitsOf/f64Of,
 * intFloatBits/intFloatOf/intFloatZero).
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Per class (emitted by the template): _0/_1, _free, _set, _get, and the four
 * typed channels (addOn/removeOn Set/Changed/Get/Nullptr).
 * ============================================================================
 */

// --- bit-cast helpers (shared by the f32/d64/IntFloat packings) ---

static pointer f32Bits(f32 value) {
    u32 bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return (pointer) bits;
}

static f32 f32Of(pointer word) {
    u32 bits = (u32) word;
    f32 value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static pointer bitsOf(d64 value) {
    u64 bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return (pointer) bits;
}

static d64 f64Of(pointer word) {
    u64 bits = (u64) word;
    d64 value = 0.0;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static pointer intFloatBits(IntFloat value) {
    u64 bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return (pointer) bits;
}

static IntFloat intFloatOf(pointer word) {
    u64 bits = (u64) word;
    IntFloat value = { 0, 0.0f };
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static IntFloat intFloatZero(void) {
    IntFloat zero = { 0, 0.0f };
    return zero;
}

// --- the ten instantiations ---

#define VEX_NAME ReactiveBool
#define VEX_T    bool
#define VEX_TYPE_ID TYPE_REACTIVE_BOOL
#define VEX_TO_WORD(v)   ((pointer) ((v) ? 1u : 0u))
#define VEX_FROM_WORD(w) ((w) != 0u)
#define VEX_ZERO       false
#define VEX_VALUE_OF(out, cap, v) snprintf((out), (cap), "%s", (v) ? "true" : "false")
#include "reactive/reactive_tmpl.inc"
#undef VEX_NAME
#undef VEX_T
#undef VEX_TYPE_ID
#undef VEX_TO_WORD
#undef VEX_FROM_WORD
#undef VEX_ZERO
#undef VEX_VALUE_OF

#define VEX_NAME ReactiveByte
#define VEX_T
#define VEX_TYPE_ID TYPE_REACTIVE_BYTE
#define VEX_TO_WORD(v)   ((pointer) (u8) (v))
#define VEX_FROM_WORD(w) (() (u8) (w))
#define VEX_ZERO       (() 0)
#define VEX_VALUE_OF(out, cap, v) snprintf((out), (cap), "%d", (int) (v))
#include "reactive/reactive_tmpl.inc"
#undef VEX_NAME
#undef VEX_T
#undef VEX_TYPE_ID
#undef VEX_TO_WORD
#undef VEX_FROM_WORD
#undef VEX_ZERO
#undef VEX_VALUE_OF

#define VEX_NAME ReactiveShort
#define VEX_T
#define VEX_TYPE_ID TYPE_REACTIVE_SHORT
#define VEX_TO_WORD(v)   ((pointer) (u16) (v))
#define VEX_FROM_WORD(w) (() (u16) (w))
#define VEX_ZERO       (() 0)
#define VEX_VALUE_OF(out, cap, v) snprintf((out), (cap), "%d", (int) (v))
#include "reactive/reactive_tmpl.inc"
#undef VEX_NAME
#undef VEX_T
#undef VEX_TYPE_ID
#undef VEX_TO_WORD
#undef VEX_FROM_WORD
#undef VEX_ZERO
#undef VEX_VALUE_OF

#define VEX_NAME ReactiveChar
#define VEX_T    u16
#define VEX_TYPE_ID TYPE_REACTIVE_CHAR
#define VEX_TO_WORD(v)   ((pointer) (u16) (v))
#define VEX_FROM_WORD(w) ((u16) (w))
#define VEX_ZERO       ((u16) 0)
#define VEX_VALUE_OF(out, cap, v) snprintf((out), (cap), "%u", (unsigned) (v))
#include "reactive/reactive_tmpl.inc"
#undef VEX_NAME
#undef VEX_T
#undef VEX_TYPE_ID
#undef VEX_TO_WORD
#undef VEX_FROM_WORD
#undef VEX_ZERO
#undef VEX_VALUE_OF

#define VEX_NAME ReactiveInt
#define VEX_T    i32
#define VEX_TYPE_ID TYPE_REACTIVE_INT
#define VEX_TO_WORD(v)   ((pointer) (u32) (v))
#define VEX_FROM_WORD(w) ((i32) (u32) (w))
#define VEX_ZERO       ((i32) 0)
#define VEX_VALUE_OF(out, cap, v) snprintf((out), (cap), "%d", (int) (v))
#include "reactive/reactive_tmpl.inc"
#undef VEX_NAME
#undef VEX_T
#undef VEX_TYPE_ID
#undef VEX_TO_WORD
#undef VEX_FROM_WORD
#undef VEX_ZERO
#undef VEX_VALUE_OF

#define VEX_NAME ReactiveLong
#define VEX_T    int64_t
#define VEX_TYPE_ID TYPE_REACTIVE_LONG
#define VEX_TO_WORD(v)   ((pointer) (v))
#define VEX_FROM_WORD(w) ((int64_t) (w))
#define VEX_ZERO       ((int64_t) 0)
#define VEX_VALUE_OF(out, cap, v) snprintf((out), (cap), "%lld", (long long) (v))
#include "reactive/reactive_tmpl.inc"
#undef VEX_NAME
#undef VEX_T
#undef VEX_TYPE_ID
#undef VEX_TO_WORD
#undef VEX_FROM_WORD
#undef VEX_ZERO
#undef VEX_VALUE_OF

#define VEX_NAME ReactiveFloat
#define VEX_T    f32
#define VEX_TYPE_ID TYPE_REACTIVE_FLOAT
#define VEX_TO_WORD(v)   f32Bits(v)
#define VEX_FROM_WORD(w) f32Of(w)
#define VEX_ZERO       0.0f
#define VEX_VALUE_OF(out, cap, v) snprintf((out), (cap), "%g", (d64) (v))
#include "reactive/reactive_tmpl.inc"
#undef VEX_NAME
#undef VEX_T
#undef VEX_TYPE_ID
#undef VEX_TO_WORD
#undef VEX_FROM_WORD
#undef VEX_ZERO
#undef VEX_VALUE_OF

#define VEX_NAME Reactivef64
#define VEX_T    d64
#define VEX_TYPE_ID TYPE_REACTIVE_f64
#define VEX_TO_WORD(v)   bitsOf(v)
#define VEX_FROM_WORD(w) f64Of(w)
#define VEX_ZERO       0.0
#define VEX_VALUE_OF(out, cap, v) snprintf((out), (cap), "%g", (v))
#include "reactive/reactive_tmpl.inc"
#undef VEX_NAME
#undef VEX_T
#undef VEX_TYPE_ID
#undef VEX_TO_WORD
#undef VEX_FROM_WORD
#undef VEX_ZERO
#undef VEX_VALUE_OF

#define VEX_NAME ReactiveString
#define VEX_T    const u8 *
#define VEX_TYPE_ID TYPE_REACTIVE_STRING
#define VEX_TO_WORD(v)   ((pointer) (v))
#define VEX_FROM_WORD(w) ((const u8*) (w))
#define VEX_ZERO       nullptr
#define VEX_VALUE_OF(out, cap, v) snprintf((out), (cap), "%s", (v) ? (const char*) (v) : "")
#include "reactive/reactive_tmpl.inc"
#undef VEX_NAME
#undef VEX_T
#undef VEX_TYPE_ID
#undef VEX_TO_WORD
#undef VEX_FROM_WORD
#undef VEX_ZERO
#undef VEX_VALUE_OF

#define VEX_NAME ReactiveIntFloat
#define VEX_T    IntFloat
#define VEX_TYPE_ID TYPE_REACTIVE_INT_FLOAT
#define VEX_TO_WORD(v)   intFloatBits(v)
#define VEX_FROM_WORD(w) intFloatOf(w)
#define VEX_ZERO       intFloatZero()
#define VEX_VALUE_OF(out, cap, v) snprintf((out), (cap), "%g", (d64) (v).scalar + (d64) (v).decimal)
#include "reactive/reactive_tmpl.inc"
#undef VEX_NAME
#undef VEX_T
#undef VEX_TYPE_ID
#undef VEX_TO_WORD
#undef VEX_FROM_WORD
#undef VEX_ZERO
#undef VEX_VALUE_OF
