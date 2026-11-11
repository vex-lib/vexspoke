#ifndef PRIMITIVE_FIXED32_H
#define PRIMITIVE_FIXED32_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "re/nio/bit_pool.h"

#include "lang/constructor.h"

// primitive/fixed32.h — Fixed32 primitive (Legacy: primitive/Fixed32.java).
// Delegates to Bit32 width pool (4B stride).

extern BitPool g_fixed32Pool;

bool Fixed32_init(void);
void Fixed32_shutdown(void);
void *Fixed32_alloc(void);
void *Fixed32_allocArray(bytes count);
void Fixed32_free(void *ptr);
i32 Fixed32_get(void *ptr);
void Fixed32_set(void *ptr, i32 value);
bool Fixed32_compareAndSet(void *ptr, i32 expected, i32 value);
u64 Fixed32_type(void *ptr);
bytes Fixed32_length(void *ptr);


// Ergonomic constructors — Fixed32() and Fixed32(value)
void *Fixed32_allocWithValue(i32 value);
#define Fixed32_0(...) Fixed32_alloc()
#define Fixed32_1(value) Fixed32_allocWithValue(value)
#define Fixed32(...) CONSTRUCTOR_DISPATCH(Fixed32, __VA_ARGS__)


#define Fixed32_array(count) Fixed32_allocArray(count)

#endif
