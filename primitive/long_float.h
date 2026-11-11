#ifndef PRIMITIVE_LONG_FLOAT_H
#define PRIMITIVE_LONG_FLOAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "re/nio/bit_pool.h"

#include "lang/constructor.h"

// primitive/long_f32.h — LongFloat primitive (Legacy: primitive/LongFloat.java).
// Delegates to Bit128 width pool (16B stride).

// LongFloat — the value: an int64 scalar plus a normalized f32 fraction in
// [-1, 1). 16 Bytes (int64 + aligned f32) — too big for one engine word.
typedef struct LongFloat {
    int64_t scalar;   // the integer part
    f32   decimal;  // the fractional part, in [-1, 1)
} LongFloat;

_Static_assert(sizeof(LongFloat) == 16, "LongFloat must be 16 Bytes");

extern BitPool g_long_f32Pool;

bool LongFloat_init(void);
void LongFloat_shutdown(void);
void *LongFloat_alloc(void);
void *LongFloat_allocArray(bytes count);
void LongFloat_free(void *ptr);
int64_t LongFloat_get(void *ptr);
void LongFloat_set(void *ptr, int64_t value);
bool LongFloat_compareAndSet(void *ptr, int64_t expected, int64_t value);
u64 LongFloat_type(void *ptr);
bytes LongFloat_length(void *ptr);


// Ergonomic constructors — LongFloat() and LongFloat(v1, v2)
void *LongFloat_allocWithValues(int64_t v1, f32 v2);
#define LongFloat_0(...) LongFloat_alloc()
#define LongFloat_2(v1, v2) LongFloat_allocWithValues(v1, v2)
#define LongFloat(...) CONSTRUCTOR_DISPATCH(LongFloat, __VA_ARGS__)


#define LongFloat_array(count) LongFloat_allocArray(count)

#endif
