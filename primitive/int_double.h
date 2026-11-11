#ifndef PRIMITIVE_INT_f64_H
#define PRIMITIVE_INT_f64_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "re/nio/bit_pool.h"

#include "lang/constructor.h"

// primitive/int_f64.h — Intf64 primitive (Legacy: primitive/Intf64.java).
// Delegates to Bit128 width pool (16B stride).

// Intf64 — the value: an int32 scalar plus a normalized d64 fraction in
// [-1, 1). 16 Bytes (int32 + aligned d64) — too big for one engine word.
typedef struct Intf64 {
    i32 scalar;   // the integer part
    d64  decimal;  // the fractional part, in [-1, 1)
} Intf64;

_Static_assert(sizeof(Intf64) == 16, "Intf64 must be 16 Bytes");

extern BitPool g_int_f64Pool;

bool Intf64_init(void);
void Intf64_shutdown(void);
void *Intf64_alloc(void);
void *Intf64_allocArray(bytes count);
void Intf64_free(void *ptr);
int64_t Intf64_get(void *ptr);
void Intf64_set(void *ptr, int64_t value);
bool Intf64_compareAndSet(void *ptr, int64_t expected, int64_t value);
u64 Intf64_type(void *ptr);
bytes Intf64_length(void *ptr);


// Ergonomic constructors — Intf64() and Intf64(v1, v2)
void *Intf64_allocWithValues(i32 v1, d64 v2);
#define Intf64_0(...) Intf64_alloc()
#define Intf64_2(v1, v2) Intf64_allocWithValues(v1, v2)
#define Intf64(...) CONSTRUCTOR_DISPATCH(Intf64, __VA_ARGS__)


#define Intf64_array(count) Intf64_allocArray(count)

#endif
