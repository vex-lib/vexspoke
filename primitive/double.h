#ifndef PRIMITIVE_f64_H
#define PRIMITIVE_f64_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "re/nio/bit_pool.h"

#include "lang/constructor.h"

// primitive/d64.h — d64 primitive (Legacy: primitive/d64.java).
// Delegates to Bit64 width pool (8B stride).

extern BitPool g_f64Pool;

bool f64_init(void);
void f64_shutdown(void);
void *f64_alloc(void);
void *f64_allocArray(bytes count);
void f64_free(void *ptr);
d64 f64_get(void *ptr);
void f64_set(void *ptr, d64 value);
bool f64_compareAndSet(void *ptr, d64 expected, d64 value);
u64 f64_type(void *ptr);
bytes f64_length(void *ptr);


// Ergonomic constructors — d64() and d64(value)
void *f64_allocWithValue(d64 value);
#define f64_0(...) f64_alloc()
#define f64_1(value) f64_allocWithValue(value)
#define d64(...) CONSTRUCTOR_DISPATCH(d64, __VA_ARGS__)


#define f64_array(count) f64_allocArray(count)

#endif
