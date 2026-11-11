#ifndef PRIMITIVE_LONG_f64_H
#define PRIMITIVE_LONG_f64_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "re/nio/bit_pool.h"

#include "lang/constructor.h"

// primitive/long_f64.h — Longf64 primitive (Legacy: primitive/Longf64.java).
// Delegates to Bit128 width pool (16B stride).

// Longf64 — the value: an int64 scalar plus a normalized d64 fraction in
// [-1, 1). 16 Bytes (int64 + aligned d64) — too big for one engine word.
typedef struct Longf64 {
    int64_t scalar;   // the integer part
    d64  decimal;  // the fractional part, in [-1, 1)
} Longf64;

_Static_assert(sizeof(Longf64) == 16, "Longf64 must be 16 Bytes");

extern BitPool g_long_f64Pool;

bool Longf64_init(void);
void Longf64_shutdown(void);
void *Longf64_alloc(void);
void *Longf64_allocArray(bytes count);
void Longf64_free(void *ptr);
int64_t Longf64_get(void *ptr);
void Longf64_set(void *ptr, int64_t value);
bool Longf64_compareAndSet(void *ptr, int64_t expected, int64_t value);
u64 Longf64_type(void *ptr);
bytes Longf64_length(void *ptr);


// Ergonomic constructors — Longf64() and Longf64(v1, v2)
void *Longf64_allocWithValues(int64_t v1, d64 v2);
#define Longf64_0(...) Longf64_alloc()
#define Longf64_2(v1, v2) Longf64_allocWithValues(v1, v2)
#define Longf64(...) CONSTRUCTOR_DISPATCH(Longf64, __VA_ARGS__)


#define Longf64_array(count) Longf64_allocArray(count)

#endif
